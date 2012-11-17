#ifdef PLATFORM_X64
#include "x86_64.h"
#else
#ifdef PLATFORM_X86
#include "x86.h"
#else
#error "No platform defined"
#endif
#endif
