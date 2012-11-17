#include "core.h"
#include "threading.h"
#include "platform.h"

using std::vector;

Mutex::Mutex() {
	mutex = CreateMutexA(NULL, FALSE, NULL);
}

Mutex::Mutex(Mutex&& other) : mutex(other.mutex) {
	other.mutex = NULL;
}

Mutex::~Mutex() {
	if (mutex) {
		CloseHandle(mutex);
	}
}

Lock Mutex::lock() {
	return Lock(*this);
}

Lock::Lock(Mutex& mutex) : mutex(mutex.mutex) {
	WaitForSingleObject(mutex.mutex, INFINITE);
}

Lock::Lock(Lock&& other) : mutex(other.mutex) {
	other.mutex = NULL;
}

Lock::~Lock() {
	if (mutex) {
		ReleaseMutex(mutex);
	}
}

thread_identifier get_current_thread() {
	return GetCurrentThread();
}

struct thread_start {
	gc_context* context;
	thread_func func;
	void* data;
};

WINAPI DWORD thread_startup(LPVOID info) {
	thread_start* start_info = (thread_start*) info;
	gc_context* context = start_info->context;
	thread_func func = start_info->func;
	void* data = start_info->data;

	{
		Lock lock = context->lock();
		free(start_info);
	}

	declare_thread(context, data, func);

	return 0;
}

static void setup_thread(gc_context* context, thread_identifier current_thread) {
	void* stack_start = get_stack_pointer();

	Lock lock = context->lock();

	current_thread = get_current_thread();

	context->declare_thread(current_thread, stack_start);
}

static void finalize_thread(gc_context* context, thread_identifier current_thread) {
	Lock lock = context->lock();
	context->forget_thread(current_thread);
}

void declare_thread(gc_context* context, void* data, thread_func func) {
	thread_identifier current_thread = get_current_thread();

	setup_thread(context, current_thread);

	func(data);

	finalize_thread(context, current_thread);
}

thread_identifier start_thread(gc_context* context, void* data, thread_func func) {
	Lock lock = context->lock();

	thread_start* start_info = (thread_start*) malloc(sizeof(thread_start));
	start_info->context = context;
	start_info->func = func;
	start_info->data = data;

	HANDLE thread = CreateThread(NULL, 0, thread_startup, start_info, 0, NULL);

	return thread;
}

vector<CONTEXT> stop_world(vector<thread_identifier>& threads) {
	thread_identifier current_thread = get_current_thread();

	vector<CONTEXT> contexts;
	CONTEXT context;

	for (thread_identifier thread : threads) {
		if (thread != current_thread) {
			GetThreadContext(thread, &context);
			contexts.push_back(context);
		}
	}

	return contexts;
}

void dereference_thread(thread_identifier thread) {
	CloseHandle(thread);
}

void join_thread(thread_identifier thread) {
	WaitForSingleObject(thread, INFINITE);
}
