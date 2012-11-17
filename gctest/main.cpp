#include "core.h"
#include <iostream>

using std::cout;
using std::cerr;
using std::endl;

gc_type_store* type_store;
gc_context* ctx;
class_type* cls_coreLink;

void test_linked_list() {
	type_info* cls_type = type_store->get_class_type(cls_coreLink);
	const field& fprev = cls_coreLink->fields[0];
	const field& fnext = cls_coreLink->fields[1];
	const field& fval = cls_coreLink->fields[2];

	size_t oprev = fprev.field_offset;
	size_t onext = fnext.field_offset;
	size_t oval = fval.field_offset;

	void* e1 = 0;

	for (int i = 0; i < 1000; ++i) {
		void* first = 0;
		void* prev = 0;
		for (int j = 0; j < 15000; ++j) {
			void* node = ctx->alloc_class(cls_type);
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
		array_representation* array = ctx->alloc_array(type_store->get_type_int32(), 50000);
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

void test_statics(bool check_old) {
	class_type* cls = type_store->class_by_name("core.Link");
	type_info* cls_type = type_store->get_class_type(cls);
	const field& fval = cls->fields[2];
	const field& fsomething = cls->fields[3];
	const field& fNotableLink = cls->fields[4];

	size_t oval = fval.field_offset;
	size_t osomething = fsomething.field_offset;
	size_t oNotableLink = fNotableLink.field_offset;

	void* static_data = cls->static_field_data;

	if (check_old) {
		void* node = *((void**) ((char*) static_data + oNotableLink));
		uint32_t* val = (uint32_t*)((char*) node + oval);
		cout << "Old value was " << *val << endl;
		*val = 123;
	}

	void* node = ctx->alloc_class(cls_type);

	*((uint32_t*) ((char*) static_data + osomething)) = 0x12345678;
	*((void**) ((char*) static_data + oNotableLink)) = node;

	uint32_t* val = (uint32_t*)((char*) node + oval);
	*val = 123;
}

void run_main(void* args) {

	class_type core_Link;
	core_Link.full_name = "core.Link";
	core_Link.base_type = 0;
	core_Link.owned_type = nullptr;
	cls_coreLink = &core_Link;
	type_store->push_class_type(&core_Link);

	field core_Link_prev;
	core_Link_prev.type = type_store->get_class_type(&core_Link);
	core_Link_prev.flags.is_static = 0;
	core_Link.fields.push_back(core_Link_prev);

	field core_Link_next;
	core_Link_next.type = type_store->get_class_type(&core_Link);
	core_Link_next.flags.is_static = 0;
	core_Link.fields.push_back(core_Link_next);

	field core_Link_val;
	core_Link_val.type = type_store->get_type_int32();
	core_Link_val.flags.is_static = 0;
	core_Link.fields.push_back(core_Link_val);

	field core_Link_something;
	core_Link_something.type = type_store->get_type_int32();
	core_Link_something.flags.is_static = 1;
	core_Link.fields.push_back(core_Link_something);

	field core_Link_NotableLink;
	core_Link_NotableLink.type = type_store->get_class_type(&core_Link);
	core_Link_NotableLink.flags.is_static = 1;
	core_Link.fields.push_back(core_Link_NotableLink);

	type_store->compute_sizes();
	type_store->compute_static_sizes();
	type_store->log_headers();
	ctx->prepare_static_fields();

	cout << "Test statics" << endl;
	test_statics(false);
	cout << "Now array" << endl;
	test_array();
	cout << "Now list" << endl;
	test_linked_list();
	cout << "Array again" << endl;
	test_array();
	cout << "More statics" << endl;
	test_statics(true);

	cout << ctx->count_heaps() << endl;

	cout.flush();
	cerr.flush();
}

int main() {
	type_store = new gc_type_store();
	ctx = new gc_context(std::unique_ptr<gc_type_store>(type_store));

	declare_thread(ctx, NULL, run_main);

	return 0;
}
