#include "MemoryManager.h"

#include <assert.h>
#include <algorithm>
#include <cstring>
#include <climits>

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

	VerifyAvailableMemoryList();

	return (mp_pool != nullptr);
}

void MemoryManagerD1::ReleaseMemory()
{
	delete[] mp_pool;
	mp_pool = nullptr;
}

void* MemoryManagerD1::Allocate(uint sizeBytes)
{
	assert(sizeBytes > 0);

	// Calculate the actual block size requested, and make sure it is of the minimum size
	uint size_meta_l = sizeof(uint);	// Need an extra bit of space for the size tracker.
	uint size_meta_r = 0;

#if defined MEMORY_DEBUG_VERIFY
	// Need space for the front and back data buffer
	size_meta_l += sizeof(PATTERN_BUFFER);
	size_meta_r += sizeof(PATTERN_BUFFER);
#endif

	uint size_total = std::max(sizeBytes + size_meta_l + size_meta_r, sizeof(MemoryBlockD1));

	/////////////////////////////////////////////
	// Search list for smallest valid block

	// Define initial "best fit"
	MemoryBlockD1* p_block_best = nullptr;
	uint size_best				= UINT_MAX;

	MemoryBlockD1* p_block_curr = mp_listStart;
	while (p_block_curr)
	{
		uint size_curr = p_block_curr->m_sizeBytes;
		if (size_curr >= size_total && size_curr < size_best)
		{
			p_block_best = p_block_curr;
			size_best = size_curr;
		}
		p_block_curr = p_block_curr->mp_blockNext;
	}
	/////////////////////////////////////////////

	void* p_block_alloc = nullptr;
	// Allocate memory + split empty memory block
	if (p_block_best)
	{
		// Define parameters of allocated block
		p_block_alloc	= p_block_best;
		uint size_alloc	= size_total;

		// If we don't have enough leftover memory to create a new empty memory block, use all the space
		if (size_best - size_total < sizeof(MemoryBlockD1))
		{
			size_alloc = size_best;
		}

		//////////////////////////////////////////
		// Remove selected block from the list
		MemoryBlockD1* p_block_L = p_block_best->mp_blockPrev;
		MemoryBlockD1* p_block_R = p_block_best->mp_blockNext;
		// Case: Middle of List
		if (p_block_L && p_block_R)
		{
			p_block_L->mp_blockNext = p_block_R;
			p_block_R->mp_blockNext = p_block_L;
		}
		// Case: Rightmost Block
		else
		if (p_block_L)
		{
			p_block_L->mp_blockNext = nullptr;
		}
		// Case: Leftmost Block
		else
		if (p_block_R)
		{
			mp_listStart = p_block_R;
		}
		// Case: Only entry in list
		else 
		{
			mp_listStart = nullptr;
		}
		//////////////////////////////////////////

		// Finally, create new empty block w/ remaining memory if necessary
		if (size_alloc < size_best)
		{
			void* p_block_new	= (void*)((uchar*)p_block_best + size_total);
			uint size_new		= size_best - size_total;
			CreateMemoryBlock(p_block_new, size_new);
		}

		// Store updated size for allocated block
		((MemoryBlockD1*)p_block_alloc)->m_sizeBytes = size_alloc;

#if defined MEMORY_DEBUG_VERIFY
		// Fill allocated memory with debug pattern
		*((uint*)p_block_alloc + 1) = PATTERN_BUFFER;												// Buffer: Front
		memset(((uint*)p_block_alloc + 2), PATTERN_ALLOC, size_alloc - size_meta_l - size_meta_r);	// User memory
		*(uint*)((uchar*)p_block_alloc + size_alloc - size_meta_r) = PATTERN_BUFFER;				// Buffer: Rear
#endif
	}
	else
	{
		assert(false); // :TODO: Change to warning?
	}

	// DEBUG
	VerifyAvailableMemoryList();

	// Return pointer to start of the usable memory block (step past the block size)
	return (p_block_alloc ? (void*)((uchar*)p_block_alloc + size_meta_l) : nullptr);
}

void MemoryManagerD1::Deallocate(void* pMem)
{
	// Check if address lies within our memory block
	assert(IsValidAddress(pMem));
	
	// Retrieve size
	void* p_block_start	= (void*)((uint*)pMem - 1);
	uint block_size		= *((uint*)p_block_start);

	// Create new empty memory block
	MemoryBlockD1* p_block = CreateMemoryBlock(p_block_start, block_size);

#if defined MEMORY_DEBUG_VERIFY
	// After newly-freed block is created, set verification pattern
	memset((void*)(p_block + 1), PATTERN_FREE, p_block->m_sizeBytes - sizeof(MemoryBlockD1));
#endif

	// DEBUG
	VerifyAvailableMemoryList();
}

/**
**	Create an EMPTY memory block.
**	@param pAddr		Pointer to the start of the memory block
**	@param sizeBytes	Total size of free space available at pAddr
**						NOTE: This is NOT the amount of space available for allocation
 */
MemoryManagerD1::MemoryBlockD1* MemoryManagerD1::CreateMemoryBlock(void* pAddr, uint sizeBytes)
{
	// Typecast & Initialize
	MemoryBlockD1* p_block = (MemoryBlockD1*)pAddr;
	p_block->m_sizeBytes = sizeBytes;
	p_block->mp_blockPrev = nullptr;
	p_block->mp_blockNext = nullptr;

	MemoryBlockD1* p_block_inserted = p_block;

	/////////////////////////
	// Insert into list
	// Case: Empty List
	if (!mp_listStart)
	{
		mp_listStart = p_block;
	}
	// Case: Front of List
	else
	if (p_block < mp_listStart)
	{
		p_block_inserted = InsertMemoryBlock(p_block, nullptr,mp_listStart);
	}
	// Case: Not Front of List
	else
	{
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
			p_block_curr = p_block_curr->mp_blockNext;
		}

		// Connect Memory Blocks
		p_block_inserted = InsertMemoryBlock(p_block, p_block_prev, p_block_curr);
	}

	return p_block_inserted;
}

