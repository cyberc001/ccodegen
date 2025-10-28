unsigned int PRIME32_1 = 2654435761;
unsigned int PRIME32_2 = 2246822519;
unsigned int PRIME32_3 = 3266489917;
unsigned int PRIME32_4 =  668265263;
unsigned int PRIME32_5 =  374761393;

unsigned long int PRIME64_1 = 11400714785074694791;
unsigned long int PRIME64_2 = 14029467366897019727;
unsigned long int PRIME64_3 =  1609587929392839161;
unsigned long int PRIME64_4 =  9650029242287828579;
unsigned long int PRIME64_5 =  2870177450012600261;

void xxh32_copy_state(struct xxh32_state *dst, struct xxh32_state *src)
{
	memcpy(dst, src, sizeof(*dst));
}
EXPORT_SYMBOL(xxh32_copy_state);

void xxh64_copy_state(struct xxh64_state *dst, struct xxh64_state *src)
{
	memcpy(dst, src, sizeof(*dst));
}
EXPORT_SYMBOL(xxh64_copy_state);

unsigned int xxh32_round(unsigned int seed, unsigned int input)
{
	seed += input * PRIME32_2;
	seed = xxh_rotl32(seed, 13);
	seed *= PRIME32_1;
	return seed;
}

unsigned int xxh32(void *input, size_t len, unsigned int seed)
{
	unsigned char *p = (unsigned char *)input;
	unsigned char *b_end = p + len;
	unsigned int h32;

	if (len >= 16) {
		unsigned char *limit = b_end - 16;
		unsigned int v1 = seed + PRIME32_1 + PRIME32_2;
		unsigned int v2 = seed + PRIME32_2;
		unsigned int v3 = seed + 0;
		unsigned int v4 = seed - PRIME32_1;

		while(p <= limit){
			v1 = xxh32_round(v1, get_unaligned_le32(p));
			p += 4;
			v2 = xxh32_round(v2, get_unaligned_le32(p));
			p += 4;
			v3 = xxh32_round(v3, get_unaligned_le32(p));
			p += 4;
			v4 = xxh32_round(v4, get_unaligned_le32(p));
			p += 4;
		}

		h32 = xxh_rotl32(v1, 1) + xxh_rotl32(v2, 7) +
			xxh_rotl32(v3, 12) + xxh_rotl32(v4, 18);
	} else {
		h32 = seed + PRIME32_5;
	}

	h32 += (unsigned int)len;

	while (p + 4 <= b_end) {
		h32 += get_unaligned_le32(p) * PRIME32_3;
		h32 = xxh_rotl32(h32, 17) * PRIME32_4;
		p += 4;
	}

	while (p < b_end) {
		h32 += (*p) * PRIME32_5;
		h32 = xxh_rotl32(h32, 11) * PRIME32_1;
		p++;
	}

	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;

	return h32;
}
EXPORT_SYMBOL(xxh32);

unsigned long int xxh64_round(unsigned long int acc, unsigned long int input)
{
	acc += input * PRIME64_2;
	acc = xxh_rotl64(acc, 31);
	acc *= PRIME64_1;
	return acc;
}

unsigned long int xxh64_merge_round(unsigned long int acc, unsigned long int val)
{
	val = xxh64_round(0, val);
	acc ^= val;
	acc = acc * PRIME64_1 + PRIME64_4;
	return acc;
}

unsigned long int xxh64(void *input, size_t len, unsigned long int seed)
{
	unsigned char *p = (unsigned char *)input;
	unsigned char *b_end = p + len;
	unsigned long int h64;

	if (len >= 32) {
		unsigned char *limit = b_end - 32;
		unsigned long int v1 = seed + PRIME64_1 + PRIME64_2;
		unsigned long int v2 = seed + PRIME64_2;
		unsigned long int v3 = seed + 0;
		unsigned long int v4 = seed - PRIME64_1;

		while(p <= limit){
			v1 = xxh64_round(v1, get_unaligned_le64(p));
			p += 8;
			v2 = xxh64_round(v2, get_unaligned_le64(p));
			p += 8;
			v3 = xxh64_round(v3, get_unaligned_le64(p));
			p += 8;
			v4 = xxh64_round(v4, get_unaligned_le64(p));
			p += 8;
		}

		h64 = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) +
			xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);
		h64 = xxh64_merge_round(h64, v1);
		h64 = xxh64_merge_round(h64, v2);
		h64 = xxh64_merge_round(h64, v3);
		h64 = xxh64_merge_round(h64, v4);

	} else {
		h64  = seed + PRIME64_5;
	}

	h64 += (unsigned long int)len;

	while (p + 8 <= b_end) {
		unsigned long int k1 = xxh64_round(0, get_unaligned_le64(p));

		h64 ^= k1;
		h64 = xxh_rotl64(h64, 27) * PRIME64_1 + PRIME64_4;
		p += 8;
	}

	if (p + 4 <= b_end) {
		h64 ^= (unsigned long int)(get_unaligned_le32(p)) * PRIME64_1;
		h64 = xxh_rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
		p += 4;
	}

	while (p < b_end) {
		h64 ^= (*p) * PRIME64_5;
		h64 = xxh_rotl64(h64, 11) * PRIME64_1;
		p++;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	return h64;
}
EXPORT_SYMBOL(xxh64);

