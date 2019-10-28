//
//  SimpleAllocator.hpp
//  SimpleAllocator
//
//  Created by Aleksandr Khasanov.
//  Copyright © 2019 Sashok Corporation LLC. All rights reserved.
//

#ifndef SimpleAllocator_hpp
#define SimpleAllocator_hpp

#include <cstdlib>
#include <vector>

/**
 * Strcut for marking memory block beginning.
 */
struct BeginMarker
{
    size_t blockSize;
    
    // marks current memory block as 'free' or 'not free'
    bool isFree = true;
    
    // pointer for next free memry block
    // if isFree flag is set to true - must be set to nullptr
    BeginMarker* nextFreeBlock = nullptr;
    
    // pointer for previous free memory block
    // if isFree flag is set to true - must be set to nullptr
    BeginMarker* prevFreeBlock = nullptr;
};

/**
 * Struct for marking memory block end.
 */
struct EndMarker
{
    size_t blockSize;
    bool isFree = true;
};

/**
 * Simple memory allocator.
 *
 * Stores blocks of free memory as double-linked list.
 * Uses border markers for  storing memory block bouds and decreasing memory fragmentation.
 */
class SimpleAllocator
{
public:
    
    SimpleAllocator(void* memoryBuffer, size_t size);

    void* alloc(size_t size);

    void free(void* ptr);

private:
    
    bool hasPreviousMemoryBlock(void* ptr);
    
    bool hasNextMemoryBlock(void* ptr);

    BeginMarker* getBeginMarker(EndMarker* endMarker);

    EndMarker* getEndMarker(BeginMarker* beginMarker);
    
    std::pair<BeginMarker*, EndMarker*> getMemoryBlockBounds(void* ptr);
    
    void addFreeMemoryBlock(BeginMarker* marker);

    void removeFreeMemoryBlock(BeginMarker* node);
    
private:
    // Pointer for the first block of free memory
    // Blocks of free memory are stored as doluble-linked list.
    BeginMarker* head = nullptr;

    // pointer for logical memory space
    void* memoryBegin = nullptr;
    
    // total amount of memory that can be allocated
    size_t totalMemorySize = 0;
};

#endif /* SimpleAllocator_hpp */
