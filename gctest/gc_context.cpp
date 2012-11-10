#include "core.h"
#include "utils.h"
#include <iostream>
#include <utility>
#include <cstring>

using std::cout;
using std::cerr;
using std::endl;
using std::queue;
using std::strcmp;

gc_context::gc_context(void* stack_start) : stack_start(stack_start),
		last_mark_id(0),
		last_alloc_heap(0),
		first_heap((char*) UINTPTR_MAX),
		last_heap(nullptr) {}

size_t gc_context::full_compute_class_size(class_type* cls) {
	size_t size;

	if (cls->base_type) {
		size = full_compute_class_size(cls->base_type);
	}
	else {
		size = sizeof(core_representation);
	}

	for (field& field : cls->fields) {
		if (field.flags.is_static) {
			continue;
		}

		field.field_offset = size;
		if (field.type[0] == '[') {
			size = align(size, sizeof(void*));
			size += sizeof(void*);
		}
		else if (field.type[0] == 'L') {
			size = align(size, sizeof(void*));
			size += sizeof(void*);
		}
		else if (field.type[0] == 'I') {
			size = align(size, sizeof(uint32_t));
			size += sizeof(uint32_t);
		}
		else {
			cerr << "Unrecognized field type " << field.type << endl;
			abort();
		}
	}

	return size;
}

void gc_context::compute_sizes() {
	for (class_type* cls : class_types) {
		cls->computed_size = full_compute_class_size(cls);
	}
}

class_type* gc_context::class_by_name(const char* class_name, size_t len) {
	for (class_type* cls : class_types) {
		if (strncmp(cls->full_name, class_name, len) == 0) {
			return cls;
		}
	}

	cerr << "Class not found" << endl;
	abort();
}

const class_type* gc_context::class_by_name(const char* class_name, size_t len) const {
	for (const class_type* cls : class_types) {
		if (strncmp(cls->full_name, class_name, len) == 0) {
			return cls;
		}
	}

	cerr << "Class not found" << endl;
	abort();
}

size_t gc_context::measure_class_size(const char* class_name, size_t len) const {
	return class_by_name(class_name, len)->computed_size;
}

size_t gc_context::measure_direct_heap_size(const char* type_name) const {
	switch (type_name[0]) {
	case 'L': return sizeof(core_representation);
	case '[': return sizeof(array_representation);
	case 'I': return sizeof(uint32_t);
	}

	cerr << "measure_direct_heap_size Unrecognized type " << type_name << endl;
	abort();
}

size_t gc_context::measure_array_content_size(const char* content_type, size_t len) const {
	return len * measure_direct_heap_size(content_type);
}

core_representation* gc_context::alloc_class(const char* type) {
	//cout << "Allocate class " << type << endl;

	const char* class_name = type + 1;
	const char* semicolon_pos = strchr(class_name, ';');
	size_t len = semicolon_pos - class_name;

	class_type* cls = class_by_name(class_name, len);

	size_t class_size = cls->computed_size;

	core_representation* repr = (core_representation*) alloc(class_size, true);
	memset(repr, 0, class_size);
	repr->type = type;
	repr->last_mark = last_mark_id;

	return repr;
}

array_representation* gc_context::alloc_array(const char* type, size_t length) {
	const char* content_type = type + 1;

	size_t content_size = measure_array_content_size(content_type, length);
	void* content = alloc(content_size, false);

	array_representation* repr = (array_representation*) alloc(sizeof(array_representation), true);
	repr->array_length = length;
	repr->content = content;
	repr->core.type = type;
	repr->core.last_mark = last_mark_id;

	return repr;
}

void gc_context::log_headers() {
	for (class_type* cls : class_types) {
		cout << "full_name=" << cls->full_name << endl;
		if (cls->base_type) {
			cout << "base_type=" << cls->base_type->full_name << endl;
		}
		else {
			cout << "no base_type" << endl;
		}
		cout << "computed_size=" << cls->computed_size << endl;
		cout << endl;
	}
}

void* gc_context::try_alloc(size_t size, bool is_gc_object) {
	if (last_alloc_heap >= heaps.size()) {
		last_alloc_heap = 0;
	}
	for (size_t aheap = last_alloc_heap; aheap < heaps.size(); ++aheap) {
		gc_heap& heap = heaps[aheap];
		void* chunk = heap.try_alloc(size, is_gc_object);
		if (chunk) {
			last_alloc_heap = aheap;
			return chunk;
		}
	}
	for (size_t aheap = 0; aheap < last_alloc_heap; ++aheap) {
		gc_heap& heap = heaps[aheap];
		void* chunk = heap.try_alloc(size, is_gc_object);
		if (chunk) {
			last_alloc_heap = aheap;
			return chunk;
		}
	}

	return nullptr;
}

void* gc_context::alloc(size_t size, bool is_gc_object) {
	//cout << "alloc(" << size << ")" << endl;

	void* chunk = try_alloc(size, is_gc_object);
	if (chunk) {
		return chunk;
	}

	//cout << "Not enough space." << endl;

	if (heaps.size() > 0) { //GC would be worthless otherwise
		perform_gc();

		chunk = try_alloc(size, is_gc_object);
		if (chunk) {
			return chunk;
		}
	}

	//cout << "Allocate new chunk" << endl;

	size_t new_heap_size = PREFERRED_HEAP_SIZE;
	if (new_heap_size < size) {
		new_heap_size = size;
	}
	heaps.push_back(gc_heap(new_heap_size));
	gc_heap& heap = heaps.back();
	if (heap.heap_aligned < first_heap) {
		first_heap = heap.heap_aligned;
	}
	if (heap.heap + heap.heap_size > last_heap) {
		last_heap = heap.heap + heap.heap_size;
	}

	chunk = heap.try_alloc(size, is_gc_object);
	//cout << "allocated " << chunk << endl;
	return chunk;
}

