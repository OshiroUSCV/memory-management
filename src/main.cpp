#include <stdio.h>

#include "MemoryManager.h"

int main()
{
	printf("Hello World!\n");

	MemoryManagerD1 mem_pool;
	mem_pool.AllocateMemoryPool(4096);	

	
	void* p_block_1 = mem_pool.Allocate(64);
	void* p_block_2 = mem_pool.Allocate(1024);
	void* p_block_3 = mem_pool.Allocate(520);

	mem_pool.Deallocate(p_block_2);
	p_block_2 = mem_pool.Allocate(128);
	mem_pool.Deallocate(p_block_1);


	//mem_pool.AllocateMemory(20, 64);	// Request 64 20-byte blocks (should be rounded up to 24)
	//printf("Block size: %d\n", mem_pool.GetBlockSize());
	//printf("Memory pool is located @ %p\n", mem_pool[0]);
	//printf("Memory pool[7] located @ %p\n", mem_pool[7]);

	while (true) {};
	return 0;
}