package com.github.jstreusel.web;

import java.io.IOException;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;

import com.github.jstreusel.IHeapTracker;
import com.github.jstreusel.RequestStats;

public class RequestTrackingFilter implements Filter {

	Map<Integer, RequestDetails> detailsByRequestId = new HashMap<Integer, RequestDetails>();
	IHeapTracker tracker;
	
	public static class RequestDetails 
	{
		int requestId;
		String url;
		long startTime;
		long stopTime;
		boolean inProgress;
		public String getUrl() {
			return url;
		}
		public Date getStartTime() {
			return new Date(startTime);
		}
		public Date getStopTime() {
			return new Date(stopTime);
		}
		public boolean isInProgress() {
			return inProgress;
		}
	}
	
	public void destroy() {
	}

	public void doFilter(ServletRequest request, ServletResponse response,
			FilterChain chain) throws IOException, ServletException {
		
		HttpServletRequest httpRequest = (HttpServletRequest)request;
		
		RequestDetails details = beforeHandlingRequest(httpRequest);
		try {
			chain.doFilter(request, response);
		} finally {
			afterHandlingRequest(details);
		}
	}

	private void afterHandlingRequest(RequestDetails details) {
		tracker.endRequest();

		details.stopTime = System.currentTimeMillis();
		details.inProgress = false;
	}

	private RequestDetails beforeHandlingRequest(HttpServletRequest httpRequest) {
		RequestDetails details = new RequestDetails();
		details.url = httpRequest.getRequestURI();
		details.startTime = System.currentTimeMillis();
		details.inProgress = true;
		details.stopTime = 0;
		details.requestId = tracker.startRequest();

		synchronized(detailsByRequestId)
		{
			detailsByRequestId.put(details.requestId, details);
		}
		
		return details;
	}

	public void garbageCollectRequests() {
		Set<Integer> unusedKeys;
		synchronized(detailsByRequestId)
		{
			unusedKeys = new HashSet<Integer>(detailsByRequestId.keySet());
		}
		
		for(RequestStats stats : tracker.getRequestStats()) {
			// if it's in the stats, it's still in use, otherwise
			// it's safe the throw out those details
			unusedKeys.remove(stats.requestId);
		}

		// now remove the unused details
		synchronized(detailsByRequestId) 
		{
			detailsByRequestId.keySet().removeAll(unusedKeys);
		}
	}
	
	public void init(FilterConfig config) throws ServletException {
		
	}
}
