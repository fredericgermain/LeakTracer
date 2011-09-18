#ifndef __LEAKTRACER_H__
#define __LEAKTRACER_H__

#ifdef __cplusplus
extern "C"
{
#endif

/** starts monitoring memory allocations in all threads */
void leaktracer_startMonitoringAllThreads(void);

/** starts monitoring memory allocations in current thread */
void leaktracer_startMonitoringThisThread(void);

/** stops monitoring memory allocations (in all threads or in
 *   this thread only, depends on the function used to start
 *   monitoring
 */
void leaktracer_stopMonitoringAllocations(void);

/** stops all monitoring - both of allocations and releases */
void leaktracer_stopAllMonitoring(void);

/** writes report with all memory leaks */
void leaktracer_writeLeaksToFile(const char* reportFileName);

#ifdef __cplusplus
}
#endif

#endif /* LEAKTRACER_H */
