#ifndef CORE_H_
#define CORE_H_

#include <cstdint>
#include <vector>
#include <string>
#include <queue>
#include "fast_bitset.h"
#include "x86_64.h"

#define TYPE_ARRAY 0

typedef uint8_t mark_id_t;
typedef uint8_t fixed_count_t;

#define PREFERRED_HEAP_SIZE 0x1000
#define HEAP_UNIT_SIZE sizeof(core_representation)

struct class_type;
struct gc_heap;

struct core_representation {
	const char* type;
	mark_id_t last_mark;
};

struct array_representation {
	core_representation core;
	size_t array_length;
	void* content;
};

struct field_flags {
	unsigned is_public : 1;
	unsigned is_static : 1;
};

struct field {
	const char* type;
	field_flags flags;
	size_t field_offset;
};

struct class_type {
	const char* full_name;
	class_type* base_type; //NULL for no base
	size_t computed_size;
	std::vector<field> fields;
	void* static_field_data;
};

struct gc_heap {
	size_t heap_size;
	char* heap;
	char* heap_aligned;
	fast_bitset heap_bitset;
	std::vector<bool> heap_starts;

	gc_heap(size_t heap_size);
	gc_heap(const gc_heap& other) = delete;
	gc_heap(gc_heap&& other);
	~gc_heap();

	void* try_alloc(size_t size, bool is_gc_object);
	void free_non_gc_object(void* obj, size_t size);
	inline bool contains(void* obj, bool is_gc_object) const {
		if(obj >= heap_aligned && obj < heap_aligned + heap_size) {
			uintptr_t block_num = uintptr_t((char*) obj - heap_aligned) / HEAP_UNIT_SIZE;
			return !is_gc_object || heap_starts[block_num];
		}

		return false;
	}

	gc_heap& operator=(const gc_heap& other) = delete;
};

class gc_context {
	void* stack_start;
	std::vector<class_type*> class_types;
	std::vector<gc_heap> heaps;
	mark_id_t last_mark_id;
	size_t last_alloc_heap;
	char* first_heap;
	char* last_heap;

	void mark();
	void mark(core_representation* object, std::queue<core_representation*>& pending_list);
	void mark_fields(const class_type* cls, core_representation* object,
			std::queue<core_representation*>& pending_list);
	void mark_field(const field& field, core_representation* object,
			std::queue<core_representation*>& pending_list);
	void mark_array(const char* content_type, core_representation* object,
			std::queue<core_representation*>& pending_list);
	void sweep();
public:
	gc_context(void* stack_start);

	void push_class_type(class_type* type) { class_types.push_back(type); }
	size_t count_heaps() { return heaps.size(); }
	gc_heap* find_owner_heap(void* content_location, bool is_gc_object);
	const gc_heap* find_owner_heap(void* content_location, bool is_gc_object) const;

	size_t full_compute_class_size(class_type* cls);
	void compute_sizes();

	class_type* class_by_name(const char* class_name, size_t len);
	const class_type* class_by_name(const char* class_name, size_t len) const;

	size_t measure_class_size(const char* class_name, size_t len) const;
	size_t measure_direct_heap_size(const char* type_name) const;
	size_t measure_array_content_size(const char* content_type, size_t len) const;

	core_representation* alloc_class(const char* class_name);
	array_representation* alloc_array(const char* inner_type, size_t length);

	//Tries to allocate, but does not trigger GC nor allocates new space
	void* try_alloc(size_t size, bool is_gc_object);

	void* alloc(size_t size, bool is_gc_object);

	bool is_heap_object(void* obj) const;

	void perform_gc();

	void log_headers();
};

#endif /* CORE_H_ */
