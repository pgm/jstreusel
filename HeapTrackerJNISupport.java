import java.util.List;

public class HeapTrackerJNISupport
{
    private static int engaged = 0; 
  
    static native void _newobj(Object thread, Object o);
    static native void _start_request(Object thread, int requestId); 
    static native void _newarr(Object thread, Object a);
    static native void _extract_stats(Object clazz, Object dest);

    public static void newobj(Object o)
    {
	if ( engaged != 0 ) {
	    _newobj(Thread.currentThread(), o);
	}
    }
    
    public static void newarr(Object a)
    {
	if ( engaged != 0 ) {
	    _newarr(Thread.currentThread(), a);
	}
    }

}
