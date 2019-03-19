// Brought to you by Joe Dunton and James Leo Roche IV 

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "memlib.h"
#include "mm.h"

#define ALIGNMENT 16
#define WSIZE 8
#define DSIZE 16
#define OVERHEAD (2 * sizeof(word))
#define CHUNKSIZE (1<<24)
#define MIN_BLOCK_SIZE 4

typedef uint64_t word;
typedef uint32_t tag;
typedef uint8_t byte;
typedef byte* address;


// We set this up to represent the start of the heap but also
// as a way to grab the dummy header of our free list
static address free_list_head;


static inline address find_fit (uint32_t blkSize);

static inline uint32_t sizeOf (tag* base) {
  return *base & (uint32_t)-2;
}

static inline bool isAllocated (tag* base) {
  return *base & (uint32_t)1;
}

static inline tag* header (address base) {
  return (tag*)base - 1;
}

static inline tag* footer (address base) {
  return (tag*)(base + sizeOf (header (base)) * sizeof (word) - 2 * sizeof (tag));
}

static inline address nextBlock (address base) {
  return base + sizeOf (header(base)) * sizeof (word);
}

static inline tag* prevFooter (address base) {
  return header(base) - 1;
}

static inline tag* nextHeader (address base) {
  return footer(base) + 1;
}

static inline address prevBlock (address base) {
  return base - sizeOf (prevFooter (base)) * sizeof(word);
}

static inline address* nextPtr (address base) {
  return (address*)base;
}

static inline address* prevPtr (address base) {
  return (address*)base + 1;
}

/* Adds a node to the free list */
static inline void addNode(address bp){
	address prev = free_list_head;
	address next = *nextPtr(prev);
	*nextPtr(bp) = next;
	*prevPtr(bp) = prev;
	*prevPtr(next) = bp;
	*nextPtr(prev) = bp;
}

/* Removes a node from the free list */
static inline void removeNode (address bp){
	*nextPtr(*prevPtr(bp)) = *nextPtr(bp);
	*prevPtr(*nextPtr(bp)) = *prevPtr(bp);
}

/*basePtr, size, allocated */
static inline address makeBlock (address bp, uint32_t size, bool allocated) {
	*header(bp) = size | allocated;
	*footer(bp) = size | allocated;
	if (!allocated) {
		addNode (bp);
	}
	return bp;
}

/* basePtr — toggles alloced/free */
static inline void toggleBlock (address bp){
	*header(bp) ^= 1;
	*footer(bp) ^= 1;
}

/*
 *  Coalesce - merge free blocks with main memory pool
 */
static inline address coalesce(address bp)
{
	uint32_t size = sizeOf(header(bp));
	address base = bp;
	if (!isAllocated(nextHeader(bp))) {
		size += sizeOf(nextHeader(bp));
		removeNode(nextBlock(bp));
	}
	if (!isAllocated(prevFooter(bp))) {
		size += sizeOf(prevFooter(bp));
		removeNode(prevBlock(bp));
		base = prevBlock(bp);
	}
	if (size != sizeOf(header(bp))) {
		removeNode(bp);
		makeBlock(base, size, false);
	}
	return base;
}

/*
 * blocksFromBytes - ensures that all blocks are aligned to 16 bytes 
 * 		bytes -> represents the number of bytes requested by the application
 */
static inline uint32_t blocksFromBytes (uint32_t bytes) {
	// The remainder of the requested space must be equal to 8 bytes because of the positioning of
	// the footer and proceding header to reach the next payload
	uint32_t size = (uint32_t) ((bytes + 2*sizeof(tag) + DSIZE - 1) / DSIZE )* 2;
	if (size < MIN_BLOCK_SIZE)
		return MIN_BLOCK_SIZE;
	return size;
}

/*
 * extend_heap - grows the heap by a given word size to accomodate new blocks 
 */
static inline address extend_heap(uint32_t words)
{
	words += (words & 1);
	uint32_t size = words * WSIZE;
	address bp = mem_sbrk ((int)size);
	if ((uint64_t)bp == (uint64_t)-1)
		return NULL;
	/* Initialize free block header/footer and the epilogue header */
	makeBlock (bp, words, false); 
	*header (nextBlock (bp)) = 0 | true;
	/* Coalesce if the previous block was free */
	return coalesce (bp);
}

