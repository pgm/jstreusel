TODO: Write some real documentation.

This is the result of a morning of hacking around.   The goal 
was to produce a tool for tracking and monitoring memory consumption
on a per-request basis for diagnosing memory leaks in JVM based web 
applications.

This tool is only minorly evasive and allows you to track "per request" 
memory consumption.    (Where a request is defined as a period of 
execution on a single thread.)  There shouldn't be any extra memory 
consumed per allocation while the agent is loaded and it can be run 
in a stock jvm.

It requires application changes to tell it where 
the start and end of a "request" is.

There's a IHeapTracker interface which has methods:
	startRequest()
	endRequest()
	List<RequestStats> getRequestStats()

For a web server, the way I'd imagine hooking this up is in the 
first HTTP filter, call startRequest().   Then after the http 
request has been handled call endRequest().   

Then periodically (or you could have a UI to generate a report) call 
getRequestStats() to get a summary of the memory which was allocated within 
a start/endRequest(), broken up by each pair of calls within each thread.


