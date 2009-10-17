#
# @(#)sample.makefile.txt	1.7 05/11/17
# 
# Copyright (c) 2006 Sun Microsystems, Inc. All Rights Reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# -Redistribution of source code must retain the above copyright notice, this
#  list of conditions and the following disclaimer.
# 
# -Redistribution in binary form must reproduce the above copyright notice, 
#  this list of conditions and the following disclaimer in the documentation
#  and/or other materials provided with the distribution.
# 
# Neither the name of Sun Microsystems, Inc. or the names of contributors may 
# be used to endorse or promote products derived from this software without 
# specific prior written permission.
# 
# This software is provided "AS IS," without a warranty of any kind. ALL 
# EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING
# ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
# OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN MIDROSYSTEMS, INC. ("SUN")
# AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE
# AS A RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS
# DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST 
# REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, 
# INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY 
# OF LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, 
# EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
# 
# You acknowledge that this software is not designed, licensed or intended
# for use in the design, construction, operation or maintenance of any
# nuclear facility.
#

########################################################################
#
# Sample GNU Makefile for building JVMTI Demo heapTracker
#
#  Example uses:    
#       gnumake JDK=<java_home> OSNAME=solaris [OPT=true] [LIBARCH=sparc]
#       gnumake JDK=<java_home> OSNAME=solaris [OPT=true] [LIBARCH=sparcv9]
#       gnumake JDK=<java_home> OSNAME=linux   [OPT=true]
#       gnumake JDK=<java_home> OSNAME=win32   [OPT=true]
#
########################################################################

# Source lists
LIBNAME=heapTracker
SOURCES=heapTracker.c ../agent_util/agent_util.c
JAVA_SOURCES=HeapTracker.java TestHeapTracking.java

# Name of jar file that needs to be created
JARFILE=heapTracker.jar

# Solaris Sun C Compiler Version 5.5
ifeq ($(OSNAME), solaris)
    # Sun Solaris Compiler options needed
    COMMON_FLAGS=-mt -KPIC
    # Options that help find errors
    COMMON_FLAGS+= -Xa -v -xstrconst -xc99=%none
    # Check LIBARCH for any special compiler options
    LIBARCH=$(shell uname -p)
    ifeq ($(LIBARCH), sparc)
        COMMON_FLAGS+=-xarch=v8 -xregs=no%appl
    endif
    ifeq ($(LIBARCH), sparcv9)
        COMMON_FLAGS+=-xarch=v9 -xregs=no%appl
    endif
    ifeq ($(OPT), true)
        CFLAGS=-xO2 $(COMMON_FLAGS) 
    else
        CFLAGS=-g $(COMMON_FLAGS)
    endif
    # Object files needed to create library
    OBJECTS=$(SOURCES:%.c=%.o)
    # Library name and options needed to build it
    LIBRARY=lib$(LIBNAME).so
    LDFLAGS=-z defs -ztext
    # Libraries we are dependent on
    LIBRARIES=-L $(JDK)/jre/lib/$(LIBARCH) -ljava_crw_demo -lc
    # Building a shared library
    LINK_SHARED=$(LINK.c) -G -o $@
endif

# Linux GNU C Compiler
ifeq ($(OSNAME), linux)
    # GNU Compiler options needed to build it
    COMMON_FLAGS=-fno-strict-aliasing -fPIC -fno-omit-frame-pointer
    # Options that help find errors
    COMMON_FLAGS+= -W -Wall  -Wno-unused -Wno-parentheses
    ifeq ($(OPT), true)
        CFLAGS=-O2 $(COMMON_FLAGS) 
    else
        CFLAGS=-g $(COMMON_FLAGS) 
    endif
    # Object files needed to create library
    OBJECTS=$(SOURCES:%.c=%.o)
    # Library name and options needed to build it
    LIBRARY=lib$(LIBNAME).so
    LDFLAGS=-Wl,-soname=$(LIBRARY) -static-libgcc -mimpure-text
    # Libraries we are dependent on
    LIBRARIES=-L $(JDK)/jre/lib/$(LIBARCH) -L../java_crw_demo -ljava_crw_demo -lc
    # Building a shared library
    LINK_SHARED=$(LINK.c) -shared -o $@
endif

# Windows Microsoft C/C++ Optimizing Compiler Version 12
ifeq ($(OSNAME), win32)
    CC=cl
    # Compiler options needed to build it
    COMMON_FLAGS=-Gy -DWIN32
    # Options that help find errors
    COMMON_FLAGS+=-W0 -WX
    ifeq ($(OPT), true)
        CFLAGS= -Ox -Op -Zi $(COMMON_FLAGS) 
    else
        CFLAGS= -Od -Zi $(COMMON_FLAGS) 
    endif
    # Sources need java_crw_demo
    SOURCES += ../java_crw_demo/java_crw_demo.c
    # Object files needed to create library
    OBJECTS=$(SOURCES:%.c=%.obj)
    # Library name and options needed to build it
    LIBRARY=$(LIBNAME).dll
    LDFLAGS=
    # Libraries we are dependent on
    LIBRARIES=$(JDK)/
    # Building a shared library
    LINK_SHARED=link -dll -out:$@
endif

# Common -I options
CFLAGS += -I.
CFLAGS += -I../agent_util
CFLAGS += -I../java_crw_demo
CFLAGS += -I$(JDK)/include -I$(JDK)/include/$(OSNAME)

# Default rule (build both native library and jar file)
all: $(LIBRARY) $(JARFILE)

# Build native library
$(LIBRARY): $(OBJECTS)
	$(LINK_SHARED) $(OBJECTS) $(LIBRARIES)

# Build jar file
$(JARFILE): $(JAVA_SOURCES)
	rm -f -r classes
	mkdir -p classes
	$(JDK)/bin/javac -d classes $(JAVA_SOURCES)
	(cd classes; $(JDK)/bin/jar cf ../$@ *)

# Cleanup the built bits
clean:
	rm -f -r classes
	rm -f $(LIBRARY) $(JARFILE) $(OBJECTS)

# Simple tester
test: all
	LD_LIBRARY_PATH=. $(JDK)/bin/java -agentlib:$(LIBNAME) -Xbootclasspath/a:./$(JARFILE) TestHeapTracking
#	LD_LIBRARY_PATH=. $(JDK)/bin/java -agentlib:$(LIBNAME) -Xbootclasspath/a:./$(JARFILE) -version

# Compilation rule only needed on Windows
ifeq ($(OSNAME), win32)
%.obj: %.c
	$(COMPILE.c) $<
endif

