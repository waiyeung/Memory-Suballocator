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

#define HEADER_SIZE sizeof(struct free_list_header)  
#define MAGIC_FREE  0xDEADBEEF
#define MAGIC_ALLOC 0xBEEFDEAD
#define MIN_REGION_SIZE 4
#define FREE_PTR_START 0

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
    u_int32_t magic;    // ought to contain MAGIC_FREE
    vsize_t size;       // # bytes in this block (including header)
    vlink_t next;       // memory[] index of next free block
    vlink_t prev;       // memory[] index of previous free block
} free_header_t;

// Global data
static byte *memory = NULL;      // pointer to start of suballocator memory
static vaddr_t free_list_ptr;    // index in memory[] of first block in free list
static vsize_t memory_size;      // number of bytes malloc'd in memory[]


void sal_init(u_int32_t size) {
    // Do nothing if allocator has already been initiated
    if (memory != NULL) {
        return;
    }
    
    // Convert size to a power of two if it isn't already (the smallest that is larger than the input size)
    memory_size = 2;
    while (memory_size < size) {
        memory_size *= 2;
    }    

    free_list_ptr = FREE_PTR_START;

    memory = malloc(memory_size);
    if (memory == NULL) {
        fprintf(stderr, "sal_init: insufficient memory");
        abort();
    }

    // Create the memory header
    free_header_t *header = (free_header_t *) memory;
    header->magic = MAGIC_FREE;
    header->size = memory_size;
    header->next = FREE_PTR_START;
    header->prev = FREE_PTR_START;
}

void *sal_malloc(u_int32_t n) {
    // The size of every region must be a power of two and greater than 4 bytes
    if (n <= MIN_REGION_SIZE) {
        abort();
    }

    // search for appropriate minimum free region
    vsize_t N = n + HEADER_SIZE;
    free_header_t *target = NULL;
    free_header_t *startpoint = (free_header_t *) (memory + free_list_ptr);
    free_header_t *curr = startpoint;

    if (curr->magic != MAGIC_FREE) {
        fprintf(stderr, "Memory corruption");
        abort();
    }

    if (curr->size >= N) {
        target = curr;
    }

    curr = (free_header_t *)(memory + curr->next);
    while (curr != startpoint) {
        if (curr->magic != MAGIC_FREE) {
            fprintf(stderr, "Memory corruption");
            abort();
        }

        if ((curr->size < target->size) && (curr->size >= N)) {
            target = curr;
        }

        curr = (free_header_t *)(memory + curr->next);
    }

    // return NULL if there is not enough space
    if (target == NULL) {
        return NULL;
    }
     
    //spilt target to appropriate size
    vlink_t targetLink = (vlink_t)((void *)target - free_list_ptr);
    free_header_t *split;
    while (target->size >= N * 2) {
        target->size /= 2;
        split = (free_header_t *)((byte *)target + target->size);
        split->magic = MAGIC_FREE;
        split->size = target->size;
        split->next = target->next;
        split->prev = targetLink;
        target->next = targetLink + target->size;
        if (target->prev == targetLink) {
            target->prev = target->next;
        }
    }

    // If there is only one free region left then return null
    if (target->next == targetLink) {
        return NULL;
    }
     
    target->magic = MAGIC_ALLOC;

    //curr = the region just before target
    curr = (free_header_t *)(memory + target->prev);
    curr->next = target->next;

    //curr = the region just after target
    curr = (free_header_t *)(memory + target->next);  
    curr->prev = target->prev;

    if (curr == startpoint) {
        free_list_ptr = target->next;
    }

    return ((void *) (target + HEADER_SIZE));    
}

void sal_free(void *object) {
    // TODO
}

void sal_end(void) {
    free(memory);
    memory = NULL;
    free_list_ptr = 0;
    memory_size = 0;
}

void sal_stats(void) {
    // Optional, but useful
    printf("sal_stats\n");
    // we "use" the global variables here
    // just to keep the compiler quiet
    memory = memory;
    free_list_ptr = free_list_ptr;
    memory_size = memory_size;
}
