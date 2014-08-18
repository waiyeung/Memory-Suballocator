//
//  COMP1927 Assignment 1 - Memory Suballocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014
//  Copyright (c) 2012-2014 UNSW. All rights reserved.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HEADER_SIZE    sizeof(struct free_list_header)  
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
   u_int32_t magic;           // ought to contain MAGIC_FREE
   vsize_t size;              // # bytes in this block (including header)
   vlink_t next;              // memory[] index of next free block
   vlink_t prev;              // memory[] index of previous free block
} free_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of suballocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]


void sal_init(u_int32_t size)
{
	if(memory != NULL) break;

   memory_size = 8;
   while (memory_size < size) memory_size *= 2;

   free_list_ptr = 0;

   memory = malloc(memory_size);
   if (memory == NULL)
   {
      fprintf(stderr, "sal_init: insufficient memory");
      abort();
   }

   free_header_t *newHeader = memory;
   newHeader->magic = MAGIC_FREE;
   newHeader->size = memory_size;
   newHeader->next = 0;
   newHeader->prev = 0;
}

void *sal_malloc(u_int32_t n)
{
	//search for minimum suitable free region
	if ((memory+free_list_ptr)->magic != MAGIC_FREE)
   {
   		fprintf(stderr, "Memory corruption");
			abort();
   }

	n += HEADER_SIZE;

	vaddr_t min = -1;
	if ((memory+free_list_ptr)->size >= n)
   {
   		min = free_list_ptr;
   }

	vaddr_t curr = (memory+free_list_ptr)->next;
	while(curr != free_list_ptr)
	{
		if ((memory+curr)->magic != MAGIC_FREE)
   	{
   		fprintf(stderr, "Memory corruption");
			abort();
   	}
   	if ((memory+curr)->size <= (memory+min)->size && (memory+curr)->size >= n)
   	{
   		min = curr;
   	}
   	curr = (memory+curr)->next;
	}

	//split regions
	if (min == -1) return NULL; else
	{
		vsize_t currSize = (memory+min)->size;
		vaddr_t prev = min;
		while(currSize >= n * 2)
		{
			currSize /= 2;
			(memory+min)->size = currSize;
			free_header_t *newHeader = (memory + min + currSize);
			newHeader->magic = MAGIC_FREE;
   		newHeader->size = currSize;
  			newHeader->next = min;
   		newHeader->prev = min;
		}
		if ((memory+curr)->next == curr) return NULL; else
		{
			(memory+curr)->magic = MAGIC_ALLOC;
			return ((void*)(memory + curr + HEADER_SIZE));
		}
	}
   
}

void sal_free(void *object)
{
   // TODO
}

void sal_end(void)
{
   free(memory);
   memory = NULL;
   free_list_ptr = 0;
   memory_size = 0;
}

void sal_stats(void)
{
   // Optional, but useful
   printf("sal_stats\n");
    // we "use" the global variables here
    // just to keep the compiler quiet
   memory = memory;
   free_list_ptr = free_list_ptr;
   memory_size = memory_size;
}
