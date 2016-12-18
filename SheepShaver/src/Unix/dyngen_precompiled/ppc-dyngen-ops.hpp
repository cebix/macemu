#if defined(__x86_64__)
	#include "ppc-dyngen-ops-x86_64.hpp"
#elif defined(__i386__)
	#include "ppc-dyngen-ops-x86_32.hpp"
#else
	#error Unknown platform
#endif
