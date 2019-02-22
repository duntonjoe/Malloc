#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static char *heap_head;

/*
 * get and set - read or write a word at an address (ptr)
*/
static inline unsigned int get (void *ptr) {
	return (*(unsigned int *) ptr);
}
static inline void set (void *ptr, unsigned int val) {
	(*(unsigned int *) ptr) = (val);
}

/* returns HDR address given basePtr */
static inline tag* header (address bp){
	return (bp - sizeof(tag))
}
/* return true IFF block is allocated */
static inline bool isAllocated (address targetBlockAddr){
	return targetBlockAddr & 0x1;
}
/* returns size of block (words) */
static inline uint32_t sizeOf (address targetBlockAddr){
	return targetBlockAddr & ~0x2;
}
/* returns FTR address given basePtr */
static inline tag* footer (address bp){
	return (bp + *(bp - sizeof(tag)) - (WSIZE);
}
/* gives the basePtr of next block */
static inline address nextBlock (address bp){
	return footer(bp) + WSIZE;
}
/* gives the basePtr of prev block */
static inline address prevBlock (address bp){
	return header(bp) - WSIZE;
}
/* basePtr, size, allocated */
void makeBlock (address bp, uint32_t size, bool allocated){
	set(bp, (size | allocated));
	set(footer(bp), (size | allocated));
}
/* basePtr — toggles alloced/free */
void
toggleBlock (address targetAddr){
	return targetAddr ^ 0x1;
}

int
mm_init (void)
{
  //create the initial heap	
  if ((heap_head = mem_sbrk(4*WSIZE)) == (void *)-1)
  	return -1;
  set(heap_head, 0);
  set(heap_head + (1*WSIZE), (unsigned int) 1);		//prologue header
  set(heap_head + (2*WSIZE), (unsigned int) 2);		//prologue footer
  set(heap_head + (3*WSIZE), (unsigned int) 3);		//epilogue header

  heap_head += (2*WSIZE);				//put pointer after prologue footer, where data will go

  /*
   * Extend heap by 1 block of chunksize bytes.
   * Chunksize is equal to 3 words of space, as this accounts for the overhead of a header and footer word.
   */
  if(extendHeap(CHUNKSIZE/WSIZE) == NULL)
	  return -1;
  return 0;
}

void*
mm_malloc (uint32_t size)
{
  fprintf(stderr, "allocate block of size %u\n", size);
  return NULL;
}

void
mm_free (void *ptr)
{
  fprintf(stderr, "free block at %p\n", ptr);
}

void*
mm_realloc (void *ptr, uint32_t size)
{
  fprintf(stderr, "realloc block at %p to %u\n", ptr, size);
  return NULL;
}

