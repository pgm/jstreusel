public class RequestStats
{
	public final int requestId;
	public final int objectCount;
	public final int memoryCount;
	public final int arrayCount;
	
	public RequestStats(int requestId, int objectCount, int memoryCount, int arrayCount)
	{
		this.requestId = requestId;
		this.objectCount = objectCount;
		this.memoryCount = memoryCount;
		this.arrayCount = arrayCount;
	}
}
