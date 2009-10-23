package com.github.jstreusel;
import java.util.List;

public interface IHeapTracker {
	public List<RequestStats> getRequestStats();
	public int startRequest();
	public void endRequest();
}
