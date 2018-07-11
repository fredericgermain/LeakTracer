#include "leaktracer.h"
#include "MemoryTrace.hpp"


/** starts monitoring memory allocations in all threads */
void leaktracer_startMonitoringAllThreads()
{
	leaktracer::MemoryTrace::GetInstance().startMonitoringAllThreads();
}

/** starts monitoring memory allocations in current thread */
void leaktracer_startMonitoringThisThread()
{
	leaktracer::MemoryTrace::GetInstance().startMonitoringThisThread();
}

/** stops monitoring memory allocations (in all threads or in
 *   this thread only, depends on the function used to start
 *   monitoring
 */
void leaktracer_stopMonitoringAllocations()
{
	leaktracer::MemoryTrace::GetInstance().stopMonitoringAllocations();
}

/** stops all monitoring - both of allocations and releases */
void leaktracer_stopAllMonitoring()
{
	leaktracer::MemoryTrace::GetInstance().stopAllMonitoring();
}

/** writes report with all memory leaks */
void leaktracer_writeLeaksToFile(const char* reportFileName)
{
	leaktracer::MemoryTrace::GetInstance().writeLeaksToFile(reportFileName);
}
