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

#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <list>
#ifdef USE_BACKTRACE
#include <execinfo.h>
#endif


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
//              for each allocation.
//
/////////////////////////////////////////////////////////////

#ifndef ALLOCATION_STACK_DEPTH
#	define ALLOCATION_STACK_DEPTH 5
#endif

#ifndef PRINTED_DATA_BUFFER_SIZE
#	define PRINTED_DATA_BUFFER_SIZE 50
#endif
#include "LeakTracer_l.hpp"


namespace leaktracer {


/**
 * Main class to trace memory allocations
 * and releases.
 */
class MemoryTrace {
public:
	/** singleton class */
	inline static MemoryTrace & GetInstance(void);

	/** setup undelying libc malloc/free... */
	static int Setup(void);

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

	/** registers memory reallocation, should be called by the
	 *  function intercepting realloc calls */
	inline void registerReallocation(void *p, size_t size, bool is_array);

	/** registers memory release, should be called by the
	 *  function intercepting "delete" calls */
	inline void registerRelease(void *p, bool is_array);

	/** writes report with all memory leaks */
	void writeLeaks(std::ostream &out);

	/** writes report with all memory leaks */
	void writeLeaksToFile(const char* reportFileName);

	/** returns TRUE if all monitoring is currently disabled,
	 *  required to make sure we don't use this class before it
	 *  was properly initialized */
	inline bool AllMonitoringIsDisabled(void) {
		return ( (__monitoringDisabler!=0)||
			 (((intptr_t)pthread_getspecific(__thread_internal_disabler_key)) != 0) );
	}

	inline int InternalMonitoringDisablerThreadUp(void) {
		intptr_t oldvalue;
		oldvalue = (intptr_t)pthread_getspecific(__thread_internal_disabler_key);
		//TRACE((stderr, "InternalMonitoringDisablerThreadUp oldvalue %d\n", oldvalue));
		pthread_setspecific(__thread_internal_disabler_key, (void*) (oldvalue + 1) );
		return oldvalue;
	}

	inline int InternalMonitoringDisablerThreadDown(void) {
		intptr_t oldvalue;
		oldvalue = (intptr_t)pthread_getspecific(__thread_internal_disabler_key);
		//TRACE((stderr, "InternalMonitoringDisablerThreadDown oldvalue %d\n", oldvalue));
		pthread_setspecific(__thread_internal_disabler_key, (void*) (oldvalue - 1) );
		return oldvalue;
	}

	static void __attribute__ ((constructor)) MemoryTraceOnInit(void);
	static void __attribute__ ((destructor)) MemoryTraceOnExit(void);

	/** destructor */
	virtual ~MemoryTrace(void);

private:
	// singleton object
	MemoryTrace(void);
	static MemoryTrace *__instance;

	// global settings
	bool __setupDone;
	bool __monitoringAllThreads;
	bool __monitoringReleases;
	int  __monitoringDisabler;

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
	pthread_key_t __thread_internal_disabler_key;

	static void CleanUpThreadData(void *ptrThreadOptions);
	pthread_key_t __thread_options_key;

	// init functions
	static void init_no_alloc_allowed();
	static pthread_once_t _init_no_alloc_allowed_once;

	void init_full();
	static void init_full_from_once();
	static pthread_once_t _init_full_once;

	// signal handler
	static int __sigStartAllThread;
	static int __sigStopAllThread;
	static int __sigReport;
	static void sigactionHandler(int, siginfo_t *, void *);
	static int signalNumberFromString(const char* signame);

	/** writes report with all memory leaks */
	void writeLeaksPrivate(std::ostream &out);

	// centralized list of all per-thread options
	typedef std::list<ThreadMonitoringOptions*> list_monitoring_options_t;
	list_monitoring_options_t __listThreadOptions;
	Mutex __threadListMutex;

	// per - allocation info
	typedef struct _allocation_info_struct {
		size_t size;
		struct timespec timestamp;
		void * allocStack[ALLOCATION_STACK_DEPTH];
		bool isArray;
	} allocation_info_t;
	void storeAllocationStack(void* arr[ALLOCATION_STACK_DEPTH]);
	inline void storeTimestamp(struct timespec &tm);

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
		InternalMonitoringDisablerThreadUp();
		pOpt = new ThreadMonitoringOptions;
		pthread_setspecific(__thread_options_key, pOpt);
		__listThreadOptions.push_back(pOpt);
		InternalMonitoringDisablerThreadDown();
	}
	return *pOpt;
}


// iterates over list of per-thread objects, and diables
// monitoring for all threads
inline void MemoryTrace::stopMonitoringPerThreadAllocations(void)
{
	leaktracer::MemoryTrace::Setup();

	MutexLock lock(__threadListMutex);
	for (list_monitoring_options_t::iterator it = __listThreadOptions.begin(); it != __listThreadOptions.end(); ++it) {
		(*it)->monitoringAllocations = false;
	}
}


