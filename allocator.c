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
#define TRUE 1
#define FALSE 0
#define BEFORE 0
#define AFTER 1

typedef unsigned char byte;
typedef uint32_t vlink_t;
typedef uint32_t vsize_t;
typedef uint32_t vaddr_t;
typedef int boolean;

typedef struct free_list_header {
    uint32_t magic;    // ought to contain MAGIC_FREE
    vsize_t size;       // # bytes in this block (including header)
    vlink_t next;       // memory[] index of next free block
    vlink_t prev;       // memory[] index of previous free block
} free_header_t;

void sal_merge(void *);
static int getMergeDirection(vaddr_t, vaddr_t, int, int);

// Global data
static byte *memory = NULL;      // pointer to start of suballocator memory
static vaddr_t free_list_ptr;    // index in memory[] of first block in free list
static vsize_t memory_size;      // number of bytes malloc'd in memory[]

void sal_init(uint32_t size) {
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

void *sal_malloc(uint32_t n) {
    // The size of every region must be a power of two and greater than 4 bytes
    if (n <= MIN_REGION_SIZE) {
        abort();
    }

    // actual size required including header
    vsize_t memSize = n + HEADER_SIZE;    
    free_header_t *startpoint = (free_header_t *) (memory + free_list_ptr);
    free_header_t *curr = startpoint;
    free_header_t *target = NULL;

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
        if ((target == NULL) && (curr->size >= memSize)) {
            target = curr;
        } else if ((curr->size < target->size) && (curr->size >= memSize)) {
            target = curr;
        }
    }

    void *returnValue = NULL;
    
    // only if there is a memory region that will fit memSize
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

    return returnValue;  
}

void sal_free(void *object) {
    //create a pointer and index to the region
    free_header_t *objPtr = (free_header_t *) ((byte *) object - HEADER_SIZE);
    vaddr_t objAddr = ((byte *) objPtr) - memory;

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
    
    // 'deallocate' the memory block
    objPtr->magic = MAGIC_FREE;

    sal_merge(objPtr);
}

// id that gets passed in is the beginning of the header
void sal_merge(void *id) {
    // if a merge occurs, newId will point to the finished merged block so sal_merge
    // can be called on it (if applicable) as there may be more possible merges
    void *newId = NULL;
    
    free_header_t *objPtr = (free_header_t *) id;
    vaddr_t objAddr = (byte *) objPtr - memory;
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
    
    // Three cases assuming both blocks on either side of objPtr are free
    // 1. a larger block on one side and the block to be merged with on the other side
    // 2. a smaller block on one side and a block of equal size on the other (need to check which
    //    one to merge with)
    // 3. two blocks of equal sizing to the middle block (need to check which one to merge with)
    
    // prevFree or afterFree will only be mergeable if their size is equal to id's size
    // and if they are directly adjacent in terms of memory position
    boolean prevFreeMergeable = (prevFree->size == objPtr->size) &&
        (((byte *) memory + (prevFreeAddr + prevFree->size )) == (byte *) id);      
    boolean afterFreeMergeable = (afterFree->size == objPtr->size) &&
        (((byte *) memory + (afterFreeAddr - objPtr->size)) == (byte *) id);
    
    if (prevFreeMergeable || afterFreeMergeable) {
        // i.e. only one can be merged with id
        // the block to be merged with id will be pointed by mergeTarget
        free_header_t *mergeTarget = NULL;
        
        // If the other block that's not getting merged is larger than the to be merged block
        // the other adjacent block must be the one getting merged.
        // Small optimisation rather than having to use the comparatively expensive getMergeDirection
        // function for first case.
        if ((prevFree->size == objPtr->size) && (afterFree->size > objPtr->size)) {
            mergeTarget = prevFree;
        } else if ((afterFree->size == objPtr->size) && (prevFree->size > objPtr->size)) {
            mergeTarget = afterFree;
        } else if (getMergeDirection(prevFreeAddr, objAddr, 0, memory_size) == BEFORE) {
            mergeTarget = prevFree;
        } else {
            mergeTarget = afterFree;
        }             
        vaddr_t mergeTargetAddr = (vaddr_t) ((byte *) mergeTarget - memory);
        
        // now do merging of mergeTarget and objPtr
        // TODO: PROBABLY CAN BE PUT INTO HELPER FUNCTION / CURRENTLY VERY MESSY
        // TODO: CHECK IF THERE ARE MORE BUGS 
        // TODO: BUG HERE WHEN MERGING THE ONLY TWO FREE BLOCKS INTO THE ONLY ONE BLOCK
        /* if (mergeTarget->next == objAddr && objPtr->prev == mergeTargetAddr) {
            // i.e. when they merge, only one block remains in free list
            if (mergeTarget < objPtr) {
                mergeTarget->size *= 2;
                mergeTarget->next = mergeTargetAddr;
                mergeTarget->prev = mergeTargetAddr;
            } else {
                objPtr->size *= 2;
                objPtr->next = objAddr;
                objPtr->prev = objAddr;
                free_list_ptr = objAddr;
            }
        } else */ 
        if (mergeTarget < objPtr) {
            // i.e. prevFree will be merged
            // mergeTarget/beforeFree will now the entry in the free list
            free_header_t *newBefore = (free_header_t *)(memory + prevFree->prev);
            mergeTarget->size *= 2;
            mergeTarget->next = afterFreeAddr;
            mergeTarget->prev = (vlink_t) ((byte *) newBefore - memory);
            afterFree->prev = (vlink_t) ((byte *) mergeTarget - memory);
            newBefore->next = (vlink_t) ((byte *) mergeTarget - memory);
            newId = (void *) mergeTarget;
        } else if (mergeTarget > objPtr) {
            // i.e. afterFree will be merged
            // objPtr will be the entry in the free list
            free_header_t *newAfter = (free_header_t *)(memory + afterFree->next);
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

// Returns 0/BEFORE or 1/AFTER depending on which objPtr needs to merge with.
// It works by recursively dividing the entire allocated memory block and seeing
// if prevFree or objPtr lies on the halfway point or at the bounds. If halfway == prevFreeAddr 
// or prevFreeAddr == begin, it means that it has to merge with prevFree. Vice versa for objAddr
// and merging with after. This should work with the last 2 cases in the above comment.
static int getMergeDirection(vaddr_t prevFreeAddr, vaddr_t objAddr, int begin, int end) {
    int direction = BEFORE;
    int halfway = (end - begin) / 2;
    if (objAddr == begin || objAddr == halfway) {
        direction = AFTER;
    } else if (prevFreeAddr == begin || prevFreeAddr == halfway) {
        direction = BEFORE;
    } else {
        // if none of the addr lie on significant areas, keep halving the bounded area
        if (prevFreeAddr < halfway) {
            // in the first half
            direction = getMergeDirection(prevFreeAddr, objAddr, begin, halfway);
        } else {
            // in the second half
            direction = getMergeDirection(prevFreeAddr, objAddr, (begin + halfway), end);
        }
    }
    return direction; 
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
