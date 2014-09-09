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

#ifdef OSX
void* (*__malloc_zone_malloc)(malloc_zone_t *zone, size_t size);
void  (*__malloc_zone_free)(malloc_zone_t *zone, void* ptr);
void* (*__malloc_zone_calloc)(malloc_zone_t *zone, size_t num_items, size_t size);
void* (*__malloc_zone_valloc)(malloc_zone_t *zone, size_t size);
void* (*__malloc_zone_realloc)(malloc_zone_t *zone, void *ptr, size_t size);
void* (*__malloc_zone_memalign)(malloc_zone_t *zone, size_t alignment, size_t size);
malloc_zone_t *__malloc_default_zone;
#endif


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
extern "C" void *malloc(size_t size)
{
	void *p;
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadUp();
	p = LT_MALLOC(size);
	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadDown();
	leaktracer::MemoryTrace::GetInstance().registerAllocation(p, size, false);

	fprintf(stderr, "malloc: %p %ld\n", p, size);

	return p;
}

void free(void* ptr)
{
	leaktracer::MemoryTrace::Setup();

	fprintf(stderr, "free: %p\n", ptr);

	leaktracer::MemoryTrace::GetInstance().registerRelease(ptr, false);
	LT_FREE(ptr);
}

void* realloc(void *ptr, size_t size)
{
	void *p;
	leaktracer::MemoryTrace::Setup();

	leaktracer::MemoryTrace::GetInstance().InternalMonitoringDisablerThreadUp();

	p = LT_REALLOC(ptr, size);

	fprintf(stderr, "realloc: %ld %p -> %p\n", size, ptr, p);

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

	fprintf(stderr, "calloc: %ld %p\n", size, p);

	return p;
}

void * malloc_zone_malloc(malloc_zone_t *zone, size_t size) {
    leaktracer::MemoryTrace::Setup();
    
	fprintf(stderr, "malloc_zone_malloc: %ld\n", size);
    if (zone != __malloc_default_zone)
        return (*__malloc_zone_malloc)(zone, size);
    
    return malloc(size);
}

void *
malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size) {
    leaktracer::MemoryTrace::Setup();
    
    if (zone != __malloc_default_zone)
        return (*__malloc_zone_malloc)(zone, size);
    
    return calloc(num_items, size);
}

void *
malloc_zone_valloc(malloc_zone_t *zone, size_t size);

void *
malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size) {
    leaktracer::MemoryTrace::Setup();
    
    if (zone != __malloc_default_zone)
        return (*__malloc_zone_malloc)(zone, size);
    
    return realloc(ptr, size);
}

void *
malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size);

void
malloc_zone_free(malloc_zone_t *zone, void *ptr) {
    leaktracer::MemoryTrace::Setup();
    
    if (zone != __malloc_default_zone) {
        (*__malloc_zone_free)(zone, ptr);
    
        return;
    }
    free(ptr);
}

