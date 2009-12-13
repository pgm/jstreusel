package com.github.jstreusel;

import java.util.*;
import java.lang.OutOfMemoryError;

public class TestHeapTracking {
	public static void main(String args[]) {
		HeapTracker tracker = new HeapTracker();
		
		int id = tracker.startRequest();
		
		System.out.println("request "+id);
		
		byte[] data = new byte[1024];

		id = tracker.startRequest();
		
		System.out.println("request "+id);
		
		byte[] data2 = new byte[2024];
		
		for(RequestStats stat:tracker.getRequestStats())
		{
			System.out.println("request "+stat.requestId+", objectCount: "+stat.objectCount+", memoryCount: "+stat.memoryCount+", arrayCount: "+stat.arrayCount);
		}

		List a = new ArrayList(); 
		
		// make a thousand requests 
		List hold = new ArrayList();
		int loopCount = 0;
		int chunkSize = 1000;
		boolean outofmem = false;
		while(!outofmem)
		{
			tracker.startRequest();
			for(int i = 0;i<1000;i++) {
				try {
					hold.add(new byte[chunkSize]);
				} 
				catch (OutOfMemoryError ex) 
				{
					outofmem = true;
					break;
				}
			}
			loopCount ++;
		}
		
		System.out.println("loopCount");
		System.out.println(Integer.toString(loopCount));
		
		try {
			System.out.println("done");
			while(true) {
				System.out.println("before");
				a.addAll( tracker.getRequestStats() );
				System.out.println("after");
			}
		} catch (OutOfMemoryError ex) {
			System.out.println("survived call while memory was low");
		};
		System.out.println("counter:"+a.size());
	}
}
