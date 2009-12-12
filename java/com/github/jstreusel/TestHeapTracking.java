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
		
		// make a thousand requests 
		List hold = new ArrayList();
		int byteCount = 0;
		int chunkSize = 1000;
		while(true)
		{
			tracker.startRequest();
			try {
				hold.add(new byte[chunkSize]);
			} 
			catch (OutOfMemoryError ex) 
			{
				break;
			}
			byteCount += chunkSize;
		}
		
//		System.out.println("got out of memory exception after "+(byteCount/chunkSize));
		
		List a = null; 
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
