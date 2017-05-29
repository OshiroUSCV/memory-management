#include "MemoryManager.h"

#include <assert.h>

MemoryAllocator::MemoryAllocator()
{
	m_blockCountFree	= 0;
	m_blockCountTotal	= 0;
	m_blockCountInit	= 0;
	m_blockSize			= 0;
	m_poolSize			= 0;
	mp_blockNext		= nullptr;
	mp_memPool			= nullptr;
}

/**
 *	@return True if memory was successfully allocated, false otherwise (TODO)
*/
bool MemoryAllocator::AllocateMemory(uint blockSize, uint blockCount)
{
	// Sanity check
	assert(blockCount > 0);

	// Release memory if we already have some allocated
	if (m_poolSize > 0)
	{
		ReleaseMemory();
	}
	// Align block size
	int remainder_bytes	= blockSize % BYTE_ALIGNMENT;
	m_blockSize			= blockSize + remainder_bytes;
	// :TODO: If we reduce the alignmnent, make sure we have at least 4 bytes to store indices in empty blocks

	// Allocate memory
	m_poolSize			= m_blockSize * blockCount;
	mp_memPool			= new uchar[m_poolSize];
	m_blockCountTotal	= blockCount;
	m_blockCountFree	= blockCount;
	m_blockCountInit = 0;

	return true;
}


uchar* MemoryAllocator::GetBlock(uint blockIdx) const
{
	// Sanity check
	assert(0 <= blockIdx  && blockIdx < m_blockCountTotal);

	return mp_memPool + (blockIdx * m_blockSize);
}

uint MemoryAllocator::GetIdx(uchar* pBlock) const
{
	assert(GetBlock(0) <= pBlock && pBlock <= GetBlock(m_blockCountTotal - 1));
	return (uint)((pBlock - mp_memPool) / m_blockSize);
}


uchar*	MemoryAllocator::AllocBlock()
{
	// Initialize the next uninitialized block each time AllocBlock is called
	if (m_blockCountInit < m_blockCountTotal)
	{
		// Retrieve the next uninitialized block
		uint* p_block_init = (uint*)GetBlock(m_blockCountInit);	
		// Point it to the next available block
		*p_block_init = m_blockCountInit + 1;
		// Increment count
		m_blockCountInit++;
	}

	// Check if the pool has been fully exhausted
	if (m_blockCountFree == 0)
	{
		assert(false);
		return nullptr;
	}
	
	// If free blocks exist, allocate out the next available block
	uchar* p_block_next = mp_blockNext;
	m_blockCountFree--;

	// Still have free blocks; point to next block in the list
	if (m_blockCountFree > 0)
	{
		mp_blockNext = GetBlock(*((uint*)mp_blockNext));

	}
	// No free blocks remain after this; empty List
	else
	{
		mp_blockNext = nullptr;
	}

	return p_block_next;

	// WIP ATTEMPT?
	//// Check if the pool has been fully exhausted
	//if (m_blockCountFree == 0)
	//{
	//	assert(false);
	//	return nullptr;
	//}
	//
	//// Otherwise, allocate out the next available block
	//uchar* p_block_next = mp_blockNext;
	//m_blockCountFree--;

	//int idx_next = GetIdx(mp_blockNext);
	//	
	//// If next block is uninitialized
	//if (m_blockCountInit == idx_next)
	//{
	//	// New "next block" is the next block sequentially
	//	mp_blockNext = GetBlock(idx_next + 1);
	//	m_blockCountInit++;
	//}
	//// If next block is initialized
	//else
	//{
	//	// Retrieve the new "next block" from the current block
	//	mp_blockNext = GetBlock(*(uint*)mp_blockNext);
	//}

	//// Return pointer to the next block
	//return p_block_next;
}
void MemoryAllocator::DeallocBlock(void* pBlock)
{
	// There is at least one available block
	if (mp_blockNext != nullptr)
	{
		*((uint*)pBlock) = GetIdx(mp_blockNext);	// Store previous block's IDX in the newly-freed block
	}
	// There were no available blocks in waiting
	else
	{
		*((uint*)pBlock)	= m_blockCountTotal;	// Use the total block count to indicate the end of the list
	}

	// Place newly-freed block at the head of the list
	mp_blockNext = (uchar*)pBlock;				

	// Update free block count
	m_blockCountFree++;
}

////////////////////////////////////////////////////
// CLASS: MemoryManagerD1
////////////////////////////////////////////////////

MemoryManagerD1::MemoryManagerD1()
{
	mp_pool			= nullptr;
	m_poolSizeBytes	= 0;
	mp_listStart	= nullptr;
}

MemoryManagerD1::MemoryManagerD1(uint memBytes) : MemoryManagerD1()
{
	AllocateMemoryPool(memBytes);
}

MemoryManagerD1::~MemoryManagerD1()
{
	ReleaseMemory();
}

bool MemoryManagerD1::AllocateMemoryPool(uint memBytes)
{
	ReleaseMemory();

	// Allocate pool
	mp_pool			= new uchar[memBytes];
	m_poolSizeBytes	= memBytes;

	// Initialize pool as one big block
	if (mp_pool)
	{
		CreateMemoryBlock(mp_pool, memBytes);
	}

	return mp_pool;
}

void MemoryManagerD1::ReleaseMemory()
{
	delete[] mp_pool;
	mp_pool = nullptr;
}

MemoryManagerD1::MemoryBlockD1* MemoryManagerD1::CreateMemoryBlock(void* pAddr, uint sizeBytes)
{
	// Typecast & Initialize
	MemoryBlockD1* p_block = (MemoryBlockD1*)pAddr;
	p_block->m_sizeBytes = sizeBytes;
	p_block->mp_blockPrev = nullptr;
	p_block->mp_blockNext = nullptr;

	/////////////////////////
	// Insert into list
	// Case: Empty List
	if (!mp_listStart)
	{
		mp_listStart = p_block;
	}

	MemoryBlockD1* p_block_curr = mp_listStart;
	MemoryBlockD1* p_block_prev = nullptr;
	while (p_block_curr)
	{
		// Bail out if we've reached the end or if we've found our spot
		if (p_block_curr->mp_blockNext == nullptr || p_block > p_block_curr->mp_blockNext)
		{
			break;
		}

		// Advance
		p_block_prev = p_block_curr;
		p_block_curr = mp_listStart->mp_blockNext;
	}

	// Connect Memory Blocks
	InsertMemoryBlock(p_block, p_block_prev, p_block_curr);

	return p_block;
}

void MemoryManagerD1::InsertMemoryBlock(MemoryBlockD1* pBlockM, MemoryBlockD1* pBlockL, MemoryBlockD1* pBlockR)
{
	assert(pBlockM);

	// Left Block
	if (pBlockL)
	{
		pBlockM->mp_blockPrev = pBlockL;
		pBlockL->mp_blockNext = pBlockM;
	}

	// Right Block
	if (pBlockR)
	{
		pBlockM->mp_blockNext = pBlockR;
		pBlockR->mp_blockPrev = pBlockL;
	}
}

bool MemoryManagerD1::TryCoalesceBlocks(MemoryBlockD1* pBlockL, MemoryBlockD1* pBlockR)
{
	// Bail out if we don't have 2 valid blocks
	if (!pBlockL || !pBlockR)
	{
		return false;
	}
	// Check if contiguous


}