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

#include <sys/syscall.h>

#include "MemoryTrace.hpp"
#include <ctype.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

#include <dlfcn.h>
#include <assert.h>


#include <stdio.h>
#include "LeakTracer_l.hpp"

// glibc/eglibc: dlsym uses calloc internally now, so use weak symbol to get their symbol
extern "C" void* __libc_malloc(size_t size) __attribute__((weak));
extern "C" void  __libc_free(void* ptr) __attribute__((weak));
extern "C" void* __libc_realloc(void *ptr, size_t size) __attribute__((weak));
extern "C" void* __libc_calloc(size_t nmemb, size_t size) __attribute__((weak));

namespace leaktracer {


MemoryTrace *MemoryTrace::__instance = NULL;
pthread_once_t MemoryTrace::_thread_create_key_once = PTHREAD_ONCE_INIT;
pthread_once_t MemoryTrace::_thread_init_all_once = PTHREAD_ONCE_INIT;
pthread_key_t  MemoryTrace::__thread_internal_disabler_key;
int MemoryTrace::__sigStartAllThread = 0;
int MemoryTrace::__sigStopAllThread = 0;
int MemoryTrace::__sigReport = 0;


MemoryTrace::MemoryTrace(void) :
		__monitoringAllThreads(false), __monitoringReleases(false)
{
	pthread_key_create(&__thread_options_key, CleanUpThreadData);
}

void MemoryTrace::sigactionHandler(int sigNumber, siginfo_t *siginfo, void *arg)
{
	(void)siginfo;
	(void)arg;
	if (sigNumber == __sigStartAllThread)
	{
  		TRACE((stderr, "MemoryTracer: signal %d received, starting monitoring\n", sigNumber));
		leaktracer::MemoryTrace::GetInstance().startMonitoringAllThreads();
	}
	if (sigNumber == __sigStopAllThread)
	{
  		TRACE((stderr, "MemoryTracer: signal %d received, stoping monitoring\n", sigNumber));
		leaktracer::MemoryTrace::GetInstance().stopAllMonitoring();
	}
	if (sigNumber == __sigReport)
	{
		const char* reportFilename;
                if (getenv("LEAKTRACER_ONSIG_REPORTFILENAME") == NULL)
                	reportFilename = "leaks.out";
		else
			reportFilename = getenv("LEAKTRACER_ONSIG_REPORTFILENAME");
  		TRACE((stderr, "MemoryTracer: signal %d received, writing report to %s\n", sigNumber, reportFilename));
		leaktracer::MemoryTrace::GetInstance().writeLeaksToFile(reportFilename);
	}
}

int MemoryTrace::signalNumberFromString(const char* signame)
{
	if (strncmp(signame, "SIG", 3) == 0)
		signame += 3;

	if (strcmp(signame, "USR1") == 0)
		return SIGUSR1;
	else if (strcmp(signame, "USR2") == 0)
		return SIGUSR2;
	else
		return atoi(signame);
}

void
MemoryTrace::init_create_key()
{
	pthread_key_create(&__thread_internal_disabler_key, NULL);
}

void
MemoryTrace::init_all()
{
	int sigNumber;
	struct sigaction sigact;

	if (!getenv("LEAKTRACER_NOBANNER"))
	{
#ifdef SHARED
		fprintf(stderr, "LeakTracer " LEAKTRACER_VERSION " (shared library) -- LGPLv2\n");
#else
		fprintf(stderr, "LeakTracer " LEAKTRACER_VERSION " (static library) -- LGPLv2\n");
#endif
	}

	if (!lt_calloc)
	{
		if (__libc_calloc)
		{
			TRACE((stderr, "LeakTracer: setup using libc calloc without usin dlsym\n"));
			lt_calloc = __libc_calloc;
		}
		else
		{
			TRACE((stderr, "LeakTracer: setup dlsym(lt_calloc)\n"));
			lt_calloc = (void*(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
			TRACE((stderr, "LeakTracer: setup lt_calloc=%p\n", lt_calloc));
			if (!lt_calloc) {
				fprintf(stderr, "LeakTracer: could not resolve 'calloc' in 'libc.so': %s\n", dlerror());
				exit(1);
			}
		}
	}
		if (!lt_malloc)
	{
		if (__libc_malloc)
		{
			TRACE((stderr, "LeakTracer: setup using libc malloc without usin dlsym\n"));
			lt_malloc = __libc_malloc;
		}
		else
		{
			lt_malloc = (void*(*)(size_t))dlsym(RTLD_NEXT, "malloc");
			TRACE((stderr, "LeakTracer: setup lt_malloc=%p\n", lt_malloc));
			if (!lt_malloc) {
				fprintf(stderr, "LeakTracer: could not resolve 'malloc' in 'libc.so': %s\n", dlerror());
				exit(1);
			}
		}
	}
	if (!lt_free)
	{
		if (__libc_free)
		{
			TRACE((stderr, "LeakTracer: setup using libc free without usin dlsym\n"));
			lt_free = __libc_free;
		}
		else
		{
			lt_free = (void(*)(void*))dlsym(RTLD_NEXT, "free");
			TRACE((stderr, "LeakTracer: setup lt_free=%p\n", lt_free));
			if (!lt_free) {
				fprintf(stderr, "LeakTracer: could not resolve 'free' in 'libc.so': %s\n", dlerror());
				exit(1);
			}
		}
	}
	if (!lt_realloc)
	{
		if (__libc_realloc)
		{
			TRACE((stderr, "LeakTracer: setup using libc realloc without usin dlsym\n"));
			lt_realloc = __libc_realloc;
		}
		else
		{
			lt_realloc = (void*(*)(void*,size_t))dlsym(RTLD_NEXT, "realloc");
			TRACE((stderr, "LeakTracer: setup lt_realloc=%p\n", lt_realloc));
			if (!lt_realloc) {
				fprintf(stderr, "LeakTracer: could not resolve 'realloc' in 'libc.so': %s\n", dlerror());
				exit(1);
			}
		}
	}

	if (getenv("LEAKTRACER_ONSIG_STARTALLTHREAD"))
	{
		sigact.sa_sigaction = sigactionHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_SIGINFO;
		sigNumber = signalNumberFromString(getenv("LEAKTRACER_ONSIG_STARTALLTHREAD"));
		__sigStartAllThread = sigNumber;
		sigaction(sigNumber, &sigact, NULL);
		TRACE((stderr, "LeakTracer: registered signal %d SIGSTART for tid %d\n", sigNumber, (pid_t) syscall (SYS_gettid)));
	}

	if (getenv("LEAKTRACER_ONSIG_STOPALLTHREAD"))
	{
		sigact.sa_sigaction = sigactionHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_SIGINFO;
		sigNumber = signalNumberFromString(getenv("LEAKTRACER_ONSIG_STOPALLTHREAD"));
		__sigStopAllThread = sigNumber;
		sigaction(sigNumber, &sigact, NULL);
		TRACE((stderr, "LeakTracer: registered signal %d SIGSTOP for tid %d\n", sigNumber, (pid_t) syscall (SYS_gettid)));
	}

	if (getenv("LEAKTRACER_ONSIG_REPORT"))
	{
		sigact.sa_sigaction = sigactionHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_SIGINFO;
		sigNumber = signalNumberFromString(getenv("LEAKTRACER_ONSIG_REPORT"));
		__sigReport = sigNumber;
		sigaction(sigNumber, &sigact, NULL);
		TRACE((stderr, "LeakTracer: registered signal %d SIGREPORT for tid %d\n", sigNumber, (pid_t) syscall (SYS_gettid)));
	}

	leaktracer::MemoryTrace::InternalMonitoringDisablerThreadUp();
	__instance = new MemoryTrace();

	if (getenv("LEAKTRACER_ONSTART_STARTALLTHREAD") || getenv("LEAKTRACER_AUTO_REPORTFILENAME"))
	{
		leaktracer::MemoryTrace::GetInstance().startMonitoringAllThreads();
	}
#ifdef USE_BACKTRACE
	// we call backtrace here, because there is some init on its first call
	void *bt;
	backtrace(&bt, 1);
#endif
	leaktracer::MemoryTrace::InternalMonitoringDisablerThreadDown();
}

int MemoryTrace::Setup(void)
{
	pthread_once(&MemoryTrace::_thread_create_key_once, MemoryTrace::init_create_key);

	if (!AllMonitoringIsDisabled())
		pthread_once(&MemoryTrace::_thread_init_all_once, MemoryTrace::init_all);
			
	return 0;

}

void MemoryTrace::MemoryTraceOnInit(void)
{
	//TRACE((stderr, "LeakTracer: MemoryTrace::MemoryTraceOnInit\n"));
        leaktracer::MemoryTrace::Setup();
}


void MemoryTrace::MemoryTraceOnExit(void)
{
	if (getenv("LEAKTRACER_ONEXIT_REPORT") || getenv("LEAKTRACER_AUTO_REPORTFILENAME"))
	{
		const char *reportName;
		if ( !(reportName = getenv("LEAKTRACER_ONEXIT_REPORTFILENAME")) && !(reportName = getenv("LEAKTRACER_AUTO_REPORTFILENAME")))
		{
			TRACE((stderr, "LeakTracer: LEAKTRACER_ONEXIT_REPORTFILENAME needs to be defined when using LEAKTRACER_ONEXIT_REPORT\n"));
			return;
		}
		leaktracer::MemoryTrace::GetInstance().stopAllMonitoring();
		TRACE((stderr, "LeakTracer: writing leak report in %s\n", reportName));
		leaktracer::MemoryTrace::GetInstance().writeLeaksToFile(reportName);
	}
   
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
void MemoryTrace::writeLeaksPrivate(std::ostream &out)
{
	out << "# LeakTracer report\n";

	allocation_info_t *info;
	void *p;
	__allocations.beginIteration();
	while (__allocations.getNextPair(&info, &p)) {
                double d = info->timestamp.tv_sec + (((double)info->timestamp.tv_nsec)/1000000000);
		out << "leak, ";
		out << "time="  << std::fixed << std::left << std::setprecision(6) << d << ", "; // setw(16) ?
		out << "stack=";
		for (unsigned int i = 0; i < ALLOCATION_STACK_DEPTH; i++) {
			if (info->allocStack[i] == NULL) break;

			if (i > 0) out << ' ';
			out << info->allocStack[i];
		}
		out << ", ";

		out << "size=" << info->size << ", ";

		out << "data=";
		const char *data = reinterpret_cast<const char *>(p);
		for (unsigned int i = 0; i < PRINTED_DATA_BUFFER_SIZE && i < info->size; i++) 
			out << (isprint(data[i]) ? data[i] : '.');
		out << '\n';
	}
}


// writes all memory leaks to given stream
void MemoryTrace::writeLeaks(std::ostream &out)
{
	MutexLock lock(__allocations_mutex);
	InternalMonitoringDisablerThreadUp();

	writeLeaksPrivate(out);

	InternalMonitoringDisablerThreadDown();
}


// writes all memory leaks to given stream
void MemoryTrace::writeLeaksToFile(const char* reportFilename)
{
	MutexLock lock(__allocations_mutex);
	InternalMonitoringDisablerThreadUp();

	std::ofstream oleaks;
	oleaks.open(reportFilename, std::ios_base::out);
	if (oleaks.is_open())
	{
		writeLeaksPrivate(oleaks);
		oleaks.close();
	}
	else
	{
		std::cerr << "Failed to write to \"leaks.out\"\n";
	}
	InternalMonitoringDisablerThreadDown();
}

void MemoryTrace::clearAllocationsInfo(void)
{
	MutexLock lock(__allocations_mutex);
	__allocations.clearAllInfo();
}


}  // end namespace


