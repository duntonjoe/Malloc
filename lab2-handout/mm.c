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
#define CHUNKSIZE (1<<24)

typedef uint64_t word;
typedef uint32_t tag;
typedef uint8_t byte;
typedef byte* address;

static address heap_head;


static inline address find_fit (uint32_t blkSize);

/*
 * get and set - read or write a word at an address (ptr)
*/
static inline tag get (tag *ptr) {
  return *ptr;
}
static inline void set (tag *ptr, tag val) {
  *ptr = val;
}

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
static inline tag* footer (address bp){
  return (tag*)(bp + WSIZE*sizeOf(header(bp)) - WSIZE);
}
/* gives the basePtr of next block */
static inline address nextBlock (address bp){
  return bp + WSIZE * sizeOf(header(bp));
}
/* gives the basePtr of prev block */
static inline address prevBlock (address bp){
  return bp - WSIZE * sizeOf(header(bp)-1);
}
/* basePtr, size, allocated */
static inline address makeBlock (address bp, uint32_t size, bool allocated) {
  *header(bp) = size | allocated;
  *footer(bp) = size | allocated;
  return bp;
}
/* basePtr — toggles alloced/free */
static inline void toggleBlock (address bp){
  *header(bp) ^= 1;
  *footer(bp) ^= 1;
}
static inline address coalesce(address bp)
{
  uint32_t size = sizeOf(header(bp));
  if (!isAllocated(footer(bp)+1)) {
    size += sizeOf(header(nextBlock(bp)));
  }
  if (!isAllocated(header(bp)-1)) {
    bp = prevBlock(bp);
    size += sizeOf(header(bp));
  }
  return makeBlock(bp, size, 0);
}

static inline address extend_heap(uint32_t words)
{
 address bp;
 uint32_t size;

 /* Allocate an even number of words to maintain alignment */
 words += (words & 1);
 size = words * WSIZE;
 if ((uint64_t)(bp = mem_sbrk((int)size)) == (uint64_t)-1)
 	return NULL;

 /* Initialize free block header/footer and the epilogue header */
 makeBlock(bp, size, 0);  
 *header(nextBlock(bp)) = 0 | true;
 
 /* Coalesce if the previous block was free */
 return coalesce(bp);
}

/*
 *PLACE - takes in a pointer and size, puts block of that size at the pointer.
*/
static inline address place(address bp, uint32_t asize){
	if(sizeOf((tag*) bp) == asize){
		toggleBlock((address)header(bp));
		toggleBlock((address)footer(bp));
	}
	else{
		uint32_t freeBlockSize = sizeOf(header(bp));
		*header(bp) = asize | 1;
		makeBlock((address)footer(bp), asize, 1);
		makeBlock((address)(footer(bp) + sizeof(tag)), (freeBlockSize - asize), 0);
		set(footer((address)footer(bp) + sizeof(tag)), (freeBlockSize - asize));
	}
	return bp;
}

int
mm_init (void)
{
  //create the initial heap	
  if ((heap_head = mem_sbrk(4*WSIZE)) == (void *)-1)
  	return -1;

  heap_head += DSIZE;
  *(header(heap_head) - 1) = 0 | true;
  *header(heap_head) = 0 | true;
  /*
   * Extend heap by 1 block of chunksize bytes.
   * Chunksize is equal to 3 words of space, as this accounts for the overhead of a header and footer word.
   */
  if(extend_heap(2) == NULL)
	  return -1;
  return 0;
}

void*
mm_malloc (uint32_t size)
{
 uint32_t asize; /* Adjusted block size */
 uint32_t extendsize; /* Amount to extend heap if no fit */
 address bp;

 /* Ignore spurious requests */
 if (size == 0)
 return NULL;

 /* Adjust block size to include overhead and alignment reqs. */
 if (size <= DSIZE)
 	asize = 2*DSIZE;
 else
	 asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

 /* Search the free list for a fit */
 if ((bp = find_fit(asize)) != NULL) {
	 place(bp, asize);
 	return bp;
 }

 /* No fit found. Get more memory and place the block */
 extendsize = (asize > CHUNKSIZE) ? (asize) : (CHUNKSIZE);
 if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
 	return NULL;
 place(bp, asize);
 return bp;

 //fprintf(stderr, "allocate block of size %u\n", size);
 //return NULL;
}

void
mm_free (void *ptr)
{
  if(isAllocated(header(ptr)))
  {
    toggleBlock((address)header(ptr));
    toggleBlock((address)footer(ptr));
  }
  //fprintf(stderr, "free block at %p\n", ptr);
}

void*
mm_realloc (void *ptr, uint32_t size)
{
  fprintf(stderr, "realloc block at %p to %u\n", ptr, size);
  return NULL;
}

/*
 *FIND FIT - 
*/
address find_fit (uint32_t blkSize) {
	for(address blockPtr = heap_head + (2 * sizeof(tag)); sizeOf((tag*)blockPtr) != 0; blockPtr = nextBlock(blockPtr))
	{
		if((!isAllocated((tag*)blockPtr)) && (sizeOf((tag*)blockPtr) >= blkSize))
		{
			return blockPtr;	
		}
	}
	return extend_heap(blkSize);
 }