/**
 *	Inserts BlockM into list between BlockL & BlockR. 
 *	Merges blocks when contiguous.
 *
 *	@return pointer to the block that BlockM is a part of at the end of the insertion.
 *	This is BlockM unless BlockM is merged into BlockL, in which case we return BlockL.
 */
MemoryManagerD1::MemoryBlockD1* MemoryManagerD1::InsertMemoryBlock(MemoryBlockD1* pBlockM, MemoryBlockD1* pBlockL, MemoryBlockD1* pBlockR)
{
	assert(pBlockM);

	// Return value
	MemoryBlockD1* p_block = pBlockM;

	// Right Block
	if (pBlockR)
	{
		pBlockM->mp_blockNext = pBlockR;
		pBlockR->mp_blockPrev = pBlockM;

		TryCoalesceBlocks(pBlockM, pBlockR);
	}

	// Left Block
	if (pBlockL)
	{
		pBlockM->mp_blockPrev = pBlockL;
		pBlockL->mp_blockNext = pBlockM;

		if (TryCoalesceBlocks(pBlockL, pBlockM))
		{
			p_block = pBlockL;
		}
	}
	else
	{
		// Update list start if no left block exists
		mp_listStart = pBlockM;
	}

	return p_block;
}

bool MemoryManagerD1::TryCoalesceBlocks(MemoryBlockD1* pBlockL, MemoryBlockD1* pBlockR)
{
	// Bail out if we don't have 2 valid linked blocks blocks
	if (!pBlockL || !pBlockR || pBlockL->mp_blockNext != pBlockR)
	{
		return false;
	}

	// Check if contiguous
	uint size_l = pBlockL->m_sizeBytes;

	// Not contiguous: Bail out
	if ((uchar*)pBlockL + size_l != (uchar*)pBlockR)
	{
		return false;
	}

	// Contiguous: Merge right block into the left block
	pBlockL->m_sizeBytes += pBlockR->m_sizeBytes;

	MemoryBlockD1* p_block_next = pBlockR->mp_blockNext;
	pBlockL->mp_blockNext = p_block_next;
	if (p_block_next)
	{
		p_block_next->mp_blockPrev = pBlockL;
	}

	return true;
}

bool MemoryManagerD1::IsValidAddress(void* pAddr)
{
	// Null: Clearly not valid
	if (!pAddr)
	{
		return false;
	}

	// Verify address lies within our memory pool
	uchar* p_addr = (uchar*)pAddr;
	return (mp_pool <= p_addr && p_addr < mp_pool + m_poolSizeBytes);
}

//////////////////////////////////////////////////////////////////////
// DEBUG
void MemoryManagerD1::PrintMemoryBlock(MemoryBlockD1* pBlock) const
{
	printf("Block Addr: %p\n", pBlock);
	printf("Block Size: %d bytes\n", pBlock->m_sizeBytes);
}

void MemoryManagerD1::VerifyAvailableMemoryList() const
{
	printf("========== LIST VALIDATION ==========\n");
	MemoryBlockD1* p_block_curr = mp_listStart;
	MemoryBlockD1* p_block_prev = nullptr;
	//while (p_block_curr)
	//{
	//	printf("[][][]\n");
	//	PrintMemoryBlock(p_block_curr);

	//	// Check: Ascending value
	//	if (p_block_prev > p_block_curr)
	//	{
	//		printf("WARNING! Linked list is not strictly increasing!\n");
	//	}

	//	// Check: Doubly-Linked Lists strongly connected
	//	if (p_block_curr->mp_blockPrev != p_block_prev)
	//	{
	//		printf("WARNING! This block does not link back to the previous block!\n");
	//	}
	//	printf("[][][]\n");

	//	// Advance list
	//	p_block_prev = p_block_curr;
	//	p_block_curr = p_block_curr->mp_blockNext;
	//}

	// Alternate Loop
	void* p_addr_curr	= mp_pool;
	void* p_addr_end	= mp_pool + m_poolSizeBytes;
	p_block_curr		= mp_listStart;

	while (p_addr_curr < p_addr_end)
	{
		// Retrieve size
		uint size_curr = *(uint*)p_addr_curr;

		// Case: Empty Block
		if (p_addr_curr == p_block_curr)
		{
			printf("[0][0][0][0][0]\n");
			PrintMemoryBlock(p_block_curr);
			printf("[0][0][0][0][0]\n");

			// Advance to next empty block
			p_block_curr = p_block_curr->mp_blockNext;
		}
		// Case: Allocated Block
		else
		{
			printf("[1][1][1][1][1]\n");
			printf("Size: %d\n", size_curr);

#if defined MEMORY_DEBUG_VERIFY
			// Check: Data Buffers
			uint* p_buffer_l = (uint*)p_addr_curr + 1;
			uint* p_buffer_r = (uint*)((uchar*)p_addr_curr + size_curr - sizeof(uint));
			if (*p_buffer_l != PATTERN_BUFFER)
			{
				printf("WARNING! Left buffer compromised! (%p)\n", p_buffer_l);
				assert(false);
			}
			if (*p_buffer_r != PATTERN_BUFFER)
			{
				printf("WARNING! Right buffer compromised! (%p)\n", p_buffer_r);
				assert(false);
			}
#endif
			printf("[1][1][1][1][1]\n");	
		}

		p_addr_curr = ((uchar*)p_addr_curr) + size_curr;
	}
	printf("========== LIST END ==========\n\n");
}