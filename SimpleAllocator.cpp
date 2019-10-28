//
//  SimpleAllocator.cpp
//  SimpleAllocator
//
//  Created by Aleksandr Khasanov.
//  Copyright © 2019 Sashok Corporation LLC. All rights reserved.
//

#include "SimpleAllocator.hpp"

////////////////////////////////////////////////////////////////////////////////
// class SimpleAllocator

//----------------------------------------------------------------------------//
SimpleAllocator::SimpleAllocator(void* memoryBuffer, size_t size)
{
    memoryBegin = memoryBuffer;
    totalMemorySize = size;

    // create initial block and mark all memory as free
    
    // begin of memory block
    head = (BeginMarker*)memoryBuffer;
    head->blockSize = size - sizeof(BeginMarker) - sizeof(EndMarker);
    head->isFree = true;
    head->nextFreeBlock = nullptr;
    head->prevFreeBlock = nullptr;

    // end of memory block
    EndMarker* endMarker = (EndMarker*)((char*)memoryBuffer + size - sizeof(EndMarker));
    endMarker->blockSize = head->blockSize;
    endMarker->isFree = head->isFree;
}

//----------------------------------------------------------------------------//
void* SimpleAllocator::alloc(size_t size)
{
    if (size == 0)
    {
        return nullptr;
    }
    
    BeginMarker* currentFreeBlockMarker = head;

    while (currentFreeBlockMarker != nullptr)
    {
        if (currentFreeBlockMarker->blockSize == size
            || currentFreeBlockMarker->blockSize == size + sizeof(BeginMarker) + sizeof(EndMarker))
        {
            // only have to mark block as 'not free' and remove it from list of 'free' memory blocks
            // in follwing cases:
            // - if requested block size is equal to current block size
            // - or no free space would be left in current block after adding bounding markers
            
            removeFreeMemoryBlock(currentFreeBlockMarker);

            currentFreeBlockMarker->isFree = false;
            currentFreeBlockMarker->nextFreeBlock = nullptr;
            currentFreeBlockMarker->prevFreeBlock = nullptr;

            EndMarker* blockEndMarker = getEndMarker(currentFreeBlockMarker);
            blockEndMarker->isFree = false;

            return (void*)((char*)currentFreeBlockMarker + sizeof(BeginMarker));
        }
        else if (size + sizeof(BeginMarker) + sizeof(EndMarker) < currentFreeBlockMarker->blockSize)
        {
            // decrease size of current 'free' memory block
            // and add 'not free' memory block
            
            size_t newSize = currentFreeBlockMarker->blockSize - size - sizeof(BeginMarker) - sizeof(EndMarker);

            currentFreeBlockMarker->blockSize = newSize;

            EndMarker* newEndMarker = (EndMarker*)((char*)currentFreeBlockMarker + sizeof(BeginMarker) + newSize);
            newEndMarker->isFree = true;
            newEndMarker->blockSize = newSize;

            BeginMarker* newBeginMarker = (BeginMarker*)((char*)newEndMarker + sizeof(EndMarker));
            newBeginMarker->isFree = false;
            newBeginMarker->nextFreeBlock = nullptr;
            newBeginMarker->prevFreeBlock = nullptr;
            newBeginMarker->blockSize = size;

            EndMarker* endMarker = (EndMarker*)((char*)newBeginMarker + sizeof(BeginMarker) + size);
            endMarker->isFree = false;
            endMarker->blockSize = size;

            return (void*)((char*)newBeginMarker + sizeof(BeginMarker));
        }

        currentFreeBlockMarker = currentFreeBlockMarker->nextFreeBlock;
    }

    return nullptr;
}

