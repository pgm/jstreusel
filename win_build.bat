set COMPILE=cl -Od -Zi -Gy -DWIN32 -W0 -WX  -I. -I"%JDK_HOME%\include" -I"%JDK_HOME%\include\win32" -c 
set JAVA_SOURCES=java/com/github/jstreusel/HeapTracker.java java/com/github/jstreusel/HeapTrackerJNISupport.java java/com/github/jstreusel/IHeapTracker.java java/com/github/jstreusel/RequestStats.java java/com/github/jstreusel/TestHeapTracking.java

%COMPILE% heapTracker.c
%COMPILE% agent_util.c
%COMPILE% java_crw_demo.c

link -dll -out:heapTracker.dll heapTracker.obj agent_util.obj java_crw_demo.obj
mkdir classes
del /q classes
"%JDK_HOME%/bin/javac" -d classes -sourcepath java %JAVA_SOURCES%
jar cf heapTracker.jar -C classes .