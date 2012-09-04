/*
 * main.h
 *
 */
#ifndef Main_H_
#define Main_H_

#define JSON_MAX_DATA_BUFFER 128

#include "SimpleJsonParser.h"

extern json_parser_t json_parser;

#ifdef DEBUG_PRINT
	#define SERIAL_BUFF_SIZE	64
	inline int8_t SerialProcessCommand(char const*);
	inline void onSerialReceive(char *);
#endif

#endif /* Main_H_ */