void xxh32_reset(struct xxh32_state *statePtr, unsigned int seed)
{
	struct xxh32_state state;

	memset(&state, 0, sizeof(state));
	state.v1 = seed + PRIME32_1 + PRIME32_2;
	state.v2 = seed + PRIME32_2;
	state.v3 = seed + 0;
	state.v4 = seed - PRIME32_1;
	memcpy(statePtr, &state, sizeof(state));
}
EXPORT_SYMBOL(xxh32_reset);

void xxh64_reset(struct xxh64_state *statePtr, unsigned long int seed)
{
	struct xxh64_state state;

	memset(&state, 0, sizeof(state));
	state.v1 = seed + PRIME64_1 + PRIME64_2;
	state.v2 = seed + PRIME64_2;
	state.v3 = seed + 0;
	state.v4 = seed - PRIME64_1;
	memcpy(statePtr, &state, sizeof(state));
}
EXPORT_SYMBOL(xxh64_reset);

int xxh64_update(struct xxh64_state *state, void *input, size_t len)
{
	unsigned char *p = (unsigned char *)input;
	unsigned char *b_end = p + len;

	if (input == NULL)
		return -EINVAL;

	state->total_len += len;

	if (state->memsize + len < 32) {
		memcpy(((unsigned char *)state->mem64) + state->memsize, input, len);
		state->memsize += (unsigned int)len;
		return 0;
	}

	if (state->memsize) {
		unsigned long int *p64 = state->mem64;

		memcpy(((unsigned char *)p64) + state->memsize, input,
			32 - state->memsize);

		state->v1 = xxh64_round(state->v1, get_unaligned_le64(p64));
		p64++;
		state->v2 = xxh64_round(state->v2, get_unaligned_le64(p64));
		p64++;
		state->v3 = xxh64_round(state->v3, get_unaligned_le64(p64));
		p64++;
		state->v4 = xxh64_round(state->v4, get_unaligned_le64(p64));

		p += 32 - state->memsize;
		state->memsize = 0;
	}

	if (p + 32 <= b_end) {
		unsigned char *limit = b_end - 32;
		unsigned long int v1 = state->v1;
		unsigned long int v2 = state->v2;
		unsigned long int v3 = state->v3;
		unsigned long int v4 = state->v4;

		while(p <= limit){
			v1 = xxh64_round(v1, get_unaligned_le64(p));
			p += 8;
			v2 = xxh64_round(v2, get_unaligned_le64(p));
			p += 8;
			v3 = xxh64_round(v3, get_unaligned_le64(p));
			p += 8;
			v4 = xxh64_round(v4, get_unaligned_le64(p));
			p += 8;
		} 

		state->v1 = v1;
		state->v2 = v2;
		state->v3 = v3;
		state->v4 = v4;
	}

	if (p < b_end) {
		memcpy(state->mem64, p, (size_t)(b_end-p));
		state->memsize = (unsigned int)(b_end - p);
	}

	return 0;
}
EXPORT_SYMBOL(xxh64_update);

unsigned long int xxh64_digest(struct xxh64_state *state)
{
	unsigned char *p = (unsigned char *)state->mem64;
	UINT8_t *b_end = (unsigned char *)state->mem64 +
		state->memsize;
	unsigned long int h64;

	if (state->total_len >= 32) {
		unsigned long int v1 = state->v1;
		unsigned long int v2 = state->v2;
		unsigned long int v3 = state->v3;
		unsigned long int v4 = state->v4;

		h64 = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) +
			xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);
		h64 = xxh64_merge_round(h64, v1);
		h64 = xxh64_merge_round(h64, v2);
		h64 = xxh64_merge_round(h64, v3);
		h64 = xxh64_merge_round(h64, v4);
	} else {
		h64  = state->v3 + PRIME64_5;
	}

	h64 += (unsigned long int)state->total_len;

	while (p + 8 <= b_end) {
		unsigned long int k1 = xxh64_round(0, get_unaligned_le64(p));

		h64 ^= k1;
		h64 = xxh_rotl64(h64, 27) * PRIME64_1 + PRIME64_4;
		p += 8;
	}

	if (p + 4 <= b_end) {
		h64 ^= (unsigned long int)(get_unaligned_le32(p)) * PRIME64_1;
		h64 = xxh_rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
		p += 4;
	}

	while (p < b_end) {
		h64 ^= (*p) * PRIME64_5;
		h64 = xxh_rotl64(h64, 11) * PRIME64_1;
		p++;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	return h64;
}
EXPORT_SYMBOL(xxh64_digest);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("xxHash");
