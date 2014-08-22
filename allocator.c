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
    if (memory == NULL) {

        // Convert size to a power of two if it isn't already 
        // (the smallest that is larger than the input size)
        memory_size = 2;
        while (memory_size < size) {
            memory_size *= 2;
        }    

        //initialize memory and update gobal vars
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
}

void *sal_malloc(u_int32_t n) {
    // The size of every region must be a power of two and greater than 4 bytes
    if (n <= MIN_REGION_SIZE) {
        abort();
    }

    // actual size required including header
    vsize_t memSize = n + HEADER_SIZE;    
    free_header_t *target = NULL;
    free_header_t *startpoint = (free_header_t *) (memory + free_list_ptr);
    free_header_t *curr = startpoint;

    // check the magic header to ensure memory is not corrupted
    if (curr->magic != MAGIC_FREE) {
        fprintf(stderr, "Memory corruption");
        abort();
    }
    
    // initialise target to be curr and change it if we find a smaller region as it
    // traverses the free list
    if (curr->size >= memSize) {
        target = curr;
    }
    
    // traverse the entire free list trying to find the smallest region that will
    // fit memSize
    for (curr = (free_header_t *)(memory + curr->next); curr != startpoint; 
        curr = (free_header_t *)(memory + curr->next)) {
        if (curr->magic != MAGIC_FREE) {
            fprintf(stderr, "Memory corruption");
            abort();
        }

        if ((curr->size < target->size) && (curr->size >= memSize)) {
            target = curr;
        }
    }

    void *returnValue = NULL;
    
    // only if there is a memory region that will fit memSize
    if (target != NULL) {
     
        // get index to target (from memory[])
        vlink_t targetLink = (vlink_t)((byte *)target - (byte *)memory);
        
        // continuously split the target until we get a region that is the power of 2
        // just larger than memSize
        while (target->size >= memSize * 2) {
            // target <-> split <-> after (schematic of memory after split)
            free_header_t *split = (free_header_t *) ((byte *)target + target->size);            
            free_header_t *after = (free_header_t *) (memory + target->next);
            
            target->size /= 2;
            split->magic = MAGIC_FREE;
            split->size = target->size;
            split->next = target->next;
            split->prev = targetLink;            
            
            // change after->prev to split's index
            after->prev = targetLink + target->size;
            // change target->next to split
            target->next = targetLink + target->size;
            
            // if there is one free region left before splitting
            // set target->prev to the new split region
            if (target->prev == targetLink) {
                target->prev = target->next;
            }
        }
        

        // continue only if there are more than 1 free regions after splitting
        // otherwise return default value of returnValue which is NULL
        if ((target->next != targetLink) && (target->prev != targetLink)) {
     
            target->magic = MAGIC_ALLOC;
            
            // take out the allocated region from the link by changing the links
            // before and after target
            // curr = the region just before target
            curr = (free_header_t *)(memory + target->prev);
            curr->next = target->next;
            // curr = the region just after target
            curr = (free_header_t *)(memory + target->next);  
            curr->prev = target->prev;

            if (curr == startpoint) {
                free_list_ptr = target->next;
            }

            returnValue = (void *) ((byte *)target + HEADER_SIZE); 
        }
    } 

    return returnValue;  
}

void sal_free(void *object) {
    //create a pointer to the region
    free_header_t *obj_ptr = (free_header_t *)(object - HEADER_SIZE);

    //check magic number
    if (obj_ptr->magic != MAGIC_ALLOC)
    {
        fprintf(stderr, "Attempt to free non-allocated memory");
        abort();
    }

    //search regions to be inserted in between
    free_header_t *prev = NULL;
    free_header_t *curr = (free_header_t *)(memory + free_list_ptr);

    while(curr < obj_ptr && prev < curr) {
        prev = curr;
        curr = (free_header_t *)(memory + curr->next);
    }

    if (prev == NULL){
        //insert at first position

    } else if (prev > curr){
        //insert at last position

    } else {
        //insert at between prev & curr

    }
    obj_ptr->magic = MAGIC_FREE;

    //call sal_merge()
}

void sal_merge(void *id) {
    //Merging
    //(curr's index/curr->size) odd:bottom even:top

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
    //memory = memory;
    //free_list_ptr = free_list_ptr;
    //memory_size = memory_size;
}
