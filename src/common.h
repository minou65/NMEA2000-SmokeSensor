// common.h

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

extern uint8_t gN2KSource;
extern uint8_t gN2KInstance;
extern uint8_t gN2KSID;
extern bool gSaveParams;

extern char Version[];

extern bool gParamsChanged;

#endif

