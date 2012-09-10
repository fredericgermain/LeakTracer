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

#ifndef __OBJECTS_POOL_h_included__
#define __OBJECTS_POOL_h_included__

//////////////////////////////////////////////////////////
// Objects' pool implementation
//
// Concept taken from "Pool" class, C++ Boost:
// http://www.boost.org/libs/pool/doc/concepts.html
//////////////////////////////////////////////////////////


#include "Mutex.hpp"
#include "MutexLock.hpp"
#include <cstdlib>

/**
 * dynamic call interfaces to memory allocation functions in libc.so
 */
extern void* (*lt_malloc)(size_t size);
extern void  (*lt_free)(void* ptr);
extern void* (*lt_realloc)(void *ptr, size_t size);
extern void* (*lt_calloc)(size_t nmemb, size_t size);

/*
 * underlying allocation, de-allocation used within
 * this tool
 */
#define LT_MALLOC  (*lt_malloc)
#define LT_FREE    (*lt_free)
#define LT_REALLOC (*lt_realloc)
#define LT_CALLOC  (*lt_calloc)

namespace leaktracer {


/**
 * This class used for allocation of chunks of elements
 * of type E on the heap
 */
template <typename E, unsigned int NumOfElementsInChunk>
class TDefaultChunkAllocator
{
public:
	E * allocate()
	{
		return (E*) LT_MALLOC( NumOfElementsInChunk * sizeof(E) );
	}

	void release( E * p )
	{
		LT_FREE(p);
	}
};


//---------------------------------
// type for list element, which may be
// a data used by the client, or a pointer
// to next free cell, when the cell is free
template<typename T>
struct t_list_element
{
	union {
		unsigned char data[sizeof(T)];
		t_list_element * next_free_cell;
	};
};



#define FREE_CELL_NONE		NULL

template <typename T,
	unsigned int NumOfElementsInChunk,
	bool IsThreadSafe = true,
	typename CHUNK_ALLOCATOR = TDefaultChunkAllocator< t_list_element<T>, NumOfElementsInChunk > >
class TObjectsPool
{
public:

	//---------------------------------
	// Returns a pointer to free object
	// NOTE: no constructor is executed
	void * allocate();


	//---------------------------------
	// Releases object when unused
	void release( void *p );


	//---------------------------------
	// constructor
	TObjectsPool();


	//---------------------------------
	// statistics
	unsigned long getNumOfObjects();
	unsigned long getNumOfChunks();


private:
	// internal allocate/release functions
	void * allocate_unlocked();
	void release_unlocked( void *p );

	//---------------------------------
	// initializes the "list"
	void initializeList( t_list_element<T> *pChunk );

	// the memory allocator function
	CHUNK_ALLOCATOR __allocator;

	// pointer to the first free element
	t_list_element<T> *__first_free_cell;

	// statistics
	unsigned long __num_of_objects;
	unsigned long __num_of_chunks;

	// synchnonization
	Mutex __mutex;
};



//======================================================
// Implementation
//======================================================

//---------------------------------
// constructor
template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::TObjectsPool()
: __first_free_cell( FREE_CELL_NONE ), __num_of_objects(0), __num_of_chunks(0)
{
# if 0
	// allocate the first block
	t_list_element<T> *pBlock = __allocator.allocate();
	if( NULL != pBlock ) {
		initializeList( pBlock );
		__num_of_chunks ++;
	}
#endif
}


//---------------------------------
// initializing linked list of free cells
template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
void TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::initializeList( t_list_element<T> *pChunk )
{
	for( unsigned int i = 0; i < NumOfElementsInChunk; i++ )
		pChunk[i].next_free_cell = pChunk + i + 1;

	pChunk[NumOfElementsInChunk - 1].next_free_cell = __first_free_cell;
	__first_free_cell = pChunk;
}


//---------------------------------
// allocating objects
template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
void * TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::allocate_unlocked()
{
	if( FREE_CELL_NONE == __first_free_cell )
	{
		// try to allocate additional block
		t_list_element<T> *pNewBlock = __allocator.allocate();
		if( NULL != pNewBlock ) {
			initializeList( pNewBlock );
			__num_of_chunks ++;
		}

		if( FREE_CELL_NONE == __first_free_cell )
			return NULL;
	}

	void* retVal = static_cast<void*>( &(__first_free_cell->data) );
	__first_free_cell = __first_free_cell->next_free_cell;
	__num_of_objects ++;
	return retVal;
}

template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
void * TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::allocate()
{
	if (! IsThreadSafe)
		return allocate_unlocked();

	MutexLock lock(__mutex);
	return allocate_unlocked();
}


//---------------------------------
// ideallocating objects
template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
void TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::release_unlocked( void *p )
{
	t_list_element<T> *pReleased = reinterpret_cast<t_list_element<T> *>( p );
	pReleased->next_free_cell = __first_free_cell;
	__first_free_cell = pReleased;

	if( __num_of_objects != 0 )
		__num_of_objects --;
}

template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
void TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::release( void *p )
{
	if( p == NULL ) return;

	if (! IsThreadSafe) {
		release_unlocked(p);
	} else {
		MutexLock lock(__mutex);
		release_unlocked(p);
	}
}


// Statistics
template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
unsigned long TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::getNumOfObjects()
{
	return __num_of_objects;
}

template <typename T, unsigned int NumOfElementsInChunk, bool IsThreadSafe, typename CHUNK_ALLOCATOR>
unsigned long TObjectsPool<T, NumOfElementsInChunk, IsThreadSafe, CHUNK_ALLOCATOR>::getNumOfChunks()
{
	return __num_of_chunks;
}


}; // namespace


#endif  // include once
