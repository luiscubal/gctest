#ifndef FAST_BITSET_H
#define FAST_BITSET_H

#include <cstdint>
#include <vector>
#include <limits>
#include <utility>

class fast_bitset {
	size_t bitcount;
	std::vector<uint32_t> bits;
public:
	inline fast_bitset() : bitcount(0) {}
	inline fast_bitset(size_t bitcount) : bitcount(bitcount), bits((bitcount + 31) >> 5) {}
	inline fast_bitset(const fast_bitset& other) : bitcount(other.bitcount), bits(other.bits) {}
	inline fast_bitset(fast_bitset&& other) : bitcount(other.bitcount), bits(std::move(other.bits)) {}

	inline fast_bitset& operator=(const fast_bitset& other) {
		bitcount = other.bitcount;
		bits = other.bits;

		return *this;
	}

	inline size_t size() const { return bitcount; }
	inline bool get(size_t idx) const { return bits[idx >> 5] & (1 << (idx & 31)); }
	inline void set(size_t idx) { bits[idx >> 5] |= (1 << (idx & 31)); }
	inline void unset(size_t idx) { bits[idx >> 5] &= ~(1 << (idx & 31)); }

	inline size_t find_next_set(size_t start, size_t max_hint) const {
		//First, we align the start
		while ((start & 31) != 0 && max_hint > 0) {
			if (start >= size() || get(start)) {
				return start;
			}
			++start;
			--max_hint;
		}

		//Aligned, now mass check
		while (start < size() && max_hint > 0) {
			if (bits[start >> 5] == 0x00000000) { //Quickly rule out empty spaces
				start += 32;
				if (max_hint < 32) {
					//We exceeded the limit
					return size();
				}
				max_hint -= 32;
			}
			else { //Manual check
				for (int i = 0; i < 32; ++i) {
					if (max_hint == 0) {
						//We exceeded the limit
						return size();
					}

					if (get(start)) {
						return start;
					}
					++start;
					--max_hint;
				}
			}
		}

		return size();
	}

	inline size_t find_next_unset(size_t start, size_t max_hint) const {
		//First, we align the start
		while ((start & 31) != 0 && max_hint > 0) {
			if (start >= size() || !get(start)) {
				return start;
			}
			++start;
			--max_hint;
		}

		//Aligned, now mass check
		while (start < size() && max_hint > 0) {
			if (bits[start >> 5] == 0xFFFFFFFF) { //Quickly rule out empty spaces
				start += 32;
				if (max_hint < 32) {
					//We exceeded the limit
					return size();
				}
				max_hint -= 32;
			}
			else { //Manual check
				for (int i = 0; i < 32; ++i) {
					if (max_hint == 0) {
						//We exceeded the limit
						return size();
					}

					if (!get(start)) {
						return start;
					}
					++start;
					--max_hint;
				}
			}
		}

		return size();
	}

	inline void unset_range(size_t start, size_t length) {
		//First we align the start
		while ((start & 31) != 0 && length > 0) {
			unset(start++);
			--length;
		}

		while (length >= 32) {
			bits[start >> 5] = 0;
			length -= 32;
			start += 32;
		}

		while (length > 0) {
			unset(start++);
			--length;
		}
	}

	inline void set_range(size_t start, size_t length) {
		//First we align the start
		while ((start & 31) != 0 && length > 0) {
			set(start++);
			--length;
		}

		while (length >= 32) {
			bits[start >> 5] = 0xFFFFFFFF;
			length -= 32;
			start += 32;
		}

		while (length > 0) {
			set(start++);
			--length;
		}
	}
};

#endif
