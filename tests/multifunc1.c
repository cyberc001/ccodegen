// Тест multifunc1:
// Несколько функций с большим количеством вложенных конструкций, операций декремента

unsigned map_alloc(void* vaddr, unsigned long usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (PAGE_SIZE - 1)) / PAGE_SIZE;

	if(flags & VMEM_FLAG_MAINTAIN_CONTINUITY){
		void* paddr = allocator_alloc_align(usize * PAGE_SIZE, PAGE_SIZE);
		if(paddr == (void*)-1)
			return VMEM_ERR_NOSPACE;
		for(; usize-- && (unsigned long)paddr % PAGE_SIZE2 > 0 && (unsigned long)vaddr % PAGE_SIZE2 > 0; vaddr += PAGE_SIZE)
			MAP_PAGE(vaddr, paddr);
		for(; usize >= PAGE_SIZE2 / PAGE_SIZE; usize -= PAGE_SIZE2 / PAGE_SIZE)
			MAP_PAGE2(vaddr, paddr);
		for(; usize-- > 0; vaddr += PAGE_SIZE)
			MAP_PAGE(vaddr, paddr);
	}
	else{
		for(; usize-- && (unsigned long)vaddr % PAGE_SIZE2 > 0; vaddr += PAGE_SIZE)
			MAP_PAGE_ALLOC(vaddr); 
		for(; usize-- >= PAGE_SIZE2 / PAGE_SIZE; vaddr += PAGE_SIZE2)
			MAP_PAGE2_ALLOC(vaddr);
		for(; --usize > 0; vaddr += PAGE_SIZE)
			MAP_PAGE_ALLOC(vaddr);
	}
	return 0;
}

void* map_phys(void* vaddr, void** paddr, unsigned long int usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (PAGE_SIZE - 1)) / PAGE_SIZE;

	allocator_alloc_addr(usize * PAGE_SIZE, paddr);

	for(; usize-- && (unsigned long)paddr % PAGE_SIZE2 > 0 && (unsigned long)vaddr % PAGE_SIZE2 > 0; vaddr += PAGE_SIZE)
		MAP_PAGE(vaddr, paddr);
	for(; usize-- >= PAGE_SIZE2 / PAGE_SIZE; paddr += PAGE_SIZE2)
		MAP_PAGE2(vaddr, paddr);
	for(; usize-- > 0; vaddr += PAGE_SIZE)
		MAP_PAGE(vaddr, paddr);
	return NULL;
}

void unmap(void* vaddr, unsigned usize, int* flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (get_mem_unit_size() - 1)) / PAGE_SIZE;

	for(; usize-- && (unsigned long)vaddr % PAGE_SIZE2 > 0; vaddr += PAGE_SIZE) 
		UNMAP_PAGE(vaddr);
	for(; usize-- >= PAGE_SIZE2 / PAGE_SIZE; vaddr += PAGE_SIZE2)
		UNMAP_PAGE2(vaddr);
	for(; usize-- > 0; vaddr += PAGE_SIZE) 
		UNMAP_PAGE(vaddr);
}
