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


int
mm_init (void)
{
  if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
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
