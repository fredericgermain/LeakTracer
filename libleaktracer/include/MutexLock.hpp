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

#ifndef __LEAKTRACER_MUTEX_LOCK_h_included__
#define __LEAKTRACER_MUTEX_LOCK_h_included__

#include "Mutex.hpp"

#include <pthread.h>

namespace leaktracer {

/**
 * Used to lock <b>Mutex</b>, ensures that the mutex
 * is always released.
 *
 * Example:
 *
 * class MyClass {
 * private:
 *    Mutex my_mutex;
 *
 * public:
 *    void doSomething(void) {
 *       MutexLock lock(my_mutex);
 *       // some code
 *    }
 *    // the Mutex is released automatically by
 *    // destructor of MutexLock, even is exception
 *    // is thrown inside the body of the function.
 * };
 */
class MutexLock {
public:
	inline explicit MutexLock(Mutex& m) : __mutex(m), __locked(true) {
		pthread_mutex_lock(&__mutex.__mutex);
	}

	inline void unlock() {
		if (__locked) {
			__locked = false;
			pthread_mutex_unlock(&__mutex.__mutex);
		}
	}

	// no one should be derived from this (non-virtual destructor)
	inline ~MutexLock() {
		unlock();
	}

protected:
	Mutex& __mutex;
	bool __locked;
};


}  // end namespace

#endif  // include once
