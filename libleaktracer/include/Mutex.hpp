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

#ifndef __LEAKTRACER_MUTEX_h_included__
#define __LEAKTRACER_MUTEX_h_included__

#include <pthread.h>
#include <stdio.h>

#ifndef TRACE
#ifdef LOGGER
#define TRACE(arg) fprintf arg
#else
#define TRACE(arg)
#endif
#endif

namespace leaktracer {

// forward declaration
class MutexLock;

/**
 * Wrapper to "pthread_mutex_t", should be used with
 * <b>MutexLock</b> class, which insures that the mutex is
 * released even in case of exception.
 */
class Mutex { // not intended to be overridden, non-virtual destructor
public:
	inline Mutex() {
//		TRACE((stderr, "LeakTracer: Mutex()\n"));
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
//		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&__mutex, &attr);
		pthread_mutexattr_destroy(&attr);
	}

	// not intended to be overridden, non-virtual destructor
	inline ~Mutex() {
//		TRACE((stderr, "LeakTracer: ~Mutex()\n"));
		pthread_mutex_destroy(&__mutex);
	}

	pthread_mutex_t	__mutex;

protected:
	friend class MutexLock;
};

}  // end namespace


#endif  // include once
