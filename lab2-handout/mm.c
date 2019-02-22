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

static char *heap_head;

static inline unsigned int get (char *ptr) {
	return (*(unsigned int *) ptr);
}

static inline void set (char *ptr, unsigned int val) {
	(*(unsigned int *) ptr) = (val);
}

int
mm_init (void)
{
  if ((heap_head = mem_sbrk(4*WSIZE)) == (void *)-1)
  	return -1;
  set(heap_head, 0);
  set(heap_head + (1*WSIZE), (unsigned int) 1);		//prologue header
  set(heap_head + (2*WSIZE), (unsigned int) 2);		//prologue footer
  set(heap_head + (3*WSIZE), (unsigned int) 3);		//epilogue header
  heap_head += (2*WSIZE);				//put pointer after prologue footer, where data will go

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