//----------------------------------------------------------------------------//
void SimpleAllocator::free(void* ptr)
{
    {
        BeginMarker* blockBeginMarker = (BeginMarker*)((char*)ptr - sizeof(BeginMarker));

        if (blockBeginMarker->isFree)
        {
            return;
        }
    }

    void* currentBlockPointer = ptr;

    // check nearby memory blocks and append them to current if they are free
    
    if (hasPreviousMemoryBlock(currentBlockPointer))
    {
        BeginMarker* blockBeginMarker = nullptr;
        EndMarker* blockEndMarker = nullptr;
        std::tie(blockBeginMarker, blockEndMarker) = getMemoryBlockBounds(currentBlockPointer);

        EndMarker* prevBlockEndMarker = (EndMarker*)((char*)blockBeginMarker - sizeof(EndMarker));
        if (prevBlockEndMarker->isFree)
        {
            // if previous memory block is free - append it to the current memory block
            // and mark compound block as 'not free'
            
            BeginMarker* prevBlockBeginMarker = getBeginMarker(prevBlockEndMarker);

            removeFreeMemoryBlock(prevBlockBeginMarker);

            prevBlockBeginMarker->isFree = false;
            prevBlockBeginMarker->blockSize += blockBeginMarker->blockSize + sizeof(BeginMarker) + sizeof(EndMarker);

            blockEndMarker->blockSize = prevBlockBeginMarker->blockSize;
            
            // new block is now bounded with [prevBlockBeginMarker, blockEndMarker]
            
            currentBlockPointer = (void*)((char*)prevBlockBeginMarker + sizeof(BeginMarker));
        }
    }

    if (hasNextMemoryBlock(currentBlockPointer))
    {
        BeginMarker* blockBeginMarker = (BeginMarker*)((char*)currentBlockPointer - sizeof(BeginMarker));
        
        void* nextBlockPointer = (void*)((char*)currentBlockPointer + blockBeginMarker->blockSize + sizeof(EndMarker) + sizeof(BeginMarker));
        
        BeginMarker* nextBlockBeginMarker = nullptr;
        EndMarker* nextBlockEndMarker = nullptr;
        std::tie(nextBlockBeginMarker, nextBlockEndMarker) = getMemoryBlockBounds(nextBlockPointer);

        if (nextBlockBeginMarker->isFree)
        {
            //  if next memory block is free - append it to the current memory block
            // and mark compound block as 'not free'
            removeFreeMemoryBlock(nextBlockBeginMarker);

            blockBeginMarker->blockSize += nextBlockBeginMarker->blockSize + sizeof(BeginMarker) + sizeof(EndMarker);

            nextBlockEndMarker->blockSize = blockBeginMarker->blockSize;
            nextBlockEndMarker->isFree = false;
            
            // new block is now bounded with [blockBeginMarker, nextBlockEndMarker]
        }
    }
    
    //  mark memory block as free and add it to the list of free memory blocks
    BeginMarker* blockBeginMarker = nullptr;
    EndMarker* blockEndMarker = nullptr;
    std::tie(blockBeginMarker, blockEndMarker) = getMemoryBlockBounds(currentBlockPointer);
    
    blockBeginMarker->isFree = true;
    blockEndMarker->isFree = true;

    addFreeMemoryBlock(blockBeginMarker);
}

//----------------------------------------------------------------------------//
bool SimpleAllocator::hasPreviousMemoryBlock(void* ptr)
{
    return ((char*)memoryBegin) < ((char*)ptr - sizeof(BeginMarker));
}

//----------------------------------------------------------------------------//
bool SimpleAllocator::hasNextMemoryBlock(void* ptr)
{
    BeginMarker* beginMarker = (BeginMarker*)((char*)ptr - sizeof(BeginMarker));
    size_t size = beginMarker->blockSize;

    return ((char*)ptr + size + sizeof(EndMarker)) < ((char*)memoryBegin + totalMemorySize);
}

//----------------------------------------------------------------------------//
BeginMarker* SimpleAllocator::getBeginMarker(EndMarker* endMarker)
{
    size_t size = endMarker->blockSize;

    return (BeginMarker*)((char*)endMarker - size - sizeof(BeginMarker));
}

//----------------------------------------------------------------------------//
EndMarker* SimpleAllocator::getEndMarker(BeginMarker* beginMarker)
{
    size_t size = beginMarker->blockSize;

    return (EndMarker*)((char*)beginMarker + size + sizeof(BeginMarker));
}

//----------------------------------------------------------------------------//
std::pair<BeginMarker*, EndMarker*> SimpleAllocator::getMemoryBlockBounds(void* ptr)
{
    BeginMarker* blockBeginMarker = (BeginMarker*)((char*)ptr - sizeof(BeginMarker));
    EndMarker* blockEndMarker = (EndMarker*)((char*)ptr + blockBeginMarker->blockSize);
    
    return std::make_pair(blockBeginMarker, blockEndMarker);
}

//----------------------------------------------------------------------------//
void SimpleAllocator::addFreeMemoryBlock(BeginMarker* marker)
{
    if (head != nullptr)
    {
        BeginMarker* currentHead = head;

        currentHead->prevFreeBlock = marker;
        marker->nextFreeBlock = currentHead;
        marker->prevFreeBlock = nullptr;
    }

    head = marker;
}

//----------------------------------------------------------------------------//
void SimpleAllocator::removeFreeMemoryBlock(BeginMarker* node)
{
    if (node == head)
    {
        head = node->nextFreeBlock;
    }

    BeginMarker* prevNode = node->prevFreeBlock;
    BeginMarker* nextNode = node->nextFreeBlock;

    if (prevNode != nullptr)
    {
        prevNode->nextFreeBlock = nextNode;
    }
    
    if (nextNode != nullptr)
    {
        nextNode->prevFreeBlock = prevNode;
    }

    node->nextFreeBlock = nullptr;
    node->prevFreeBlock = nullptr;
}
