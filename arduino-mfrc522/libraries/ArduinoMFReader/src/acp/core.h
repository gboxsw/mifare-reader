#ifndef ACP_HEADER_H_INCLUDED
#define ACP_HEADER_H_INCLUDED

#include <Arduino.h>
#include <math.h>

// Indicates whether the debug mode is enabled
#define ACP_DEBUG 0

// Default event handler
typedef void (*ACPEventHandler)();

namespace acp {
	extern void enableLooper(int looperId);
	extern void disableLooper(int looperId);
}

#endif // ACP_HEADER_H_INCLUDED
