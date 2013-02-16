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
#include "LeakTracer_l.hpp"

void* (*lt_malloc)(size_t size);
void  (*lt_free)(void* ptr);
void* (*lt_realloc)(void *ptr, size_t size);
void* (*lt_calloc)(size_t nmemb, size_t size);

void* operator new(size_t size) {
	void *p;
	leaktracer::MemoryTrace::Setup();

	p = LT_MALLOC(size);
	leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, false);

	return p;
}


void* operator new[] (size_t size) {
	void *p;
	leaktracer::MemoryTrace::Setup();

	p = LT_MALLOC(size);
	leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, true);

	return p;
}


void operator delete (void *p) {
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().registerRelease(p, false);
	LT_FREE(p);
}


void operator delete[] (void *p) {
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().registerRelease(p, true);
	LT_FREE(p);
}

/** -- libc memory operators -- **/

/* malloc
 * in some malloc implementation, there is a recursive call to malloc
 * (for instance, in uClibc 0.9.29 malloc-standard )
 * we use a InternalMonitoringDisablerThreadUp that use a tls variable to prevent several registration
 * during the same malloc
 */
void *malloc(size_t size)
{
	void *p;
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadUp();
	p = LT_MALLOC(size);
	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadDown();
	leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, false);

	return p;
}

void free(void* ptr)
{
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().registerRelease(ptr, false);
	LT_FREE(ptr);
}

void* realloc(void *ptr, size_t size)
{
	void *p;
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadUp();

	p = LT_REALLOC(ptr, size);

	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadDown();

	if (p != ptr)
	{
		if (ptr)
			leaktracer::MemoryTrace::GetInstance().registerRelease(ptr, false);
		leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, false);
	}
	else
	{
		leaktracer::MemoryTrace::GetInstance().registerReallocation(p, size, false);
	}

	return p;
}

void* calloc(size_t nmemb, size_t size)
{
	void *p;
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadUp();
	p = LT_CALLOC(nmemb, size);
	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadDown();
	leaktracer::MemoryTrace::GetInstance().registerAllocation(p, nmemb*size, false);

	return p;
}
