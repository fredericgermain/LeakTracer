////////////////////////////////////////////////////////
//
// LeakTracer
// Contribution to original project by Erwin S. Andreasen
// site: http://www.andreasen.org/LeakTracer/
// 
// Added by Michael Gopshtein, 2006
// mgopshtein@gmail.com
// 
// Any comments/suggestions are welcome
// 
////////////////////////////////////////////////////////


#include "MemoryTrace.hpp"


void* operator new(size_t size) {
	void *p = malloc(size);
	if (!leaktracer::MemoryTrace::AllMonitoringIsDisabled())
		leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, false);
	return p;
}


void* operator new[] (size_t size) {
	void *p = malloc(size);
	if (!leaktracer::MemoryTrace::AllMonitoringIsDisabled())
		leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, true);
	return p;
}


void operator delete (void *p) {
	if (!leaktracer::MemoryTrace::AllMonitoringIsDisabled())
		leaktracer::MemoryTrace::GetInstance().registerRelease(p, false);
	free(p);
}


void operator delete[] (void *p) {
	if (!leaktracer::MemoryTrace::AllMonitoringIsDisabled())
		leaktracer::MemoryTrace::GetInstance().registerRelease(p, true);
	free(p);
}




