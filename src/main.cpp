#include <stdio.h>

#include "MemoryManager.h"

int main()
{
	printf("Hello World!\n");

	MemoryAllocator mem_pool;
	mem_pool.AllocateMemory(20, 64);	// Request 64 20-byte blocks (should be rounded up to 24)

	printf("Block size: %d\n", mem_pool.GetBlockSize());
	printf("Memory pool is located @ %p\n", mem_pool[0]);
	printf("Memory pool[7] located @ %p\n", mem_pool[7]);

	while (true) {};
	return 0;
}