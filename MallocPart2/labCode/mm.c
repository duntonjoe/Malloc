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

typedef uint64_t word;
typedef uint32_t tag;
typedef uint8_t byte;
typedef byte* address;

static address heap_head;
static address free_list_head;


static inline address find_fit (uint32_t blkSize);

/* returns HDR address given basePtr */
static inline tag* header (address bp){
	return (tag*)bp - 1;
}

/* return true IFF block is allocated */
static inline bool isAllocated (tag* targetBlockAddr){
	return *targetBlockAddr & 0x1;
}

/* returns size of block (words) */
static inline tag sizeOf (tag* targetBlockAddr){
	return *targetBlockAddr & (tag)-2;
}

/* returns FTR address given basePtr */
static inline tag* footer (address bp) {
	return (tag*)(bp + WSIZE*sizeOf(header(bp)) - WSIZE);
}

/* gives the basePtr of next block */
static inline address nextBlock (address bp){
	return bp + WSIZE * sizeOf(header(bp));
}

/*returns the previous block's footer */
static inline tag* prevFooter (address base){
	return header(base) - 1;
}

/* gives the basePtr of prev block */
static inline address prevBlock (address bp){
	return (address) ((prevFooter(bp) - sizeOf(prevFooter(bp))) + (2 * WSIZE)); 
}

/*returns the next block's header */
static inline tag* nextHeader (address base){
	return footer(base) + 1;
}

/*returns next pointer of a block */
static inline address* nextPtr (address base){
	return (address*)base;
}

/*returns prev pointer of a block */
static inline address* prevPtr (address base){
	return (address*)base + 1;
}

static inline void addNode(address p){
	address prev = free_list_head;
	address next = *nextPtr(prev);
	*nextPtr(p) = next;
	*prevPtr(p) = prev;
	*prevPtr(next) = p;
	*nextPtr(prev) = p;
}

static inline void removeNode (address p){
	*nextPtr(*prevPtr(p)) = *nextPtr(p);
	*prevPtr(*nextPtr(p)) = *prevPtr(p);
}

/*basePtr, size, allocated */
static inline address makeBlock (address bp, uint32_t size, bool allocated) {
	*header(bp) = (tag) (size + OVERHEAD) | allocated;
	*footer(bp) = (tag) (size + OVERHEAD) | allocated;
	*((address*)header(bp) + sizeof(tag))         = nextBlock(bp);
	*((address*)header(bp) + sizeof(tag) + WSIZE) = prevBlock(bp);
	return bp;
}

/* basePtr — toggles alloced/free */
static inline void toggleBlock (address bp){
	*header(bp) ^= 1;
	*footer(bp) ^= 1;
}

/* 
 * removeBlock - removes a block from the free list 
 */
static inline void removeBlock(address bp){
	if(prevPtr(bp)){
		*((address*)prevBlock(bp) - OVERHEAD) = nextBlock(bp);
	}
	else {
		free_list_head = *nextPtr(bp);
	}
	*((address*)nextBlock(bp) - OVERHEAD) = prevBlock(bp);
}

/* 
 * insertBlock - inserts a block into the free list 
 */
static inline void insertBlock(address bp){
	*((address*)header(bp) + 1) = free_list_head;
	*((address*)header(bp) + 1) = NULL;
	*((address*)header(free_list_head) + 2) = bp;
	free_list_head = bp;
}

/*
 *  Coalesce - merge free blocks with main memory pool
 */
static inline address coalesce(address bp)
{
	uint32_t size = sizeOf(header(bp));

	if (!isAllocated(nextHeader(bp))) {
		size += sizeOf(nextHeader(bp));
		removeNode(nextBlock(bp));
	}
	if (!isAllocated(prevFooter(bp))) {
		size += sizeOf(header(bp));
		bp = prevBlock(bp);
		removeNode(prevBlock(bp));
	}
	*header(bp) = size | false;
	*footer(bp) = size | false;
	addNode(bp);
	return makeBlock(bp, size, false);
}

/*
 * blocksFromBytes - ensures that all blocks are aligned to 16 bytes 
 * 		bytes -> represents the number of bytes requested by the application
 */
static inline uint32_t blocksFromBytes (uint32_t bytes) {
	// The remainder of the requested space must be equal to 8 bytes because of the positioning of
	// the footer and proceding header to reach the next payload
	uint32_t modulus = bytes % 16;
	if(modulus != 0 && modulus < 9) {
		return (bytes - modulus + 8);
	}
	return (bytes - modulus + 16);
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
	if (csize - asize >= 2) {
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
	for(address blockPtr = free_list_head; sizeOf(nextHeader((blockPtr))) != 0; blockPtr = nextBlock(blockPtr))
	{
		if(!isAllocated(header(blockPtr)) && sizeOf(header(blockPtr)) >= blkSize)
		{
			return blockPtr;
		}
	}
	return extend_heap(blkSize);
}

int
mm_init (void)
{
	//create the initial heap	
	if ((heap_head = mem_sbrk(6*WSIZE)) == (void *)-1)
		return -1;

	heap_head += 2 * WSIZE;
	makeBlock(heap_head, 4, true);
	*header(nextBlock(heap_head)) = 0 | 1;
	free_list_head = heap_head;
	*prevPtr(heap_head) = heap_head;
	*nextPtr(heap_head) = heap_head;	
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
	if (bp == NULL) {
		return NULL;
	}
	place(bp, asize);
	return bp;
}

void
mm_free (void *ptr)
{
	toggleBlock((address)ptr);
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
	for (address blockptr = heap_head; sizeOf(header(blockptr)) != 0; blockptr = nextBlock(blockptr)) {
		if (!isAllocated(header(blockptr))) {
			if (!isAllocated(header(nextBlock(blockptr)))) {
				// If it reaches this point, it's missed a coalesce.
				return 0;
			}
		}      
	}
	return 1;
}
