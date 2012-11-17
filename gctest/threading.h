#ifndef THREADING_H_
#define THREADING_H_

#ifdef _WIN32
#include "threading.win.h"
#else
#error "Unrecognized platform"
#endif

class gc_context;

typedef void (*thread_func)(void* data);

thread_identifier start_thread(gc_context* context, void* data, thread_func func);
void declare_thread(gc_context* context, void* data, thread_func func);
thread_identifier get_current_thread();
void dereference_thread(thread_identifier thread);
void join_thread(thread_identifier thread);

#endif
