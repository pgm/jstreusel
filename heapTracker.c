/*
 * @(#)heapTracker.c	1.21 06/02/16
 * 
 * Copyright (c) 2006 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * -Redistribution of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 * 
 * -Redistribution in binary form must reproduce the above copyright notice, 
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of contributors may 
 * be used to endorse or promote products derived from this software without 
 * specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind. ALL 
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING
 * ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN MIDROSYSTEMS, INC. ("SUN")
 * AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE
 * AS A RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS
 * DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST 
 * REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, 
 * INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY 
 * OF LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, 
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * You acknowledge that this software is not designed, licensed or intended
 * for use in the design, construction, operation or maintenance of any
 * nuclear facility.
 */

#include "stdlib.h"

#include "heapTracker.h"
#include "java_crw_demo.h"

#include "jni.h"
#include "jvmti.h"

#include "agent_util.h"


/* ------------------------------------------------------------------- 
 * Some constant names that tie to Java class/method names.
 *    We assume the Java class whose static methods we will be calling
 *    looks like:
 *
 * public class HeapTracker {
 *     private static int engaged; 
 *     private static native void _newobj(Object thr, Object o);
 *     public static void newobj(Object o)
 *     {
 *              if ( engaged != 0 ) {
 *               _newobj(Thread.currentThread(), o);
 *           }
 *     }
 *     private static native void _newarr(Object thr, Object a);
 *     public static void newarr(Object a)
 *     {
 *            if ( engaged != 0 ) {
 *               _newarr(Thread.currentThread(), a);
 *           }
 *     }
 * }
 *
 *    The engaged field allows us to inject all classes (even system classes)
 *    and delay the actual calls to the native code until the VM has reached
 *    a safe time to call native methods (Past the JVMTI VM_START event).
 * 
 */

#define HEAP_TRACKER_class           "com/github/jstreusel/HeapTrackerJNISupport" /* Name of class we are using */
#define HEAP_TRACKER_newobj        newobj   /* Name of java init method */
#define HEAP_TRACKER_newarr        newarr   /* Name of java newarray method */
#define HEAP_TRACKER_native_newobj _newobj  /* Name of java newobj native */
#define HEAP_TRACKER_native_newarr _newarr  /* Name of java newarray native */
#define HEAP_TRACKER_engaged       engaged  /* Name of static field switch */
#define HEAP_TRACKER_native_start_request _start_request /* Name of java start_request native */
#define HEAP_TRACKER_native_extract_stats _extract_stats /* Name of java start_request native */

/* C macros to create strings from tokens */
#define _STRING(s) #s
#define STRING(s) _STRING(s)

/* ------------------------------------------------------------------- */

/* Global agent data structure */

typedef struct {
    /* JVMTI Environment */
    jvmtiEnv      *jvmti;

    /* State of the VM flags */
    jboolean       vmStarted;
    jboolean       vmInitialized;
    jboolean       vmDead;

    /* Data access Lock */
    jrawMonitorID  lock;

    /* Counter on classes where BCI has been applied */
    jint           ccount;
} GlobalAgentData;

static GlobalAgentData *gdata;

typedef struct RequestStats
{
  int requestId;
  int objectCount;
  int memoryUsed;
  int arrayCount;
  struct RequestStats *next;
} RequestStats;

#define STATS_BUCKET_COUNT 1024*8

static RequestStats **collectStats(jvmtiEnv *jvmti, JNIEnv *env);
static void freeStats(RequestStats **stats);

