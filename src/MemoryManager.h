
// Basic type defines
typedef unsigned int uint;
typedef unsigned char uchar;

#define BYTE_ALIGNMENT	(8)	// 64-bit alignment (8 bytes)

/**
 * CLASS: Fixed-size memory allocator
*/
class MemoryAllocator
{
protected:
	uint m_blockSize;		// Size of each memory block (Bytes)
	uint m_blockCountTotal;	// # of blocks in the memory pool
	uint m_blockCountFree;	// # of free blocks in the memory pool
	uint m_blockCountInit;	// # of initialized blocks


	uint	m_poolSize;		// Total size of memory pool allocated (Bytes)
	uchar*	mp_memPool;		// Contiguous memory pool provided by the the memory manager
	uchar*	mp_blockNext;	// Next block to be allocated

	
public:
	///////////////////////////////////
	// Initialization/Destruction
	MemoryAllocator();

	// Releases memory
	void ReleaseMemory()
	{
		// Reset all management variables
		m_blockSize			= 0;
		m_blockCountTotal	= 0;
		m_blockCountFree	= 0;
		m_blockCountInit = 0;
		m_poolSize			= 0;
		mp_blockNext		= nullptr;

		// Release memory pool
		delete[] mp_memPool;
		mp_memPool = nullptr;
	}

	// Allocate memory for the internal pool
	bool AllocateMemory(uint blockSize, uint blockCount);

	~MemoryAllocator()
	{
		ReleaseMemory();
	}
	
	///////////////////////////////////
	// Accessors
	uchar*	GetBlock(uint blockIdx) const;
	uint	GetIdx(uchar* pBlock) const;
	uchar* operator[](uint idx) const { return GetBlock(idx);	}

	uint GetBlockSize() const { return m_blockSize; }

	// Alloc/Dealloc
	uchar*	AllocBlock();
	void	DeallocBlock(void* pBlock);

};	

class MemoryManagerD1
{
protected:
	struct MemoryBlockD1
	{
		uint m_sizeBytes;				// Size of this empty memory block (bytes)
		MemoryBlockD1* mp_blockPrev;	// Pointer to previous sequential empty memory block
		MemoryBlockD1* mp_blockNext;				// Pointer to next sequential empty memory block
	};

	uint m_poolSizeBytes;	// Size of memory pool (Bytes)
	uchar* mp_pool;			// Memory pool
	
	MemoryBlockD1* mp_listStart;	// Starting point of the sequential MemoryBlock linked-list

#define BLOCKD1_SIZE_MIN_BYTES	(sizeof(MemoryBlockD1))	// We need to be able to fit our tracking data in empty blocks

public:
// Setup
	MemoryManagerD1();
	MemoryManagerD1(uint memBytes);
	~MemoryManagerD1();


	bool AllocateMemoryPool(uint memBytes);
	void ReleaseMemory();

// Alloc/Dealloc
	void* Allocate(uint sizeBytes);
	void Deallocate(void* pMem);

// Helper functions
protected:
	MemoryBlockD1* CreateMemoryBlock(void* pAddr, uint sizeBytes);
	void InsertMemoryBlock(MemoryBlockD1* pBlockM, MemoryBlockD1* pBlockL, MemoryBlockD1* pBlockR);
	bool TryCoalesceBlocks(MemoryBlockD1* pBlockL, MemoryBlockD1* pBlockR);
};