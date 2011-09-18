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

#ifndef __MEMORY_TRACE_h_included__
#define __MEMORY_TRACE_h_included__

#include <pthread.h>
#include <iostream>
#include <list>

#include "Mutex.hpp"
#include "MutexLock.hpp"
#include "MapMemoryInfo.hpp"


/////////////////////////////////////////////////////////////
// Following MACROS are available at compile time:
// 
// START_TRACING_FROM_PROCESS_START - starts monitoring memory
//              allocations from the start of BeatBox process.
//              Otherwise should be explicetly activated
//              default: OFF
// 
// ALLOCATION_STACK_DEPTH - max number of stack frame to 
//              save. Max supported value: 10
// 
// PRINTED_DATA_BUFFER_SIZE - size of the data buffer to be printed
// 				for each allocation.
// 
/////////////////////////////////////////////////////////////

#ifndef ALLOCATION_STACK_DEPTH
#	define ALLOCATION_STACK_DEPTH 3
#endif

#ifndef PRINTED_DATA_BUFFER_SIZE
#	define PRINTED_DATA_BUFFER_SIZE 50
#endif


namespace leaktracer {


/**
 * Main class to trace memory allocations
 * and releases.
 */
class MemoryTrace {
public:
    /** singleton class */
	inline static MemoryTrace & GetInstance(void);

    /** starts monitoring memory allocations in all threads */
	inline void startMonitoringAllThreads(void);
    /** starts monitoring memory allocations in current thread */
	inline void startMonitoringThisThread(void);

    /** stops monitoring memory allocations (in all threads or in
    *   this thread only, depends on the function used to start
    *   monitoring */
	inline void stopMonitoringAllocations(void);

    /** stops all monitoring - both of allocations and releases */
	inline void stopAllMonitoring(void);

    /** registers new memory allocation, should be called by the
     *  function intercepting "new" calls */
	inline void registerAllocation(void *p, size_t size, bool is_array);
    /** registers memory release, should be called by the
     *  function intercepting "delete" calls */
	inline void registerRelease(void *p, bool is_array);

    /** writes report with all memory leaks */
	void writeLeaks(std::ostream &out);

    /** returns TRUE if all monitoring is currently disabled,
     *  required to make sure we don't use this class before it
     *  was properly initialized */
	static inline bool AllMonitoringIsDisabled(void) { return __internal_disable_monitoring; }

    /** destructor */
	virtual ~MemoryTrace(void);

private:
	// singleton object
	MemoryTrace(void);
	static MemoryTrace *__instance;
	static Mutex __instanceMutex;
	static bool __internal_disable_monitoring;

	// global settings
	bool __monitoringAllThreads;
	bool __monitoringReleases;

	// per-thread settings, for cases where only allocations
	// made by specific threads are monitored
	struct ThreadMonitoringOptions {
		bool monitoringAllocations;
		inline ThreadMonitoringOptions() : monitoringAllocations(false) {}
	};
	inline ThreadMonitoringOptions & getThreadOptions(void);
	void removeThreadOptions(ThreadMonitoringOptions *pOptions);
	inline void stopMonitoringPerThreadAllocations(void);

	// key to access per-thread info
	static void CleanUpThreadData(void *ptrThreadOptions);
	pthread_key_t __thread_options_key;

	// centralized list of all per-thread options
	typedef std::list<ThreadMonitoringOptions*> list_monitoring_options_t;
	list_monitoring_options_t __listThreadOptions;
	Mutex __threadListMutex;

	// per - allocation info
	typedef struct _allocation_info_struct {
		size_t size;
		void * allocStack[ALLOCATION_STACK_DEPTH];
		bool isArray;
	} allocation_info_t;
	inline void storeAllocationStack(void* arr[ALLOCATION_STACK_DEPTH]);

