#include "core.h"
#include <iostream>

using std::cout;
using std::cerr;
using std::endl;

gc_context* ctx;

void test_linked_list() {
	const class_type* cls = ctx->class_by_name("core.Link", 9);
	const field& fprev = cls->fields[0];
	const field& fnext = cls->fields[1];
	const field& fval = cls->fields[2];

	size_t oprev = fprev.field_offset;
	size_t onext = fnext.field_offset;
	size_t oval = fval.field_offset;

	void* e1 = 0;

	for (int i = 0; i < 1000; ++i) {
		void* first = 0;
		void* prev = 0;
		for (int j = 0; j < 15000; ++j) {
			void* node = ctx->alloc_class("Lcore.Link;");
			if (!first) {
				first = node;
				if (!e1) {
					e1 = first;
				}
			}
			else {
				*((void**) ((char*) node + oprev)) = prev;
				if (prev) {
					*((void**) ((char*) prev + onext)) = node;
				}
			}
			*((void**) ((char*) node + onext)) = nullptr;
			*((uint32_t*) ((char*) node + oval)) = j + 1;
			prev = node;
		}

		//cout << "Test:" << endl;

		uint32_t pval = 0;
		for (void* celem = first; celem; celem = *((void**) ((char*) celem + onext))) {
			if (*((uint32_t*) ((char*) celem + oval)) != pval + 1) {
				cerr << "WRONG RESULTS. Got " << *((uint32_t*) ((char*) celem + oval)) << endl;
			}
			++pval;
			//cout << ":" << *((uint32_t*) ((char*) celem + oval)) << endl;
		}
	}
}

void test_array() {
	array_representation* first = nullptr;
	for (int i = 0; i < 10000; ++i) {
		array_representation* array = ctx->alloc_array("[I", 50000);
		if (!first) {
			first = array;
		}

		for (size_t j = 0; j < array->array_length; ++j) {
			((uint32_t*) array->content)[j] = j;
		}

		for (size_t j = 0; j < array->array_length; ++j) {
			uint32_t val = ((uint32_t*) array->content)[j];
			if (val != j) {
				cerr << "WRONG RESULTS. Got " << val << endl;
			}
		}
	}

	for (size_t j = 0; j < first->array_length; ++j) {
		uint32_t val = ((uint32_t*) first->content)[j];
		if (val != j) {
			cerr << "WRONG RESULTS. Got " << val << endl;
		}
	}
}

int main() {
	ctx = new gc_context(get_stack_pointer());

	class_type core_Link;
	core_Link.full_name = "core.Link";
	core_Link.base_type = 0;

	field core_Link_prev;
	core_Link_prev.type = "Lcore.Link;";
	core_Link_prev.flags.is_static = 0;
	core_Link.fields.push_back(core_Link_prev);

	field core_Link_next;
	core_Link_next.type = "Lcore.Link;";
	core_Link_next.flags.is_static = 0;
	core_Link.fields.push_back(core_Link_next);

	field core_Link_val;
	core_Link_val.type = "I";
	core_Link_val.flags.is_static = 0;
	core_Link.fields.push_back(core_Link_val);

	ctx->push_class_type(&core_Link);

	ctx->compute_sizes();
	ctx->log_headers();

	test_array();
	cout << "Now list" << endl;
	test_linked_list();
	cout << "Array again" << endl;
	test_array();

	cout << ctx->count_heaps() << endl;

	cout.flush();
	cerr.flush();
}
