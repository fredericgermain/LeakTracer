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

#ifndef __MAP_MEMORY_INFO_h_included__
#define __MAP_MEMORY_INFO_h_included__

#include "ObjectsPool.hpp"


namespace leaktracer {

/**
 * Help class, holds all relevant information for each
 * allocation (logically it's a map of void* address to
 * structure with required info)
 */
template <typename T>
class TMapMemoryInfo {
public:
	TMapMemoryInfo(void);
	virtual ~TMapMemoryInfo(void) {}

	/** Allocates sizeof(T) buffer, and associates it with given
	 *  poiter */
	inline T * insert(void *ptr);

	/** Returns pointer to T object, associated with given pointer */
	inline T * find(void *ptr);

	/** Releases a buffer, associated with given pointer. */
	inline void release(void *ptr);

	/** Following 2 functions used for iteration over all
	 *  elements */
	void beginIteration(void);
	bool getNextPair(T **ppObject, void **pptr);

	void clearAllInfo(void);

private:
	// hash function from pointer to int (range is defined by POINTER_HASH_LENGTH static)
	inline unsigned long hash(void *ptr);

	// defines single pointer's info
	typedef struct _pointer_info_struct {
		void *ptr;
		T info;
	} pointer_info_t;

	// list node - to hold list of all info's having same hash value
	typedef struct _list_node_struct {
		pointer_info_t pinfo;
		struct _list_node_struct *next;
	} list_node_t;

	// array of lists (according to hash function)
#define POINTER_HASH_LENGTH				16
#define NUMBER_OF_MEMORY_INFO_LISTS		(1 << POINTER_HASH_LENGTH)
	list_node_t * __info_lists[NUMBER_OF_MEMORY_INFO_LISTS];

	// memory allocation - using a pool
#define DEFAULT_NUMBER_OF_ELEMENTS_IN_CHUNK (1 << 12)
	typedef TObjectsPool<list_node_t, DEFAULT_NUMBER_OF_ELEMENTS_IN_CHUNK> nodes_pool_t;
	nodes_pool_t __pool;

	// current position in iteration
	long __lIterationCurrentListIndex;
	list_node_t *__pIterationCurrentElement;
};


//////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION: TMapMemoryInfo
// (inline template functions)
//
//////////////////////////////////////////////////////////////////////

template <typename T>
TMapMemoryInfo<T>::TMapMemoryInfo(void)
{
	// initializes all lists to be empty
	for( int i = 0; i < NUMBER_OF_MEMORY_INFO_LISTS; i++ )
		__info_lists[i] = NULL;

	// members used for iteration
	__lIterationCurrentListIndex = -1;
	__pIterationCurrentElement = NULL;
}

template <typename T>
inline unsigned long TMapMemoryInfo<T>::hash(void *ptr)
{ return (reinterpret_cast<unsigned long>(ptr) & (NUMBER_OF_MEMORY_INFO_LISTS - 1)); }

template <typename T>
inline T * TMapMemoryInfo<T>::insert(void *ptr)
{
	list_node_t * pNew = static_cast<list_node_t*>(__pool.allocate());
	if( !pNew )
		return NULL;

	// insert to the "hash(ptr)" list
	long key = hash(ptr);
	pNew->next = __info_lists[key];
	__info_lists[key] = pNew;

	(pNew->pinfo).ptr = ptr;
	return &((pNew->pinfo).info);
}


template <typename T>
inline T * TMapMemoryInfo<T>::find(void *ptr)
{
	list_node_t * pNext = __info_lists[hash(ptr)];
	while( pNext != NULL )
	{
		if( (pNext->pinfo).ptr == ptr )
			return &((pNext->pinfo).info);
		pNext = pNext->next;
	}

	// not found
	return NULL;
}


template <typename T>
inline void TMapMemoryInfo<T>::release(void *ptr)
{
	long key = hash(ptr);
	list_node_t * pNext = __info_lists[key];
	if( NULL == pNext )
		// list is empty
		return;

	if( (pNext->pinfo).ptr == ptr )
	{
		// found: is a first element
		__info_lists[key] = pNext->next;
		__pool.release(pNext);
		return;
	}

	// searching in other elements
	list_node_t * pPrev = pNext;
	pNext = pNext->next;

	while( pNext != NULL)
	{
		if( (pNext->pinfo).ptr == ptr )
		{
			pPrev->next = pNext->next;
			__pool.release( pNext );
			return;
		}
		pPrev = pNext;
		pNext = pNext->next;
	}
}


template <typename T>
void TMapMemoryInfo<T>::beginIteration(void)
{
	__lIterationCurrentListIndex = 0;
	__pIterationCurrentElement = __info_lists[0];
}


//---------------------------------
// returns next pair (element, pointer) as output parameters
// returns false if no more elements
template <typename T>
bool TMapMemoryInfo<T>::getNextPair(T **ppObject, void **pptr)
{
	if( NULL == __pIterationCurrentElement )
	{
		// current list ended, should find next non-empty list
		while (__lIterationCurrentListIndex < NUMBER_OF_MEMORY_INFO_LISTS &&
			   NULL == __pIterationCurrentElement)
		{
			__lIterationCurrentListIndex ++;
			__pIterationCurrentElement = __info_lists[__lIterationCurrentListIndex];
		}

		if (__lIterationCurrentListIndex == NUMBER_OF_MEMORY_INFO_LISTS)
		{
			// reached the end of the lists
			*ppObject = NULL;
			*pptr = NULL;
			return false;
		}

	}

	*ppObject = &(__pIterationCurrentElement->pinfo.info);
	*pptr = __pIterationCurrentElement->pinfo.ptr;

	// advise "current element" to be the next element in current list
	__pIterationCurrentElement = __pIterationCurrentElement->next;

	return true;
}


template <typename T>
void TMapMemoryInfo<T>::clearAllInfo(void)
{
	for (long l = 0; l < NUMBER_OF_MEMORY_INFO_LISTS; l++) {
		list_node_t * pNext = __info_lists[l];
		while (pNext != NULL) {
			__info_lists[l] = pNext->next;
			__pool.release(pNext);
			pNext = __info_lists[l];
		}
	}
}


}  // end namespace


#endif  // include once
