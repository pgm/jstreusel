Summary

Jstreusel is a JVMTI agent which enables tracking of memory usage on 
a "per request" basis.

Motivation

In a multithreaded web application, it can be difficult to diagnose resource 
utilization problems.   The resource I am trying to investigate here is 
memory consumption.  Since the memory is owned by a single process which may 
be handling multiple, varied requests, it's hard to say why memory usage is
high at any given time.

It could be due to:

- lots of concurrent requests where each consumes some memory.
- a particular request results in a large amount of memory being consumed.

Couple this with using 3rd party libraries, such as Hibernate, it can be 
difficult to predict a priori what the memory usage will be.

In general, many failures can be gracefully handled in Java.   However, 
out of memory exceptions are especially dangerous because memory is a 
common resource, so a memory problem in one thread can cause issues for
an unrelated request.   This makes it much more difficult to understand
the root cause.

Background

This started as a morning of hacking around.   The goal 
was to produce a tool for tracking and monitoring memory consumption
on a per-request basis for diagnosing memory leaks in JVM based web 
applications.

This tool is only minorly evasive and allows you to track "per request" 
memory consumption.    (Where a request is defined as a period of 
execution on a single thread.)  There shouldn't be any extra memory 
consumed per allocation while the agent is loaded and it can be run 
in a stock jvm.

Usage

There are basically three calls:
	startRequest()
	endRequest()
	List<RequestStats> getRequestStats()

startRequest() should be called when a new request has begun.  It will 
return a sequential id which will be used to track all subsequent memory
allocations performed be this thread.

Then, after the request is complete, endRequest() can be invoked to stop
tracking memory allocations within the current thread.

To get summary statistics of the heap organized broken up by requestId, 
call getRequestStats().   This will in turn perform a garbage collection
(so that only memory still referenced remains) and then the heap is 
walked, and the memory is tabulated by requestId.

Included in the distribution is a servlet filter which performs the 
startRequest() and endRequest() as well as produces an html report
with the current stats when the "memory-info" url is hit.


