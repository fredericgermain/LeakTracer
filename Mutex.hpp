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

#ifndef __LEAKTRACE_MUTEX_h_included__
#define __LEAKTRACE_MUTEX_h_included__

#include <pthread.h>

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
    inline Mutex() { pthread_mutex_init(&__mutex, NULL); }

	// not intended to be overridden, non-virtual destructor
    inline ~Mutex() { pthread_mutex_destroy(&__mutex); }

protected:
	friend class MutexLock;

	pthread_mutex_t	__mutex;
};

}  // end namespace

#endif  // include once
