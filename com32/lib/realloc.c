/*
 * realloc.c
 */

#include <stdlib.h>
#include <string.h>
#include <minmax.h>

#include "malloc.h"

void *realloc(void *ptr, size_t size)
{
  struct free_arena_header *ah, *nah;
  void *newptr;
  size_t newsize, oldsize, xsize;

  if ( !ptr )
    return malloc(size);

  if ( size == 0 ) {
    free(ptr);
    return NULL;
  }

  ah = (struct free_arena_header *)
    ((struct arena_header *)ptr - 1);

  /* Actual size of the old block */
  oldsize = ah->a.size;

  /* Add the obligatory arena header, and round up */
  newsize = (size+2*sizeof(struct arena_header)-1) & ARENA_SIZE_MASK;

  if ( oldsize >= newsize && newsize >= (oldsize >> 2) ) {
    /* This allocation is close enough already. */
    return ptr;
  } else {
    xsize = oldsize;

    nah = ah->a.next;
    if ((char *)nah == (char *)ah + ah->a.size &&
	nah->a.type == ARENA_TYPE_FREE &&
	oldsize + nah->a.size >= newsize) {
      /* Merge in subsequent free block */
      ah->a.next = nah->a.next;
      ah->a.next->a.prev = ah;
      nah->next_free->prev_free = nah->prev_free;
      nah->prev_free->next_free = nah->next_free;
      xsize = (ah->a.size += nah->a.size);
    }

    if (xsize >= newsize) {
      /* We can reallocate in place */
      if (xsize >= newsize + 2*sizeof(struct arena_header)) {
	/* Residual free block at end */
	nah = (struct free_arena_header *)((char *)ah + newsize);
	nah->a.type = ARENA_TYPE_FREE;
	nah->a.size = xsize - newsize;
	ah->a.size = newsize;

	/* Insert into block list */
	nah->a.next = ah->a.next;
	ah->a.next = nah;
	nah->a.next->a.prev = nah;
	nah->a.prev = ah;

	/* Insert into free list */
	nah->next_free = __malloc_head.next_free;
	nah->prev_free = &__malloc_head;
	__malloc_head.next_free = nah;
	nah->next_free->prev_free = nah;
      }
      /* otherwise, use up the whole block */
      return ptr;
    } else {
      /* Last resort: need to allocate a new block and copy */
      oldsize -= sizeof(struct arena_header);
      newptr = malloc(size);
      if (newptr) {
	memcpy(newptr, ptr, min(size,oldsize));
	free(ptr);
      }
      return newptr;
    }
  }
}
