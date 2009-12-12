export LD_LIBRARY_PATH=. 
java -d32 -agentpath:./libheapTracker.so -Xbootclasspath/a:./heapTracker.jar -cp ./classes  com.github.jstreusel.TestHeapTracking

