int comp2(unsigned a, unsigned b)
{
	unsigned sum = a + b;
	sum += (sum & ~0xFFFF) >> 16;
	sum += (sum & ~0xFFFF) >> 16;
	return sum;
}

int ethernet_crc(data, size_t data_ln)
{
	return comp2(data[0], data[1]);
}
