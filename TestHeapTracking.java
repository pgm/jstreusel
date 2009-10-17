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
		
		System.out.println("finished test.  (Ignore rest of line: "+data.length+data2.length+")");
	}
}