// starts monitoring allocations and releases in all threads
inline void MemoryTrace::startMonitoringAllThreads(void)
{
	leaktracer::MemoryTrace::Setup();

	TRACE((stderr, "LeakTracer: startMonitoringAllThreads\n"));
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
	leaktracer::MemoryTrace::Setup();

	TRACE((stderr, "LeakTracer: startMonitoringThisThread\n"));
	if (!__monitoringAllThreads) {
		if (!__monitoringReleases) {
			MutexLock lock(__allocations_mutex);
			// double-check inside Mutex
			if (!__monitoringReleases) {
				__allocations.clearAllInfo();
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
	leaktracer::MemoryTrace::Setup();

	TRACE((stderr, "LeakTracer: stopMonitoringAllocations\n"));
	if (__monitoringAllThreads)
		__monitoringAllThreads = false;
	else
		getThreadOptions().monitoringAllocations = false;
}


// stop all allocation/releases monitoring
inline void MemoryTrace::stopAllMonitoring(void)
{
	leaktracer::MemoryTrace::Setup();

	TRACE((stderr, "LeakTracer: stopAllMonitoring\n"));
	stopMonitoringAllocations();
	__monitoringReleases = false;
}


// stores allocation stack, up to ALLOCATION_STACK_DEPTH
// frames
//inline void MemoryTrace::storeAllocationStack(void* arr[ALLOCATION_STACK_DEPTH])
//{
//	unsigned int iIndex = 0;
//#ifdef USE_BACKTRACE
//	void* arrtmp[ALLOCATION_STACK_DEPTH+1];
//	iIndex = backtrace(arrtmp, ALLOCATION_STACK_DEPTH + 1) - 1;
//	memcpy(arr, &arrtmp[1], iIndex*sizeof(void*));
//#else
//	void *pFrame;
//	// NOTE: we can't use "for" loop, __builtin_* functions
//	// require the number to be known at compile time
//	arr[iIndex++] = (                  (pFrame = __builtin_frame_address(0)) != NULL) ? __builtin_return_address(0) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(1)) != NULL) ? __builtin_return_address(1) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(2)) != NULL) ? __builtin_return_address(2) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(3)) != NULL) ? __builtin_return_address(3) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(4)) != NULL) ? __builtin_return_address(4) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(5)) != NULL) ? __builtin_return_address(5) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(6)) != NULL) ? __builtin_return_address(6) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(7)) != NULL) ? __builtin_return_address(7) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(8)) != NULL) ? __builtin_return_address(8) : NULL; if (iIndex == ALLOCATION_STACK_DEPTH) return;
//	arr[iIndex++] = (pFrame != NULL && (pFrame = __builtin_frame_address(9)) != NULL) ? __builtin_return_address(9) : NULL;
//#endif
//	// fill remaining spaces
//	for (; iIndex < ALLOCATION_STACK_DEPTH; iIndex++)
//		arr[iIndex] = NULL;
//}


// adds all relevant info regarding current allocation to map
inline void MemoryTrace::registerAllocation(void *p, size_t size, bool is_array)
{
	allocation_info_t *info = NULL;
	if (!AllMonitoringIsDisabled() && (__monitoringAllThreads || getThreadOptions().monitoringAllocations) && p != NULL) {
		MutexLock lock(__allocations_mutex);
		info = __allocations.insert(p);
		if (info != NULL) {
			info->size = size;
			info->isArray = is_array;
			storeTimestamp(info->timestamp);
		}
	}
 	// we store the stack without locking __allocations_mutex
	// it should be safe enough
	// prevent a deadlock between backtrave function who are now using advanced dl_iterate_phdr function
 	// and dl_* function which uses malloc functions
	if (info != NULL) {
		storeAllocationStack(info->allocStack);
	}

	if (p == NULL) {
		InternalMonitoringDisablerThreadUp();
		// WARNING
		InternalMonitoringDisablerThreadDown();
	}
}


// adds all relevant info regarding current allocation to map
inline void MemoryTrace::registerReallocation(void *p, size_t size, bool is_array)
{
	if (!AllMonitoringIsDisabled() && (__monitoringAllThreads || getThreadOptions().monitoringAllocations) && p != NULL) {
		MutexLock lock(__allocations_mutex);
		allocation_info_t *info = __allocations.find(p);
		if (info != NULL) {
			info->size = size;
			info->isArray = is_array;
			storeAllocationStack(info->allocStack);
			storeTimestamp(info->timestamp);
		}
	}

	if (p == NULL) {
		InternalMonitoringDisablerThreadUp();
		// WARNING
		InternalMonitoringDisablerThreadDown();
	}
}


// removes allocation's info from the map
inline void MemoryTrace::registerRelease(void *p, bool is_array)
{
	if (!AllMonitoringIsDisabled() && __monitoringReleases && p != NULL) {
		MutexLock lock(__allocations_mutex);
		allocation_info_t *info = __allocations.find(p);
		if (info != NULL) {
			if (info->isArray != is_array) {
				InternalMonitoringDisablerThreadUp();
				// WARNING
				InternalMonitoringDisablerThreadDown();
			}
			__allocations.release(p);
		}
	}
}

// storetimestamp function
inline void MemoryTrace::storeTimestamp(struct timespec &timestamp)
{
  clock_gettime(CLOCK_MONOTONIC, &timestamp);
}


}  // end namespace


#endif  // include once


