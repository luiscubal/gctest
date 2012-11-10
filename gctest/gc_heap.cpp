#include "core.h"
#include "utils.h"
#include <iostream>
#include <cstdlib>

using std::cerr;
using std::endl;
using std::move;
using std::malloc;
using std::free;

gc_heap::gc_heap(size_t heap_size) : heap_size(align(heap_size, HEAP_UNIT_SIZE)),
		heap_bitset(div_round_up(heap_size, HEAP_UNIT_SIZE)), heap_starts(heap_bitset.size()) {

	heap = (char*) malloc(this->heap_size);
	heap_aligned = (char*) align((uintptr_t) heap, HEAP_UNIT_SIZE);

	//cout << "Create heap in " << (void*) heap << ", size " << this->heap_size << endl;
	//cout << "aligned at " << (void*) heap_aligned << endl;

	if (!heap) {
		cerr << "gc_context::setup_heap(" << this->heap_size << ") failed." << endl;
		abort();
	}
}

gc_heap::gc_heap(gc_heap&& other) : heap_size(other.heap_size), heap(other.heap),
		heap_aligned(other.heap_aligned) {
	other.heap_size = 0;
	other.heap = nullptr;
	other.heap_aligned = nullptr;

	heap_bitset = move(other.heap_bitset);
	heap_starts = move(other.heap_starts);
}

gc_heap::~gc_heap() {
	if (heap) {
		free(heap);
	}
}

void* gc_heap::try_alloc(size_t size, bool is_gc_object) {
	if (size > heap_size) {
		//Impossible to fit
		return nullptr;
	}

	ssize_t block_start = -1;

	for (size_t i = heap_bitset.find_next_unset(0, heap_bitset.size()); i < heap_bitset.size();) {
		if (heap_bitset.get(i)) {
			block_start = -1;
			i = heap_bitset.find_next_unset(i + 1, heap_bitset.size());
			continue;
		}

		if (block_start != -1) {
			size_t block_size = i - block_start + 1;
			if (block_size * HEAP_UNIT_SIZE >= size) {
				if (is_gc_object) {
					heap_starts[block_start] = true;
				}
				for (size_t j = block_start; j <= i; ++j) {
					heap_bitset.set(j);
				}
				return heap_aligned + block_start * HEAP_UNIT_SIZE;
			}
		}
		else {
			//cout << "Free block starting at " << i << endl;
			block_start = i;
			if (HEAP_UNIT_SIZE >= size) {
				heap_bitset.set(i);
				heap_starts[i] = true;
				return heap_aligned + block_start * HEAP_UNIT_SIZE;
			}
		}

		++i;
	}

	//Not enough space
	return nullptr;
}

void gc_heap::free_non_gc_object(void* obj, size_t size) {
	size_t block_size = div_round_up(size, HEAP_UNIT_SIZE);
	size_t start_idx = ((char*) obj - heap_aligned) / HEAP_UNIT_SIZE;
	heap_bitset.unset_range(start_idx, block_size);
}
