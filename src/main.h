/*
 * main.h
 *
 */
#ifndef MAIN_H_
#define MAIN_H_

#include "SimpleJsonParser.h"

extern json_parser_t json_parser;

#ifdef DEBUG_PRINT
	#define SERIAL_BUFF_SIZE	64
	inline int8_t SerialProcessCommand(char const*);
	inline void onSerialReceive(char *);
#endif

#endif /* MAIN_H_ */