/* Enter a critical section by doing a JVMTI Raw Monitor Enter */
static void
enterCriticalSection(jvmtiEnv *jvmti)
{
    jvmtiError error;
    
    error = (*jvmti)->RawMonitorEnter(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot enter with raw monitor");
}

/* Exit a critical section by doing a JVMTI Raw Monitor Exit */
static void
exitCriticalSection(jvmtiEnv *jvmti)
{
    jvmtiError error;
    
    error = (*jvmti)->RawMonitorExit(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");
}

/* Tag an object with a TraceInfo pointer. */
static void
tagObject(jvmtiEnv *jvmti, jobject object, jthread thread)
{
    jvmtiError error;
    jlong      tag;
    void *threadData;

/*    
    error = (*jvmti)->GetThreadLocalStorage(jvmti, thread, (void**)&threadData);
    check_jvmti_error(jvmti, error, "Cannot get thread local storage");
    
    if( threadData != NULL ) {
    */
      /* Tag this object with this TraceInfo pointer */
//      tag = threadData->requestId;

      error = (*jvmti)->GetThreadLocalStorage(jvmti, thread, (void**)&threadData);
      check_jvmti_error(jvmti, error, "Cannot get thread local storage");
      
      tag = (jlong)threadData;

      error = (*jvmti)->SetTag(jvmti, object, tag);
      /*
    }
    */
}

/* Java Native Method for Object.<init> */
static void
HEAP_TRACKER_native_newobj(JNIEnv *env, jclass klass, jthread thread, jobject o)
{
    
    if ( gdata->vmDead ) {
        return;
    }

    tagObject(gdata->jvmti, o, thread);
}

/* Java Native Method for newarray */
static void
HEAP_TRACKER_native_newarr(JNIEnv *env, jclass klass, jthread thread, jobject a)
{
    
    if ( gdata->vmDead ) {
        return;
    }

    tagObject(gdata->jvmti, a, thread);
}

static void HEAP_TRACKER_native_extract_stats(JNIEnv *env, jclass klass, jclass statsClass, jobject list)
{
  RequestStats ** stats_buckets;
  jmethodID add_methodID;  
  jmethodID constructor_methodID;
  jclass listClass;
  jobject newStatsObject;
  int i;
  jvmtiEnv *jvmti = gdata->jvmti;
  jvmtiError error;

  /* Force garbage collection now so we get our ObjectFree calls */
  error = (*jvmti)->ForceGarbageCollection(jvmti);
  check_jvmti_error(jvmti, error, "Cannot force garbage collection");

  enterCriticalSection(jvmti); {
      stats_buckets = collectStats(jvmti, env);
  } exitCriticalSection(jvmti);

  listClass = (*env)->GetObjectClass(env, list);
  if(listClass == NULL)
    return;
    
  constructor_methodID = (*env)->GetMethodID(env, statsClass, "<init>", "(IIII)V");
  if(constructor_methodID == NULL)
    return;
  
  add_methodID = (*env)->GetMethodID(env, listClass, "add", "(Ljava/lang/Object;)Z");
  if(add_methodID == NULL)
    return;

    /* walk through collected stats objects */    
    for(i=0;i<STATS_BUCKET_COUNT;i++) 
    {
      RequestStats *stats;
      
      stats = stats_buckets[i];
      while(stats != NULL) {
        // construct new stats object
        newStatsObject = (*env)->NewObject(env, statsClass, constructor_methodID, stats->requestId, stats->objectCount, stats->memoryUsed, stats->arrayCount);
        
        // append it to list
        (*env)->CallVoidMethod(env, list, add_methodID, newStatsObject);

        stats = stats->next;
      }
    }

    freeStats(stats_buckets);
}

static void HEAP_TRACKER_native_start_request(JNIEnv *env, jclass klass, jthread thread, jint requestId)
{
    jvmtiError error;
    jvmtiEnv *jvmti = gdata->jvmti;

    error = (*jvmti)->SetThreadLocalStorage(jvmti, thread, (void*)requestId);
    check_jvmti_error(jvmti, error, "Cannot set thread local storage");
}

/* Callback for JVMTI_EVENT_VM_START */
static void JNICALL
cbVMStart(jvmtiEnv *jvmti, JNIEnv *env)
{
    enterCriticalSection(jvmti); {
        jclass klass;
        jfieldID field;
        jint rc;

        /* Java Native Methods for class */
        static JNINativeMethod registry[4] = {
            {STRING(HEAP_TRACKER_native_newobj), "(Ljava/lang/Object;Ljava/lang/Object;)V", 
                (void*)&HEAP_TRACKER_native_newobj},
            {STRING(HEAP_TRACKER_native_newarr), "(Ljava/lang/Object;Ljava/lang/Object;)V", 
                (void*)&HEAP_TRACKER_native_newarr},
            {STRING(HEAP_TRACKER_native_start_request), "(Ljava/lang/Object;I)V", 
                (void*)&HEAP_TRACKER_native_start_request},
            {STRING(HEAP_TRACKER_native_extract_stats), "(Ljava/lang/Object;Ljava/lang/Object;)V",
                (void*)&HEAP_TRACKER_native_extract_stats}
        };
        
        /* Register Natives for class whose methods we use */
        klass = (*env)->FindClass(env, HEAP_TRACKER_class);
        if ( klass == NULL ) {
            fatal_error("ERROR: JNI: Cannot find %s with FindClass\n", 
                        HEAP_TRACKER_class);
        }
        rc = (*env)->RegisterNatives(env, klass, registry, 4);
        if ( rc != 0 ) {
            fatal_error("ERROR: JNI: Cannot register natives for class %s\n", 
                        HEAP_TRACKER_class);
        }
        
        /* Engage calls. */
        field = (*env)->GetStaticFieldID(env, klass, STRING(HEAP_TRACKER_engaged), "I");
        if ( field == NULL ) {
            fatal_error("ERROR: JNI: Cannot get field from %s\n", 
                        HEAP_TRACKER_class);
        }
        (*env)->SetStaticIntField(env, klass, field, 1);

        /* Indicate VM has started */
        gdata->vmStarted = JNI_TRUE;
    
    } exitCriticalSection(jvmti);
  
  fflush(stdout);
}

/* Iterate Through Heap callback */
static jint JNICALL
cbObjectTagger(jlong class_tag, jlong size, jlong* tag_ptr, jint length, 
               void *user_data)
{
    return JVMTI_VISIT_OBJECTS;
}

/* Callback for JVMTI_EVENT_VM_INIT */
static void JNICALL
cbVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
    jvmtiHeapCallbacks heapCallbacks;
    jvmtiError         error;
    
    /* Iterate through heap, find all untagged objects allocated before this */
    (void)memset(&heapCallbacks, 0, sizeof(heapCallbacks));
    heapCallbacks.heap_iteration_callback = &cbObjectTagger;
    error = (*jvmti)->IterateThroughHeap(jvmti, JVMTI_HEAP_FILTER_TAGGED, 
                                         NULL, &heapCallbacks, NULL);
    check_jvmti_error(jvmti, error, "Cannot iterate through heap");

    enterCriticalSection(jvmti); {
        
        /* Indicate VM is initialized */
        gdata->vmInitialized = JNI_TRUE;
    
    } exitCriticalSection(jvmti);
}


RequestStats *findOrCreateRequestStats(RequestStats **buckets, int needle)
{
  RequestStats *stats;
  int hash;

  hash = needle % STATS_BUCKET_COUNT;
  
  stats = buckets[hash];
  while(stats != NULL)
  {
    if(stats->requestId == needle) 
    {
      return stats;
    }
 
    stats = stats->next;
  }
  
  // if we reach here, allocate a new stats object
  stats = (RequestStats*)calloc(1, sizeof(RequestStats));
  stats->requestId = needle;
  
  // and append it to the chain in the appropriate bucket
  stats->next = buckets[hash];
  buckets[hash] = stats;
  
  return stats;
}

/* Iterate Through Heap callback */
static jint JNICALL
cbObjectSpaceCounter(jlong class_tag, jlong size, jlong* tag_ptr, jint length,
                     void *user_data)
{
  RequestStats **buckets = (RequestStats **)user_data;
  
  jlong tag = *tag_ptr;
  if(tag != 0) {
    RequestStats *stats = findOrCreateRequestStats(buckets, tag);
    
    // accum memory used by this request
    stats->memoryUsed+=size;

    // is it an array?
    if(length >= 0) 
    {
      stats->arrayCount ++;
    } 
    else
    {
      stats->objectCount ++;
    }
  }

  return JVMTI_VISIT_OBJECTS;
}

static RequestStats **collectStats(jvmtiEnv *jvmti, JNIEnv *env)
{
    jvmtiHeapCallbacks heapCallbacks;
    jvmtiEventCallbacks callbacks;
    RequestStats **stats_buckets;
    jvmtiError error;

    stats_buckets  = (RequestStats **)calloc(STATS_BUCKET_COUNT, sizeof( RequestStats * )); 
    
    /* Iterate through heap and find all objects */
    (void)memset(&heapCallbacks, 0, sizeof(heapCallbacks));
    heapCallbacks.heap_iteration_callback = &cbObjectSpaceCounter;
    error = (*jvmti)->IterateThroughHeap(jvmti, 0, NULL, &heapCallbacks, stats_buckets);
    check_jvmti_error(jvmti, error, "Cannot iterate through heap");
    
    return stats_buckets;
}


static void freeStats(RequestStats **stats_buckets)
{
  int i;
  
    /* walk through collected stats objects */    
    for(i=0;i<STATS_BUCKET_COUNT;i++) 
    {
      RequestStats *stats;
      
      stats = stats_buckets[i];
      while(stats != NULL) {
        free(stats);
        stats = stats->next;
      }
    }
    
    free(stats_buckets);
}


/* Callback for JVMTI_EVENT_VM_DEATH */
static void JNICALL
cbVMDeath(jvmtiEnv *jvmti, JNIEnv *env)
{
    jvmtiHeapCallbacks heapCallbacks;
    jvmtiError         error;

    /* These are purposely done outside the critical section */

    /* Force garbage collection now so we get our ObjectFree calls */
    error = (*jvmti)->ForceGarbageCollection(jvmti);
    check_jvmti_error(jvmti, error, "Cannot force garbage collection");
    

    /* Process VM Death */
    enterCriticalSection(jvmti); {
        jclass              klass;
        jfieldID            field;
        jvmtiEventCallbacks callbacks;

        /* Disengage calls in HEAP_TRACKER_class. */
        klass = (*env)->FindClass(env, HEAP_TRACKER_class);
        if ( klass == NULL ) {
            fatal_error("ERROR: JNI: Cannot find %s with FindClass\n", 
                        HEAP_TRACKER_class);
        }
        field = (*env)->GetStaticFieldID(env, klass, STRING(HEAP_TRACKER_engaged), "I");
        if ( field == NULL ) {
            fatal_error("ERROR: JNI: Cannot get field from %s\n", 
                        HEAP_TRACKER_class);
        }
        (*env)->SetStaticIntField(env, klass, field, 0);

        /* The critical section here is important to hold back the VM death
         *    until all other callbacks have completed.
         */

        /* Clear out all callbacks. */
        (void)memset(&callbacks,0, sizeof(callbacks));
        error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, 
                                            (jint)sizeof(callbacks));
        check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");

        /* Since this critical section could be holding up other threads
         *   in other event callbacks, we need to indicate that the VM is
         *   dead so that the other callbacks can short circuit their work.
         *   We don't expect an further events after VmDeath but we do need
         *   to be careful that existing threads might be in our own agent
         *   callback code.
         */
        gdata->vmDead = JNI_TRUE;

    } exitCriticalSection(jvmti);
}  

