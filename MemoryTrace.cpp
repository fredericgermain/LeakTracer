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
#include <ctype.h>

namespace leaktracer {


MemoryTrace *MemoryTrace::__instance = NULL;
Mutex MemoryTrace::__instanceMutex;
bool MemoryTrace::__internal_disable_monitoring = false;


MemoryTrace::MemoryTrace(void) :
#	ifdef START_TRACING_FROM_PROCESS_START
		// feature enabled + monitoring from the start
		__monitoringAllThreads(true), __monitoringReleases(true)
#	else
		// feature enabled, but should not monitor from the start
		__monitoringAllThreads(false), __monitoringReleases(false)
#	endif
{
	pthread_key_create(&__thread_options_key, CleanUpThreadData);
}

MemoryTrace::~MemoryTrace(void)
{
	pthread_key_delete(__thread_options_key);
}



// is called automatically when thread exists, whould
// cleanup per-thread data
void MemoryTrace::CleanUpThreadData(void *ptrThreadOptions)
{
	if( ptrThreadOptions != NULL )
		GetInstance().removeThreadOptions( reinterpret_cast<ThreadMonitoringOptions*>(ptrThreadOptions) );
}


// cleans per-thread configuration object, and removes
// it from the list of all objects
void MemoryTrace::removeThreadOptions(ThreadMonitoringOptions *pOptions)
{
	MutexLock lock(__threadListMutex);
	for (list_monitoring_options_t::iterator it = __listThreadOptions.begin(); it != __listThreadOptions.end(); ++it) {
		if (*it == pOptions) {
			// found this object in the list
			delete *it;
			__listThreadOptions.erase(it);
			return;
		}
	}
}


// writes all memory leaks to given stream
void MemoryTrace::writeLeaks(std::ostream &out)
{
	MutexLock lock(__allocations_mutex);
	__internal_disable_monitoring = true;

	out << "# LeakTracer report\n";

	allocation_info_t *info;
	void *p;
	__allocations.beginIteration();
	while (__allocations.getNextPair(&info, &p)) {
		out << "Size=" << info->size;

		out << ", stack=";
		for (unsigned int i = 0; i < ALLOCATION_STACK_DEPTH; i++) {
			if (info->allocStack[i] == NULL) break;

			if (i > 0) out << ' ';
			out << info->allocStack[i];
		}

		out << ", data=";
		const char *data = reinterpret_cast<const char *>(p);
		for (unsigned int i = 0; i < PRINTED_DATA_BUFFER_SIZE && i < info->size; i++) 
			out << (isalpha(data[i]) ? data[i] : '.');
		out << '\n';
	}

	__internal_disable_monitoring = false;
}


void MemoryTrace::clearAllocationsInfo(void)
{
	MutexLock lock(__allocations_mutex);
	__allocations.clearAllInfo();
}


}  // end namespace