void gc_context::perform_gc() {
	mark();
	sweep();
}

gc_heap* gc_context::find_owner_heap(void* obj, bool is_gc_object) {
	if (obj < first_heap || obj >= last_heap) {
		return nullptr;
	}

	for (gc_heap& heap : heaps) {
		if (heap.contains(obj, is_gc_object)) {
			return &heap;
		}
	}

	return nullptr;
}

const gc_heap* gc_context::find_owner_heap(void* obj, bool is_gc_object) const {
	if (obj < first_heap || obj >= last_heap) {
		return nullptr;
	}

	for (const gc_heap& heap : heaps) {
		if (heap.contains(obj, is_gc_object)) {
			return &heap;
		}
	}

	return nullptr;
}

bool gc_context::is_heap_object(void* obj) const {
	return find_owner_heap(obj, true) != nullptr;
}

void gc_context::mark() {
	uintptr_t stack_pos = uintptr_t(get_stack_pointer());
	++last_mark_id;

	queue<core_representation*> objects_to_mark;

	//Mark stack
	for (uintptr_t pos = stack_pos; pos < uintptr_t(stack_start); pos += sizeof(void*)) {
		void* value_at = *((void**) pos);

		//cout << "Found " << value_at << ": ";

		if (is_heap_object(value_at)) {
			//cout << "Heap object" << endl;
			objects_to_mark.push((core_representation*) value_at);
		}
	}

	while (!objects_to_mark.empty()) {
		mark(objects_to_mark.front(), objects_to_mark);
		objects_to_mark.pop();
	}
}

void gc_context::mark(core_representation* object, queue<core_representation*>& pending_list) {
	if (object->last_mark == last_mark_id) {
		return;
	}

	object->last_mark = last_mark_id;

	if (object->type[0] == 'L') {
		const char* name_begin = object->type + 1;
		const char* semicolon_position = strchr(name_begin, ';');
		size_t len = semicolon_position - name_begin;
		const class_type* cls = class_by_name(name_begin, len);

		mark_fields(cls, object, pending_list);
	}
	else if (object->type[0] == '[') {
		const char* content_type = object->type + 1;

		mark_array(content_type, object, pending_list);
	}
}

void gc_context::mark_fields(const class_type* cls, core_representation* object,
		queue<core_representation*>& pending_list) {
	for (; cls; cls = cls->base_type) {
		for (const field& field : cls->fields) {
			if (!field.flags.is_static) { //We'll handle statics elsewhere
				mark_field(field, object, pending_list);
			}
		}
	}
}

void gc_context::mark_field(const field& field, core_representation* object,
		queue<core_representation*>& pending_list) {
	if (field.type[0] == 'L' || field.type[0] == '[') {
		core_representation* location =
				*((core_representation**) ((char*) object + field.field_offset));

		if (location) {
			//Not null

			pending_list.push(location);
		}
	}
}

void gc_context::mark_array(const char* content_type, core_representation* object,
		queue<core_representation*>& pending_list) {

	array_representation* array = (array_representation*) object;
	void* content = array->content;

	switch (content_type[0]) {
	case 'L':
		for (size_t i = 0; i < array->array_length; ++i) {
			pending_list.push(((core_representation**) content)[i]);
		}
		break;
	case '[':
		for (size_t i = 0; i < array->array_length; ++i) {
			pending_list.push((core_representation*) ((array_representation**) content)[i]);
		}
		break;
		//For the remaining types, we don't even bother checking for them.
	}
}

void gc_context::sweep() {
	//cout << "sweep " << (int) last_mark_id << endl;

	for (gc_heap& heap : heaps) {
		for (size_t i = 0; i < heap.heap_starts.size(); ++i) {
			if (!heap.heap_starts[i]) {
				continue;
			}

			core_representation* repr = (core_representation*) (heap.heap_aligned + i * HEAP_UNIT_SIZE);

			//cout << "Free " << repr << endl;

			if (repr->last_mark != last_mark_id) {
				//Free this object
				heap.heap_starts[i] = 0;

				size_t object_size;

				if (repr->type[0] == '[') {
					object_size = sizeof(array_representation);
					array_representation* arepr = (array_representation*) repr;
					size_t content_size = measure_array_content_size(repr->type + 1, arepr->array_length);

					gc_heap* owner_heap = find_owner_heap(arepr->content, false);

					owner_heap->free_non_gc_object(arepr->content, content_size);
				}
				else if (repr->type[0] == 'L') {
					const char* class_name = repr->type + 1;
					const char* semicolon_position = strchr(class_name, ';');
					size_t len = semicolon_position - class_name;

					object_size = class_by_name(class_name, len)->computed_size;
				}
				else {
					cerr << "sweep Unrecognized type " << repr->type << endl;
					abort();
				}

				size_t block_size = div_round_up(object_size, HEAP_UNIT_SIZE);
				heap.heap_bitset.unset_range(i, block_size);
			}
		}
	}
}
