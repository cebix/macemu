#if defined(__x86_64__)
	#include "config-macosx-x86_64.h"
#elif defined(__i386__)
	#include "config-macosx-x86_32.h"
#elif defined(__ppc__)
	#include "config-macosx-ppc_32.h"
#else
	#error Unknown platform
#endif