/* Callback for JVMTI_EVENT_VM_OBJECT_ALLOC */
static void JNICALL
cbVMObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread, 
                jobject object, jclass object_klass, jlong size)
{
    if ( gdata->vmDead ) {
        return;
    }
    
    tagObject(jvmti, object, thread);
}

/* Callback for JVMTI_EVENT_OBJECT_FREE */
static void JNICALL
cbObjectFree(jvmtiEnv *jvmti, jlong tag)
{
}

/* Callback for JVMTI_EVENT_CLASS_FILE_LOAD_HOOK */
static void JNICALL
cbClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv* env,
                jclass class_being_redefined, jobject loader,
                const char* name, jobject protection_domain,
                jint class_data_len, const unsigned char* class_data,
                jint* new_class_data_len, unsigned char** new_class_data)
{
    enterCriticalSection(jvmti); {
        /* It's possible we get here right after VmDeath event, be careful */
        if ( !gdata->vmDead ) {

            const char * classname;

            /* Name can be NULL, make sure we avoid SEGV's */
            if ( name == NULL ) {
                classname = java_crw_demo_classname(class_data, class_data_len,
                                NULL);
                if ( classname == NULL ) {
                    fatal_error("ERROR: No classname in classfile\n");
                }
            } else {
                classname = strdup(name);
                if ( classname == NULL ) {
                    fatal_error("ERROR: Ran out of malloc() space\n");
                }
            }

            *new_class_data_len = 0;
            *new_class_data     = NULL;

            /* The tracker class itself? */
            if ( strcmp(classname, HEAP_TRACKER_class) != 0 ) {
                jint           cnum;
                int            systemClass;
                unsigned char *newImage;
                long           newLength;

                /* Get number for every class file image loaded */
                cnum = gdata->ccount++;

                /* Is it a system class? If the class load is before VmStart
                 *   then we will consider it a system class that should
                 *   be treated carefully. (See java_crw_demo)
                 */
                systemClass = 0;
                if ( !gdata->vmStarted ) {
                    systemClass = 1;
                }

                newImage = NULL;
                newLength = 0;

                /* Call the class file reader/write demo code */
                java_crw_demo(cnum,
                    classname,
                    class_data,
                    class_data_len,
                    systemClass,
                    HEAP_TRACKER_class,
                    "L" HEAP_TRACKER_class ";",
                    NULL, NULL,
                    NULL, NULL,
                    STRING(HEAP_TRACKER_newobj), "(Ljava/lang/Object;)V",
                    STRING(HEAP_TRACKER_newarr), "(Ljava/lang/Object;)V",
                    &newImage,
                    &newLength,
                    NULL,
                    NULL);

                /* If we got back a new class image, return it back as "the"
                 *   new class image. This must be JVMTI Allocate space.
                 */
                if ( newLength > 0 ) {
                    unsigned char *jvmti_space;

                    jvmti_space = (unsigned char *)allocate(jvmti, (jint)newLength);
                    (void)memcpy((void*)jvmti_space, (void*)newImage, (int)newLength);
                    *new_class_data_len = (jint)newLength;
                    *new_class_data     = jvmti_space; /* VM will deallocate */
                }

                /* Always free up the space we get from java_crw_demo() */
                if ( newImage != NULL ) {
                    (void)free((void*)newImage); /* Free malloc() space with free() */
                }
            }
        
            (void)free((void*)classname);
        }
    } exitCriticalSection(jvmti);
}

