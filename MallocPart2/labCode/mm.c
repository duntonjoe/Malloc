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
    size += sizeOf(footer(bp)+1);
  }
  if (!isAllocated(header(bp)-1)) {
    bp = prevBlock(bp);
    size += sizeOf(header(bp));
  }
  return makeBlock(bp, size, false);
}

static inline uint32_t blocksFromBytes (uint32_t bytes) {
  return (uint32_t)((bytes + 2 * sizeof(tag) + DSIZE - 1) / DSIZE) * 2;
}

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

static inline address find_fit (uint32_t blkSize) {
  for(address blockPtr = heap_head; sizeOf(header(blockPtr)) != 0; blockPtr = nextBlock(blockPtr))
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
  if ((heap_head = mem_sbrk(2*WSIZE)) == (void *)-1)
  	return -1;
  
  heap_head += 2 * WSIZE;
  *(header(heap_head) - 1) = 0 | true;
  *header(heap_head) = 0 | true;
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
  uint32_t asize = blocksFromBytes (size);
  address bp = find_fit (asize);
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