	typedef TMapMemoryInfo<allocation_info_t> memory_allocations_info_t;
	memory_allocations_info_t __allocations;
	Mutex __allocations_mutex;
	void clearAllocationsInfo(void);
};



//////////////////////////////////////////////////////////////////////
// 
// IMPLEMENTATION: MemoryTrace
// (inline functions)
// 
//////////////////////////////////////////////////////////////////////


// Returns singleton instance of MemoryTrace object
inline MemoryTrace & MemoryTrace::GetInstance(void)
{
	if (__instance == NULL) {
		// create new instance
		MutexLock lock(__instanceMutex);
		if (__instance == NULL) {
			__internal_disable_monitoring = true;
			__instance = new MemoryTrace;
			__internal_disable_monitoring = false;
		}
	}
	return *__instance;
}


// Returns per-thread object for calling thread
// (creates one if called for the first time)
inline MemoryTrace::ThreadMonitoringOptions & MemoryTrace::getThreadOptions(void)
{
	ThreadMonitoringOptions *pOpt = reinterpret_cast<ThreadMonitoringOptions*>(pthread_getspecific(__thread_options_key));
	if (pOpt == NULL) {
		MutexLock lock(__threadListMutex);
		// before creating new object we need to disable any monitoring
		__internal_disable_monitoring = true;
		pOpt = new ThreadMonitoringOptions;
		pthread_setspecific(__thread_options_key, pOpt);
		__listThreadOptions.push_back(pOpt);
		__internal_disable_monitoring = false;
	}
	return *pOpt;
}


// iterates over list of per-thread objects, and diables
// monitoring for all threads
inline void MemoryTrace::stopMonitoringPerThreadAllocations(void)
{
	MutexLock lock(__threadListMutex);
	for (list_monitoring_options_t::iterator it = __listThreadOptions.begin(); it != __listThreadOptions.end(); ++it) {
		(*it)->monitoringAllocations = false;
	}
}


// starts monitoring allocations and releases in all threads
inline void MemoryTrace::startMonitoringAllThreads(void)
{
	if (!__monitoringReleases) {
		MutexLock lock(__allocations_mutex);
		// double-check inside Mutex
		if (!__monitoringReleases) {
			__allocations.clearAllInfo();
			__monitoringReleases = true;
		}
	}
	__monitoringAllThreads = true;
	stopMonitoringPerThreadAllocations();
}


// starts monitoring memory allocations in this thread
// (note: releases are monitored in all threads)
inline void MemoryTrace::startMonitoringThisThread(void)
{
	if (!__monitoringAllThreads) {
		if (!__monitoringReleases) {
			MutexLock lock(__allocations_mutex);
			// double-check inside Mutex
			if (!__monitoringReleases) {
				clearAllocationsInfo();
				__monitoringReleases = true;
			}
		}
		getThreadOptions().monitoringAllocations = true;
	}
}


// Depending on "startMonitoring" function used previously,
// will stop monitoring all threads, of current thread only
inline void MemoryTrace::stopMonitoringAllocations(void)
{
	if (__monitoringAllThreads)
		__monitoringAllThreads = false;
	else
		getThreadOptions().monitoringAllocations = false;
}


// stop all allocation/releases monitoring
inline void MemoryTrace::stopAllMonitoring(void)
{
	stopMonitoringAllocations();
	__monitoringReleases = false;
}


// stores allocation stack, up to ALLOCATION_STACK_DEPTH
// frames
inline void MemoryTrace::storeAllocationStack(void* arr[ALLOCATION_STACK_DEPTH])
{
	void *pFrame;
	unsigned int iIndex = 0;

	// NOTE: we can't use "for" loop, __builtin_* functions
	// require the number to be known at compile time
	arr[iIndex++] = (                  (pFrame = __builtin_frame_address(0)) != NULL) ? __builtin_return_address(0) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(1)) != NULL) ? __builtin_return_address(1) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(2)) != NULL) ? __builtin_return_address(2) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(3)) != NULL) ? __builtin_return_address(3) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(4)) != NULL) ? __builtin_return_address(4) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(5)) != NULL) ? __builtin_return_address(5) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(6)) != NULL) ? __builtin_return_address(6) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(7)) != NULL) ? __builtin_return_address(7) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(8)) != NULL) ? __builtin_return_address(8) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(9)) != NULL) ? __builtin_return_address(9) : NULL; 

	// fill remaining spaces
	for (; iIndex < ALLOCATION_STACK_DEPTH; iIndex++)
		arr[iIndex] = NULL;
}


// adds all relevant info regarding current allocation to map
inline void MemoryTrace::registerAllocation(void *p, size_t size, bool is_array)
{
	if ((__monitoringAllThreads || getThreadOptions().monitoringAllocations) && !__internal_disable_monitoring && p != NULL) {
		MutexLock lock(__allocations_mutex);
		allocation_info_t *info = __allocations.insert(p);
		if (info != NULL) {
			info->size = size;
			info->isArray = is_array;
			storeAllocationStack(info->allocStack);
		}
	}

	if (p == NULL) {
		__internal_disable_monitoring = true;
		// WARNING
		__internal_disable_monitoring = false;
	}
}


// removes allocation's info from the map
inline void MemoryTrace::registerRelease(void *p, bool is_array)
{
	if (__monitoringReleases && !__internal_disable_monitoring && p != NULL) {
		MutexLock lock(__allocations_mutex);
		allocation_info_t *info = __allocations.find(p);
		if (info != NULL) {
			if (info->isArray != is_array) {
				__internal_disable_monitoring = true;
				// WARNING
				__internal_disable_monitoring = false;
			}
			__allocations.release(p);
		}
	}
}



}  // end namespace

#endif  // include once


