#ifndef CORE_H_
#define CORE_H_

#include <cstdint>
#include <vector>
#include <string>
#include <queue>
#include "fast_bitset.h"
#ifdef PLATFORM_X64
#include "x86_64.h"
#else
#ifdef PLATFORM_X86
#include "x86.h"
#else
#error "No platform defined"
#endif
#endif

typedef uint8_t mark_id_t;
typedef uint8_t fixed_count_t;

#define PREFERRED_HEAP_SIZE 0x1000
#define HEAP_UNIT_SIZE sizeof(core_representation)

struct class_type;
struct gc_heap;
struct type_info;

struct core_representation {
	type_info* type;
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
	type_info* type;
	field_flags flags;
	size_t field_offset;
};

struct class_type {
	std::string full_name;
	class_type* base_type; //NULL for no base
	size_t computed_size;
	size_t static_size;
	std::vector<field> fields;
	type_info* owned_type;
	void* static_field_data;
};

typedef enum {
	TYPE_INT32,

	LAST_PRIMITIVE_TYPE = TYPE_INT32,

	TYPE_ARRAY,
	TYPE_CLASS_OBJECT
} type_category_t;

struct type_info {
	type_category_t type_category;
	type_info* array_type;
};

struct array_type_info {
	type_info base_type;
	type_info* content_type;
};

struct class_type_info {
	type_info base_type;
	class_type* cls;
};

struct gc_heap {
	size_t heap_size;
	char* heap;
	char* heap_aligned;
	fast_bitset heap_bitset;
	fast_bitset heap_starts;

	gc_heap(size_t heap_size);
	gc_heap(const gc_heap& other) = delete;
	gc_heap(gc_heap&& other);
	~gc_heap();

	void* try_alloc(size_t size, bool is_gc_object);
	void free_non_gc_object(void* obj, size_t size);
	inline bool contains(void* obj, bool is_gc_object) const {
		if(obj >= heap_aligned && obj < heap_aligned + heap_size) {
			if (uintptr_t((char*) obj - heap_aligned) % HEAP_UNIT_SIZE != 0) {
				//Not aligned - invalid
				return false;
			}
			uintptr_t block_num = uintptr_t((char*) obj - heap_aligned) / HEAP_UNIT_SIZE;
			if (is_gc_object) {
				return heap_starts.get(block_num);
			}
			else {
				return true;
			}
		}

		return false;
	}

	gc_heap& operator=(const gc_heap& other);
};

class gc_context {
	type_info primitive_types[LAST_PRIMITIVE_TYPE + 1];
	void* stack_start;
	std::vector<class_type*> class_types;
	std::vector<gc_heap> heaps;
	mark_id_t last_mark_id;
	size_t last_alloc_heap;
	char* first_heap;
	char* last_heap;

	void mark();
	void mark_conservative_region(uint32_t start, uint32_t end,
			std::queue<core_representation*>& pending_list);
	void mark(core_representation* object, std::queue<core_representation*>& pending_list);
	void mark_fields(const class_type* cls, core_representation* object,
			std::queue<core_representation*>& pending_list);
	void mark_field(const field& field, core_representation* object,
			std::queue<core_representation*>& pending_list);
	void mark_array(const type_info* content_type, core_representation* object,
			std::queue<core_representation*>& pending_list);
	void sweep();
public:
	gc_context(void* stack_start);

	void push_class_type(class_type* type) { class_types.push_back(type); }
	size_t count_heaps() { return heaps.size(); }
	gc_heap* find_owner_heap(void* content_location, bool is_gc_object);
	const gc_heap* find_owner_heap(void* content_location, bool is_gc_object) const;

	size_t full_compute_class_size(class_type* cls);
	size_t full_compute_class_static_size(class_type* cls);
	void compute_sizes();
	void compute_static_sizes();

	class_type* class_by_name(std::string class_name);
	const class_type* class_by_name(std::string class_name) const;

	size_t measure_class_size(type_info* type) const;
	size_t measure_direct_heap_size(type_info* type) const;
	size_t measure_array_content_size(type_info* content_type, size_t len) const;

	core_representation* alloc_class(type_info* class_type);
	array_representation* alloc_array(type_info* inner_type, size_t length);

	//Tries to allocate, but does not trigger GC nor allocates new space
	void* try_alloc(size_t size, bool is_gc_object);

	void* alloc(size_t size, bool is_gc_object);

	bool is_heap_object(void* obj) const;

	void prepare_static_fields();

	void perform_gc();

	void log_headers();

	type_info* get_type_int32();
	type_info* get_type_array(type_info* base_type);
	type_info* get_class_type(class_type* cls);
};

#endif /* CORE_H_ */