/* Agent_OnLoad: This is called immediately after the shared library is 
 *   loaded. This is the first code executed.
 */
JNIEXPORT jint JNICALL 
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
    static GlobalAgentData data;
    jvmtiEnv              *jvmti;
    jvmtiError             error;
    jint                   res;
    jvmtiCapabilities      capabilities;
    jvmtiEventCallbacks    callbacks;

    /* Setup initial global agent data area 
     *   Use of static/extern data should be handled carefully here.
     *   We need to make sure that we are able to cleanup after ourselves
     *     so anything allocated in this library needs to be freed in
     *     the Agent_OnUnload() function.
     */
    (void)memset((void*)&data, 0, sizeof(data));
    gdata = &data;
   
    /* First thing we need to do is get the jvmtiEnv* or JVMTI environment */
    res = (*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1);
    if (res != JNI_OK) {
        /* This means that the VM was unable to obtain this version of the
         *   JVMTI interface, this is a fatal error.
         */
        fatal_error("ERROR: Unable to access JVMTI Version 1 (0x%x),"
                " is your JDK a 5.0 or newer version?"
                " JNIEnv's GetEnv() returned %d\n",
               JVMTI_VERSION_1, res);
    }

    /* Here we save the jvmtiEnv* for Agent_OnUnload(). */
    gdata->jvmti = jvmti;
   
    /* Immediately after getting the jvmtiEnv* we need to ask for the
     *   capabilities this agent will need. 
     */
    (void)memset(&capabilities,0, sizeof(capabilities));
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_tag_objects  = 1;
    capabilities.can_generate_object_free_events  = 1;
    capabilities.can_get_source_file_name  = 1;
    capabilities.can_get_line_numbers  = 1;
    capabilities.can_generate_vm_object_alloc_events  = 1;
    error = (*jvmti)->AddCapabilities(jvmti, &capabilities);
    check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");

    /* Next we need to provide the pointers to the callback functions to
     *   to this jvmtiEnv*
     */
    (void)memset(&callbacks,0, sizeof(callbacks));
    /* JVMTI_EVENT_VM_START */
    callbacks.VMStart           = &cbVMStart;      
    /* JVMTI_EVENT_VM_INIT */
    callbacks.VMInit            = &cbVMInit;      
    /* JVMTI_EVENT_VM_DEATH */
    callbacks.VMDeath           = &cbVMDeath;     
    /* JVMTI_EVENT_OBJECT_FREE */
    callbacks.ObjectFree        = &cbObjectFree;     
    /* JVMTI_EVENT_VM_OBJECT_ALLOC */
    callbacks.VMObjectAlloc     = &cbVMObjectAlloc;     
    /* JVMTI_EVENT_CLASS_FILE_LOAD_HOOK */
    callbacks.ClassFileLoadHook = &cbClassFileLoadHook; 
    error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));
    check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");
   
    /* At first the only initial events we are interested in are VM
     *   initialization, VM death, and Class File Loads. 
     *   Once the VM is initialized we will request more events.
     */

    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                          JVMTI_EVENT_VM_START, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                          JVMTI_EVENT_VM_INIT, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                          JVMTI_EVENT_VM_DEATH, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                          JVMTI_EVENT_OBJECT_FREE, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                          JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 
                          JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
   
    /* Here we create a raw monitor for our use in this agent to
     *   protect critical sections of code.
     */
    error = (*jvmti)->CreateRawMonitor(jvmti, "agent data", &(gdata->lock));
    check_jvmti_error(jvmti, error, "Cannot create raw monitor");

    /* Add jar file to boot classpath */
/*    add_demo_jar_to_bootclasspath(jvmti, "heapTracker"); */

    /* We return JNI_OK to signify success */
    return JNI_OK;
}

/* Agent_OnUnload: This is called immediately before the shared library is 
 *   unloaded. This is the last code executed.
 */
JNIEXPORT void JNICALL 
Agent_OnUnload(JavaVM *vm)
{
    /* Skip any cleanup, VM is about to die anyway */
}

