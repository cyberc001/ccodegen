// Тест singlefunc1:
// Сложная функция с относительно произвольным форматированием и разнообразием операторов.

unsigned char detect_cpus(unsigned char* rsdt, unsigned char* lapic_ids, unsigned char* bsp_lapic_id)
{
	unsigned char *ent;
	unsigned char *ent_end;
	unsigned ln;

	unsigned long lapic_ptr = 0, ioapic_ptr = 0;
	unsigned char core_num = 0;

	void* rsdt_aligned = rsdt - (unsigned long)rsdt % get_mem_unit_size();
	size_t rsdt_sz = 4096 + (unsigned long)rsdt % get_mem_unit_size();
	map_phys(rsdt_aligned, rsdt_aligned, (rsdt_sz + (get_mem_unit_size() - 1)) / get_mem_unit_size(), 0);
	ent_end = rsdt + 36;
	for(ln = *((unsigned*)(rsdt + 4)); 
		ent_end < rsdt + ln;
		ent_end += rsdt[0] == 'X' ? 8 : 4){
		ent = (unsigned char*)(rsdt[0] == 'X' ? *((unsigned long*)ent_end) : *((unsigned*)ent_end)); // pointer to XSDT is 8 bytes, pointer to RSDT is 4 bytes
		if(!memcmp(ent, "APIC", 4)){
			lapic_ptr = (unsigned long)(*((unsigned*)(ent + 0x24)));
			ent_end = ent + *((unsigned*)(ent + 4));
			// MADT consists of variable-length entries (https://wiki.osdev.org/MADT):
			// ent[0] is entry type, ent[1] is entry length
			for(ent += 44; // skip the ACPI table header
					ent < ent_end; ent += ent[1]){ // iterate on APIC entries
				if(ent[0] == 0){ // processor local APIC
					if(ent[4] & 1)
						lapic_ids[core_num++] = ent[3];
				} else if(ent[0] == 1){ // IOAPIC
					ioapic_ptr = (unsigned long)*((unsigned*)(ent + 4));
				} else if(ent[0] == 5){ // LAPIC
					lapic_ptr = *((unsigned long*)(ent + 4));
				}
			}
			break;
		}
	}

	// get "initial APIC ID" (BSP APIC ID)
	unsigned eax, ebx, ecx, edx;
	cpuid(1, 0, &eax, &ebx, &ecx, &edx);
	*bsp_lapic_id = ebx >> 24;

	return core_num;
}
