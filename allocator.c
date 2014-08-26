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
#define MEMORY_START 0
#define TRUE 1
#define FALSE 0
#define BEFORE 0
#define AFTER 1

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;
typedef int boolean;
typedef int direction;

typedef struct free_list_header {
    u_int32_t magic;    // ought to contain MAGIC_FREE
    vsize_t size;       // # bytes in this block (including header)
    vlink_t next;       // memory[] index of next free block
    vlink_t prev;       // memory[] index of previous free block
} free_header_t;

void sal_merge(free_header_t *);
static direction getMergeDirection(free_header_t *, vaddr_t, vaddr_t);

// Global data
static byte *memory = NULL;      // pointer to start of suballocator memory
static vaddr_t free_list_ptr;    // index in memory[] of first block in free list
static vsize_t memory_size;      // number of bytes malloc'd in memory[]

void sal_init(u_int32_t size) {
    // Do nothing if allocator has already been initiated
    if (memory == NULL) {

        // Convert size to a power of two if it isn't already 
        // the smallest that is larger than both the input size and HEADER_SIZE
        memory_size = HEADER_SIZE;
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
    free_header_t *startpoint = (free_header_t *) (memory + free_list_ptr);
    free_header_t *curr = startpoint;
    free_header_t *target = NULL;
    
    // traverse the entire free list trying to find the smallest region that will
    // fit memSize
    do {
        if (curr->magic != MAGIC_FREE) {
            fprintf(stderr, "Memory corruption");
            abort();
        }
        if (curr->size >= memSize && (target == NULL || curr->size < target->size)) {
            target = curr;
        }
        curr = (free_header_t *)(memory + curr->next);
    } while (curr != startpoint);


    void *returnValue = NULL;
    
    // only if there is a memory region that will fit memSize
    // return NULL if (target == NULL)
    if (target != NULL) {
     
        // get index to target (from memory[])
        vaddr_t targetAddr = (vaddr_t) ((byte *)target - memory);
        
        // continuously split the target until we get a region that is the power of 2
        // just larger than memSize
        while (target->size >= memSize * 2) {
            // target <-> split <-> after (schematic of memory after split)
            target->size /= 2;
            free_header_t *split = (free_header_t *) ((byte *)target + target->size);            
            free_header_t *after = (free_header_t *) (memory + target->next);            
            
            split->magic = MAGIC_FREE;
            split->size = target->size;
            split->next = target->next;
            split->prev = targetAddr;            
            
            // change after->prev and target->next to split's index
            after->prev = targetAddr + target->size;
            target->next = targetAddr + target->size;
        }
        

        // continue only if there are more than 1 free regions after splitting
        // otherwise return default value of returnValue which is NULL
        if ((target->next != targetAddr) && (target->prev != targetAddr)) {
     
            target->magic = MAGIC_ALLOC;
            
            // take out the allocated region from the link by changing the links
            // before and after target
            // curr = the region just before target
            curr = (free_header_t *)(memory + target->prev);
            curr->next = target->next;
            // curr = the region just after target
            curr = (free_header_t *)(memory + target->next);  
            curr->prev = target->prev;
            
            // if free_list_ptr is being allocated, it needs to be changed to the next
            // available region
            if (target == startpoint) {
                free_list_ptr = target->next;
            }

            returnValue = (void *) ((byte *)target + HEADER_SIZE); 
        }
    } 
    sal_stats();
    return returnValue;  
}

void sal_free(void *object) {
    //create a pointer and index to the region
    free_header_t *objPtr = (free_header_t *) ((byte *) object - HEADER_SIZE);
    vaddr_t objAddr = ((byte *) objPtr) - memory;
    printf("%u\n", objAddr);

    //check magic number to ensure freeing valid memory
    if (objPtr->magic != MAGIC_ALLOC) {
        fprintf(stderr, "Attempt to free non-allocated memory");
        abort();
    }
    
    // traverse the list until after is the next free block after object and 
    // then set prev to be the free block before object
    free_header_t *prev = NULL;
    free_header_t *after = (free_header_t *) (memory + free_list_ptr);
    while (after <= objPtr) {
        after = (free_header_t *) (memory + after->next);
    }    
    prev = (free_header_t *) (memory + after->prev);
    
    vaddr_t prevAddr = ((byte *) prev) - memory;
    vaddr_t afterAddr = ((byte *) after) - memory;
    
    // link in the deallocated block into the free list
    prev->next = objAddr;
    after->prev = objAddr;
    objPtr->next = afterAddr;
    objPtr->prev = prevAddr;
    
    // we keep free_list_ptr to be the earliest free block so that our prevFree and afterFree
    // loops work correctly (in sal_merge and sal_free)
    // TODO: maybe rewrite to change this 
    if (objAddr < free_list_ptr) {
        free_list_ptr = objAddr;
    }
    
    // 'deallocate' the memory block
    objPtr->magic = MAGIC_FREE;

    sal_merge(objPtr);
}

// objPtr that gets passed in is the beginning of the header
void sal_merge(free_header_t *objPtr) {
    // if a merge occurs, newId will point to the finished merged block so sal_merge
    // can be called on it (if applicable) as there may be more possible merges
    void *newId = NULL;
    
    // the free blocks before and after the block to be merged
    free_header_t *prevFree = NULL;
    free_header_t *afterFree = (free_header_t *) (memory + free_list_ptr);
    
    // traverse the list until prev is the free memory block before the block that is to be merged
    // and after is the free block after (cyclically)
    while (afterFree <= objPtr) {
        afterFree = (free_header_t *) (memory + afterFree->next);
    }    
    prevFree = (free_header_t *) (memory + objPtr->prev);
    
    vaddr_t prevFreeAddr = (byte *) prevFree - memory;
    vaddr_t afterFreeAddr = (byte *) afterFree - memory;
      
    // prevFree or afterFree will only be mergeable if their size is equal to objPtr's size
    // and if they are directly adjacent in terms of memory position
    boolean prevFreeMergeable = (prevFree->size == objPtr->size) &&
        (((byte *) memory + (prevFreeAddr + prevFree->size )) == (byte *) objPtr);      
    boolean afterFreeMergeable = (afterFree->size == objPtr->size) &&
        (((byte *) memory + (afterFreeAddr - objPtr->size)) == (byte *) objPtr);
    
    if (prevFreeMergeable || afterFreeMergeable) {
    
        // the block to be merged with objPtr will be pointed by mergeTarget
        free_header_t *mergeTarget = NULL;
        direction d = getMergeDirection(objPtr, MEMORY_START, memory_size);
        
        if (afterFreeMergeable && (d == AFTER)) {            
            // therefore must merge with after
            mergeTarget = afterFree;
        } else if (prevFreeMergeable && (d == BEFORE)) {
            // therefore must merge with prev
            mergeTarget = prevFree;
        }          
        vaddr_t mergeTargetAddr = (vaddr_t) ((byte *) mergeTarget - memory);
        
        // now do merging of mergeTarget and objPtr
        // TODO: PROBABLY CAN BE PUT INTO HELPER FUNCTION / CURRENTLY VERY MESSY
        // TODO: CHECK IF THERE ARE MORE BUGS
        if (prevFree == afterFree) {
            // i.e. when the last 2 blocks merge, only one block remains in free list
            if (mergeTarget < objPtr) {
                mergeTarget->size *= 2;
                mergeTarget->next = mergeTargetAddr;
                mergeTarget->prev = mergeTargetAddr;
            } else {
                vaddr_t objAddr = (byte *) objPtr - memory;
                objPtr->size *= 2;
                objPtr->next = objAddr;
                objPtr->prev = objAddr;
                free_list_ptr = objAddr;
            }
        } else if (mergeTarget < objPtr) {
            // i.e. prevFree will be merged
            // mergeTarget/beforeFree will now the entry in the free list
            free_header_t *newBefore = (free_header_t *) (memory + prevFree->prev);
            mergeTarget->size *= 2;
            mergeTarget->next = afterFreeAddr;
            mergeTarget->prev = (vlink_t) ((byte *) newBefore - memory);
            afterFree->prev = (vlink_t) ((byte *) mergeTarget - memory);
            newBefore->next = (vlink_t) ((byte *) mergeTarget - memory);
            newId = (void *) mergeTarget;
        } else {
            // i.e. afterFree will be merged as mergeTarget > objPtr
            // objPtr will be the entry in the free list
            free_header_t *newAfter = (free_header_t *) (memory + afterFree->next);
            objPtr->size *= 2;
            objPtr->prev = prevFreeAddr;
            objPtr->next = (vlink_t) ((byte *) newAfter - memory);         
            prevFree->next = (vlink_t) ((byte *) objPtr - memory);
            newAfter->prev = (vlink_t) ((byte *) objPtr - memory);
            newId = (void *) objPtr;
            
            // change free_list_ptr to something valid as mergeTarget index will now longer
            // be the beginning of a block, newId index will
            if (mergeTargetAddr == free_list_ptr) {
                free_list_ptr = (vaddr_t) ((byte *) newId - memory);
            }
        }        
    }
    
    // recursively call sal_merge until there are no longer any merge-able blocks
    // (including when it gets merged to one contiguous block)
    if ((newId != NULL) && (((free_header_t *) memory)->size != memory_size)) {
        sal_merge(newId);
    }
}



// Returns BEFORE or AFTER depending on which objPtr needs to merge with.
// It works by recursively dividing the entire allocated memory block and seeing
// if objAddr lies on significant points (the beginning or halfway & these 
// points + objPtr->size)
static direction getMergeDirection(free_header_t *objPtr, vaddr_t begin, vaddr_t end) {
    direction d;
    vaddr_t halfway = (end - begin) / 2;
    vaddr_t objAddr = (byte *) objPtr - memory;
    if ((objAddr == begin) || (objAddr == halfway)) {
        d = AFTER;
    } else if ((objAddr == begin + objPtr->size) || (objAddr == halfway + objPtr->size)) {
        d = BEFORE;
    } else {
        // if it doesn't lie on significant areas, keep halving the bounded area
        if (objAddr < halfway) {
            // in the first half
            d = getMergeDirection(objPtr, begin, halfway);
        } else {
            // in the second half
            d = getMergeDirection(objPtr, (begin + halfway), end);
        }
    }
    return d; 
}   

void sal_end(void) {
    free(memory);
    memory = NULL;
    free_list_ptr = 0;
    memory_size = 0;
}

void sal_stats(void) {
    printf("\nsal_stats\n");
    printf("\n--Gobal variables--\n");
    printf("memory: %p\n", memory);
    printf("free_list_ptr: %u\n", free_list_ptr);
    printf("memory_size: %u\n", memory_size);

    printf("\n--Free list--\n");
    free_header_t *startpoint = (free_header_t *) (memory + free_list_ptr);
    free_header_t *curr = startpoint;
    printf("<START>\n");
    do
    {
        printf("Index-->%u size: %u, next: %u, prev: %u\n", (unsigned int) ((byte *) curr - memory), curr->size, curr->next, curr->prev);
        curr = (free_header_t *) (memory + curr->next);
    } while(curr != startpoint);
    printf("<END>\n");
}
