// common.h

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

extern bool debugMode;

#define DEBUG_PRINT(x) if (debugMode) Serial.print(x) 
#define DEBUG_PRINTLN(x) if (debugMode) Serial.println(x)
#define DEBUG_PRINTF(...) if (debugMode) Serial.printf(__VA_ARGS__)

extern uint8_t gN2KSource;
extern uint8_t gN2KInstance;
extern uint8_t gN2KSID;
extern bool gSaveParams;

extern char Version[];

extern bool gParamsChanged;

#endif

