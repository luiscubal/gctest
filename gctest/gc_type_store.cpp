#include "core.h"
#include <iostream>
#include <cstdlib>
#include "utils.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::abort;
using std::malloc;

gc_type_store::gc_type_store() {
	for (size_t i = 0; i <= LAST_PRIMITIVE_TYPE; ++i) {
		primitive_types[i].type_category = type_category_t(i);
		primitive_types[i].array_type = nullptr;
	}
}

type_info* gc_type_store::get_type_void() {
	return &(primitive_types[TYPE_VOID]);
}

type_info* gc_type_store::get_type_int32() {
	return &(primitive_types[TYPE_INT32]);
}

type_info* gc_type_store::get_type_array(type_info* base_type) {
	if (base_type->array_type) {
		return base_type->array_type;
	}

	Lock mlock = lock();

	array_type_info* type = (array_type_info*) malloc(sizeof(array_type_info));

	type->base_type.type_category = TYPE_ARRAY;
	type->base_type.array_type = nullptr;
	type->content_type = base_type;

	base_type->array_type = (type_info*) type;

	return (type_info*) type;
}

type_info* gc_type_store::get_class_type(class_type* cls) {
	if (cls->owned_type) {
		return cls->owned_type;
	}

	Lock mlock = lock();

	class_type_info* type = (class_type_info*) malloc(sizeof(class_type_info));

	type->base_type.type_category = TYPE_CLASS_OBJECT;
	type->base_type.array_type = nullptr;
	type->cls = cls;

	cls->owned_type = (type_info*) type;

	return (type_info*) type;
}

size_t gc_type_store::full_compute_class_size(class_type* cls) {
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

		switch (field.type->type_category) {
		case TYPE_ARRAY:
			size = align(size, sizeof(void*));
			field.field_offset = size;
			size += sizeof(void*);
			break;
		case TYPE_CLASS_OBJECT:
			size = align(size, sizeof(void*));
			field.field_offset = size;
			size += sizeof(void*);
			break;
		case TYPE_INT32:
			size = align(size, sizeof(uint32_t));
			field.field_offset = size;
			size += sizeof(uint32_t);
			break;
		default:
			cerr << "Unrecognized field type " << field.type << endl;
			abort();
			break;
		}
	}

	for (method& method : cls->methods) {
		if (!method.flags.is_virtual) {
			continue;
		}

		size = align(size, sizeof(func_ptr));
		method.virtual_offset = size;
		size += sizeof(func_ptr);
	}

	return size;
}

void gc_type_store::compute_sizes() {
	for (class_type* cls : class_types) {
		cls->computed_size = full_compute_class_size(cls);
	}
}

size_t gc_type_store::full_compute_class_static_size(class_type* cls) {
	size_t size = 0;

	for (field& field : cls->fields) {
		if (!field.flags.is_static) {
			continue;
		}

		switch (field.type->type_category) {
		case TYPE_ARRAY:
			size = align(size, sizeof(void*));
			field.field_offset = size;
			size += sizeof(void*);
			break;
		case TYPE_CLASS_OBJECT:
			size = align(size, sizeof(void*));
			field.field_offset = size;
			size += sizeof(void*);
			break;
		case TYPE_INT32:
			size = align(size, sizeof(uint32_t));
			field.field_offset = size;
			size += sizeof(uint32_t);
			break;
		default:
			cerr << "Unrecognized field type " << field.type << endl;
			abort();
			break;
		}
	}

	return size;
}

void gc_type_store::compute_static_sizes() {
	for (class_type* cls : class_types) {
		cls->static_size = full_compute_class_static_size(cls);
	}
}

class_type* gc_type_store::class_by_name(string class_name) {
	for (class_type* cls : class_types) {
		if (cls->full_name == class_name) {
			return cls;
		}
	}

	cerr << "Class " << class_name << " not found" << endl;
	abort();
}

const class_type* gc_type_store::class_by_name(string class_name) const {
	for (const class_type* cls : class_types) {
		if (cls->full_name == class_name) {
			return cls;
		}
	}

	cerr << "Class " << class_name << " not found" << endl;
	abort();
}

size_t gc_type_store::measure_class_size(type_info* type) const {
	return ((class_type_info*) type)->cls->computed_size;
}

size_t gc_type_store::measure_direct_heap_size(type_info* type) const {
	switch (type->type_category) {
	case TYPE_CLASS_OBJECT: return sizeof(core_representation);
	case TYPE_ARRAY: return sizeof(array_representation);
	case TYPE_INT32: return sizeof(uint32_t);
	case TYPE_VOID: return 0;
	}

	cerr << "measure_direct_heap_size Unrecognized type " << type->type_category << endl;
	abort();
}

size_t gc_type_store::measure_array_content_size(type_info* content_type, size_t len) const {
	return len * measure_direct_heap_size(content_type);
}

void gc_type_store::log_headers() {
	for (class_type* cls : class_types) {
		cout << "full_name=" << cls->full_name << endl;
		if (cls->base_type) {
			cout << "base_type=" << cls->base_type->full_name << endl;
		}
		else {
			cout << "no base_type" << endl;
		}
		cout << "computed_size=" << cls->computed_size << endl;
		cout << "static_size=" << cls->static_size << endl;
		cout << endl;
	}
}

Lock gc_type_store::lock() {
	return mutex.lock();
}
