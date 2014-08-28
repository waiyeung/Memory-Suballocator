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

// Pointers algorithms and casting
#define FH_PtrAdd(ptr, size)    ((free_header_t *) ((byte*)ptr + size))
#define FH_PtrSub(ptr, size)    ((free_header_t *) ((byte*)ptr - size))
#define PtrsSub(ptr1, ptr2)     (u_int32_t)((byte*)ptr1 - (byte*)ptr2)

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

// Functions declaration
static void sal_merge(free_header_t *);
static void mergeLink(free_header_t *, free_header_t *, free_header_t *);
static void link(free_header_t *, free_header_t *, free_header_t *);
static boolean oneFreeBlockRemaining(void);
static direction getMergeDirection(free_header_t *);

// Global data
static byte *memory = NULL;      // pointer to start of suballocator memory
static vaddr_t free_list_ptr;    // index in memory[] of first block in free list
static vsize_t memory_size;      // number of bytes malloc'd in memory[]

void sal_init(u_int32_t size) {
    // Do nothing if allocator has already been initiated
    if (memory == NULL || size <= HEADER_SIZE) {

        // Convert size to a power of two if it isn't already 
        // & at least as large as the input "size"
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
        //fprintf(stderr, "Requested size too small. Use size larger then %d bytes.\n", MIN_REGION_SIZE);
        return NULL;
    }
     
    // "memSize" : actual size required including header
    vsize_t memSize = n + HEADER_SIZE;    
    free_header_t *startpoint = FH_PtrAdd(memory, free_list_ptr);
    free_header_t *curr = startpoint;
    free_header_t *target = NULL;
    void *returnValue = NULL;     
       
    // traverse the entire free list trying to find the smallest region that will
    // fit memSize
    if (curr->magic != MAGIC_FREE) {
        fprintf(stderr, "Memory corruption");
        abort();
    }
    if ((curr->size >= memSize) && (target == NULL || curr->size < target->size)) {
        target = curr;
    }
    curr = FH_PtrAdd(memory, curr->next);
     
    while (curr != startpoint) {
        if (curr->magic != MAGIC_FREE) {
            fprintf(stderr, "Memory corruption");
            abort();
        }
        if ((curr->size >= memSize) && (target == NULL || curr->size < target->size)) {
            target = curr;
        }
        curr = FH_PtrAdd(memory, curr->next);
    }
    
    // only if there is a memory region that will fit memSize
    // return NULL if (target == NULL)
    if (target != NULL) {
     
        // get index to target (from memory[])
        vaddr_t targetAddr = PtrsSub(target, memory);
        
        // continuously split the target until we get a region that is the power of 2
        // just larger than memSize
        while (target->size >= memSize * 2) {
            // target <-> split <-> after (schematic of memory after split)
            target->size /= 2;
            free_header_t *split = FH_PtrAdd(target, target->size);            
            free_header_t *after = FH_PtrAdd(memory, target->next);            
            
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
        if (oneFreeBlockRemaining() == FALSE) {
     
            target->magic = MAGIC_ALLOC;
            
            // take out the allocated region from the link by changing the links
            // before and after target
            // curr = the region just before target
            curr = FH_PtrAdd(memory, target->prev);
            curr->next = target->next;
            // curr = the region just after target
            curr = FH_PtrAdd(memory, target->next);  
            curr->prev = target->prev;
            
            // if free_list_ptr is being allocated, it needs to be changed to the next
            // available region
            if (target == startpoint) {
                free_list_ptr = target->next;
            }

            returnValue = (void *) FH_PtrAdd(target, HEADER_SIZE); 
        }
    } 

    return returnValue;  
}

void sal_free(void *object) {
    //create a pointer and index to the region
    free_header_t *obj = FH_PtrSub(object, HEADER_SIZE);
    vaddr_t objAddr = PtrsSub(obj, memory);

    //check magic number to ensure freeing valid memory
    if (obj->magic != MAGIC_ALLOC) {
        fprintf(stderr, "Attempt to free non-allocated memory");
        abort();
    }
    
    // traverse the list until after is the next free block after object and 
    // then set prev to be the free block before object
    free_header_t *before = NULL;
    free_header_t *after = FH_PtrAdd(memory, free_list_ptr);

    // check if the object is locate before all the regions in the free list
    if (objAddr < free_list_ptr) {

        before = FH_PtrAdd(memory, after->prev);

        // we keep free_list_ptr to be the earliest free block so that our beforeFree and afterFree
        // loops work correctly (in sal_merge and sal_free) 
        free_list_ptr = objAddr;

    } else {
        // no need to worry about infinite looping as we will not be freeing the sole
        // remaining free block (i.e. after != obj)
        // the before < after is there to know when we have wrapped around
        while ((after < obj) && (before < after)) {
            before = after;
            after = FH_PtrAdd(memory, after->next);
        }

    }

    // 'deallocate' the memory block
    obj->magic = MAGIC_FREE;

    // link back to the deallocated block into the free list
    link(before, obj, after);

    // merge back into larger a block
    sal_merge(obj);
}

// obj: the beginning of the block that may be merged
static void sal_merge(free_header_t *obj) {
    // if a merge occurs, freeObj will point to the finished merged block so sal_merge
    // can be called on it (if applicable) as there may be more possible merges
    free_header_t *freeObj = obj;

    // iterate until there are no longer any merge-able blocks
    // or it is the sole remaining free block
    while (freeObj != NULL && oneFreeBlockRemaining() == FALSE) {

        // the free blocks before and after freeObj (which is free itself)
        free_header_t *beforeFree = FH_PtrAdd(memory, freeObj->prev);
        free_header_t *afterFree = FH_PtrAdd(memory, freeObj->next);
      
        // beforeFree or afterFree will only be mergeable if their size is equal to freeObj's size
        // and if they are directly adjacent in terms of memory position
        boolean beforeFreeMergeable = (beforeFree->size == freeObj->size) && 
            (FH_PtrAdd(beforeFree, freeObj->size) == freeObj);      
        boolean afterFreeMergeable = (afterFree->size == freeObj->size) && 
            (FH_PtrSub(afterFree, freeObj->size) == freeObj);
    

        // getMergeDirection() determine which direction freeObj MUST merge with 
        // (i.e. the one that it split with)
        if (afterFreeMergeable == TRUE && (getMergeDirection(freeObj) == AFTER)) {            
            // i.e. afterFree will be merged as mergeTarget > freeObj
            // freeObj will be the entry in the free list
            free_header_t *newAfter = FH_PtrAdd(memory, afterFree->next);
            mergeLink(beforeFree, freeObj, newAfter);
        } else if (beforeFreeMergeable == TRUE && (getMergeDirection(freeObj) == BEFORE)) {
            // i.e. beforeFree will be merged
            // mergeTarget/beforeFree will now the entry in the free list
            free_header_t *newBefore = FH_PtrAdd(memory, beforeFree->prev);
            mergeLink(newBefore, beforeFree, afterFree);
            freeObj = beforeFree;
        } else {
            // use break or set freeObj = NULL and check freeObj == NULL in the while
            freeObj = NULL;
        }

    }

}

// Checks whether the first free block's next and prev point to itself;
// if yes, then it must be the sole remaining free block
// Pre: "memory" is initialized; free_list_ptr is valid
// Post: return (boolean)TRUE/FALSE
static boolean oneFreeBlockRemaining(void) {
    assert(memory != NULL);
    free_header_t *listPtr = FH_PtrAdd(memory, free_list_ptr);
    assert(listPtr->magic == MAGIC_FREE);
    return ((free_list_ptr == listPtr->next) && (free_list_ptr == listPtr->prev));
}

static void mergeLink(free_header_t *before, free_header_t *obj, free_header_t *after) {
    obj->size *= 2;
    vlink_t objLink = PtrsSub(obj, memory);

    if (before == obj || after == obj) {
        // there were two free block before merging so there will only be
        // one free block after merging (requires separate linking)
        obj->next = objLink;
        obj->prev = objLink;
    } else {
        link(before, obj, after);
    }
}

// helper function to link before, obj, and after in the free list
static void link(free_header_t *before, free_header_t *obj, free_header_t *after) {
    assert (before != NULL && obj != NULL && after != NULL);
    assert (before->magic == MAGIC_FREE && obj->magic == MAGIC_FREE && after->magic == MAGIC_FREE);

    vlink_t objLink = PtrsSub(obj, memory);
    vlink_t beforeLink = PtrsSub(before, memory);
    vlink_t afterLink = PtrsSub(after, memory);

    obj->next = afterLink;
    obj->prev = beforeLink;
    before->next = objLink;
    after->prev = objLink;
}

// Returns BEFORE or AFTER depending on which obj needs to merge with.
// It works by assuming the entire memory block is divided into regions of
// size (obj->size). Then we can see if obj is at an even position or odd postion.
// If odd, then it merges before and if it's even then it merges with after.
// It has complexity O(1) and is faster than the previous O(log(n)) 'divide and conquer
// search'.
static direction getMergeDirection(free_header_t *obj) {
    assert(obj != NULL);
    vaddr_t objAddr = (vaddr_t) ((byte *) obj - memory);
    direction d;
 
    if((objAddr / obj->size) % 2 == 0) {
        d = AFTER;
    } else {
        d = BEFORE;
    }
 
    return d;
}   

void sal_end(void) {
    free(memory);
    memory = NULL;
    free_list_ptr = -1;
    memory_size = 0;
}

void sal_stats(void) {
    printf("\n>>>>>>>>>>>>>>>>>>>>sal_stats>>>>>>>>>>>>>>>>>>>>\n");
    printf("\n--Gobal variables--\n");
    printf("memory: %p\n", memory);
    printf("free_list_ptr: %u\n", free_list_ptr);
    printf("memory_size: %u\n", memory_size);

    printf("\n--Free list--\n");
    free_header_t *startpoint = FH_PtrAdd(memory, free_list_ptr);
    free_header_t *curr = startpoint;
    printf("<START>\n");
    printf("%u --> size: %u, next: %u, prev: %u\n",         
        PtrsSub(curr, memory), curr->size, curr->next, curr->prev);
    curr = FH_PtrAdd(memory, curr->next);
    while(curr != startpoint){
        printf("%u --> size: %u, next: %u, prev: %u\n", 
            PtrsSub(curr, memory), curr->size, curr->next, curr->prev);
        curr = FH_PtrAdd(memory, curr->next);
    }
    printf("<END>\n");
    
    printf("\n<<<<<<<<<<<<<<<<<<<<sal_stats<<<<<<<<<<<<<<<<<<<<\n");
}
