#include "core.h"
#include "utils.h"
#include <iostream>
#include <utility>
#include <cstdlib>
#include <cstring>

using std::cout;
using std::cerr;
using std::endl;
using std::queue;
using std::string;
using std::malloc;
using std::memset;
using std::move;
using std::unique_ptr;

gc_context::gc_context(unique_ptr<gc_type_store> type_store, void* stack_start) :
		type_store(move(type_store)),
		stack_start(stack_start),
		last_mark_id(0),
		last_alloc_heap(0),
		first_heap((char*) UINTPTR_MAX),
		last_heap(nullptr) {

}

core_representation* gc_context::alloc_class(type_info* type) {
	class_type* cls = ((class_type_info*) type)->cls;

	size_t class_size = cls->computed_size;

	core_representation* repr = (core_representation*) alloc(class_size, true);
	memset(repr, 0, class_size);
	repr->type = type;
	repr->last_mark = last_mark_id;

	return repr;
}

array_representation* gc_context::alloc_array(type_info* content_type, size_t length) {
	size_t content_size = type_store->measure_array_content_size(content_type, length);
	void* content = alloc(content_size, false);

	array_representation* repr = (array_representation*) alloc(sizeof(array_representation), true);
	repr->array_length = length;
	repr->content = content;
	repr->core.type = type_store->get_type_array(content_type);
	repr->core.last_mark = last_mark_id;

	return repr;
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
	//Note that stack_pos is the start because the stack grows downwards.
	mark_conservative_region(stack_pos, uintptr_t(stack_start), objects_to_mark);

	//Mark static fields
	for (class_type* cls : type_store->class_types) {
		if (cls->static_size == 0) {
			continue;
		}

		for (field& field : cls->fields) {
			if (!field.flags.is_static) {
				continue;
			}
			if (field.type->type_category == TYPE_ARRAY ||
					field.type->type_category == TYPE_CLASS_OBJECT) {

				char* ptr = (char*) cls->static_field_data + field.field_offset;

				core_representation** reprptr = (core_representation**) ptr;
				core_representation* repr = *reprptr;
				repr->last_mark = last_mark_id;
			}
		}
	}

	while (!objects_to_mark.empty()) {
		mark(objects_to_mark.front(), objects_to_mark);
		objects_to_mark.pop();
	}
}

void gc_context::mark_conservative_region(uint32_t start, uint32_t end,
		std::queue<core_representation*>& pending_list) {

	for (uintptr_t pos = start; pos < end; pos += sizeof(void*)) {
		void* value_at = *((void**) pos);

		//cout << "Found " << value_at << ": ";

		if (is_heap_object(value_at)) {
			//cout << "Heap object" << endl;
			pending_list.push((core_representation*) value_at);
		}
	}

}

void gc_context::mark(core_representation* object, queue<core_representation*>& pending_list) {
	if (object->last_mark == last_mark_id) {
		return;
	}

	object->last_mark = last_mark_id;

	if (object->type->type_category == TYPE_CLASS_OBJECT) {
		const class_type* cls = ((class_type_info*) object->type)->cls;

		mark_fields(cls, object, pending_list);
	}
	else if (object->type->type_category == TYPE_ARRAY) {
		type_info* content_type = ((array_type_info*) object->type)->content_type;

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
	if (field.type->type_category == TYPE_ARRAY || field.type->type_category == TYPE_CLASS_OBJECT) {
		core_representation* location =
				*((core_representation**) ((char*) object + field.field_offset));

		if (location) {
			//Not null

			pending_list.push(location);
		}
	}
}

void gc_context::mark_array(const type_info* content_type, core_representation* object,
		queue<core_representation*>& pending_list) {

	array_representation* array = (array_representation*) object;
	void* content = array->content;

	switch (content_type->type_category) {
	case TYPE_CLASS_OBJECT:
		for (size_t i = 0; i < array->array_length; ++i) {
			pending_list.push(((core_representation**) content)[i]);
		}
		break;
	case TYPE_ARRAY:
		for (size_t i = 0; i < array->array_length; ++i) {
			pending_list.push((core_representation*) ((array_representation**) content)[i]);
		}
		break;
	default:
		//For the remaining types, we don't even bother checking for them.
		break;
	}
}

void gc_context::sweep() {
	//cout << "sweep " << (int) last_mark_id << endl;

	for (gc_heap& heap : heaps) {
		for (size_t i = heap.heap_starts.find_next_unset(0, heap.heap_starts.size());
				i < heap.heap_starts.size(); ++i) {
			if (!heap.heap_starts.get(i)) {
				continue;
			}

			core_representation* repr = (core_representation*) (heap.heap_aligned + i * HEAP_UNIT_SIZE);

			//cout << "Free " << repr << endl;

			if (repr->last_mark != last_mark_id) {
				//Free this object
				heap.heap_starts.unset(i);

				size_t object_size;

				if (repr->type->type_category == TYPE_ARRAY) {
					object_size = sizeof(array_representation);
					array_representation* arepr = (array_representation*) repr;
					array_type_info* type_as_array = (array_type_info*) repr->type;
					size_t content_size = type_store->measure_array_content_size(
							type_as_array->content_type, arepr->array_length);

					gc_heap* owner_heap = find_owner_heap(arepr->content, false);

					owner_heap->free_non_gc_object(arepr->content, content_size);
				}
				else if (repr->type->type_category == TYPE_CLASS_OBJECT) {
					class_type* cls = ((class_type_info*) repr->type)->cls;

					object_size = cls->computed_size;
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

void gc_context::prepare_static_fields() {
	for (class_type* cls : type_store->class_types) {
		if (cls->static_size > 0) {
			cls->static_field_data = alloc(cls->static_size, false);
			memset(cls->static_field_data, 0, cls->static_size);
		}
	}
}