/*
 *PLACE - takes in a pointer and size, puts block of that size at the pointer.
 */
static inline address place(address bp, uint32_t asize)
{
	uint32_t csize = sizeOf(header(bp));
	removeNode (bp);
	if (csize - asize >= MIN_BLOCK_SIZE) {
		makeBlock (bp, asize, true);
		makeBlock (nextBlock (bp), csize - asize, false);
	} else {
		makeBlock (bp, csize, true);
	}
	return bp;
}

/*
 *Find_fit - finds first available spot where a new block could fit
 */
static inline address find_fit (uint32_t blkSize) {
	for(address blockPtr = *nextPtr(free_list_head); blockPtr != free_list_head; blockPtr = *nextPtr(blockPtr))
	{
		if(sizeOf(header(blockPtr)) >= blkSize)
		{
			return blockPtr;
		}
	}
	return extend_heap(blkSize);
}

int
mm_init (void)
{
	address heap_head;
	//create the initial heap	
	if ((heap_head = mem_sbrk(6*WSIZE)) == (void *)-1)
		return -1;
	// setuo a buffer
	free_list_head = heap_head + 2 * WSIZE;
	// we make a dummy header and store it between a dummy header and footer of size
	// 4 with allocation. We set it this way so that we don't have to worry about the
	// head of the heap ever being overwritten with a payload.
	makeBlock(free_list_head, 4, true);
	// Set the epilogue header. 
	*header(nextBlock(free_list_head)) = 0 | 1;
	// Setup the doubly linked list which points to itself.
	*prevPtr(free_list_head) = free_list_head;
	*nextPtr(free_list_head) = free_list_head;
	/*
	 * Extend heap by 1 block of chunksize bytes.
	 * Chunksize is equal to 3 words of space, as this accounts for the overhead of a header and footer word.
	 */
	return 0;
}

void*
mm_malloc (uint32_t size)
{
	if (size == 0) {
		return NULL;
	}
	uint32_t asize = blocksFromBytes(size);
	address bp = find_fit(asize);
	if (bp != NULL) {
		place(bp, asize);
	}
	return bp;
}

/* We need to add the node to the freed block in this
   implementation to make sure our list contains everything
   that's been freed. */
void
mm_free (void *ptr)
{
	toggleBlock((address)ptr);
	addNode(ptr);
	coalesce ((address)ptr);
}

void*
mm_realloc (void *ptr, uint32_t size)
{
	if (ptr == NULL) {
		return mm_malloc (size);
	}
	if (size == 0) {
		mm_free(ptr);
		return NULL;
	}
	address bp = (address)ptr;
	const uint32_t newBlocks = blocksFromBytes (size);
	const uint32_t oldBlocks = sizeOf(header((address)ptr));
	const uint32_t payload = (uint32_t)(oldBlocks * sizeof(word) - 2 * sizeof(tag));
	if (newBlocks == oldBlocks) {
		return ptr;
	}
	if (newBlocks < oldBlocks) {
		if (oldBlocks - newBlocks >= 2) {
			makeBlock (bp, newBlocks, true);
			makeBlock (nextBlock (bp), oldBlocks - newBlocks, false);
		}
		return ptr;
	}
	address newPtr = mm_malloc(size);
	memcpy (newPtr, ptr, payload);
	mm_free (ptr);
	return newPtr;
}

int mm_check(void)
{
	// Heap head isn't set properly
	if (free_list_head == NULL)
		return 0;
	for (address blockptr = free_list_head; sizeOf(header(blockptr)) != 0; blockptr = nextBlock(blockptr)) {
		// The header and fooder have different allocation bits
		if (!isAllocated(header(blockptr)) != !isAllocated(footer(blockptr)))
			return 0;
		// The header and footer have different sizes
		if (sizeOf(header(blockptr)) != sizeOf(footer(blockptr)))
			return 0;
		// The two in a row are not allocated, you missed a coalesce
		if (!isAllocated(header(blockptr)) && !isAllocated(nextHeader(blockptr)))
			return 0;
	}
	// Checks to see if all the items on the free list are actually free.
	for(address ptr = *nextPtr(free_list_head); ptr != free_list_head; ptr = *nextPtr(ptr)) {
		if (isAllocated(header(ptr)))
			return 0;
	}
	return 1;
}
