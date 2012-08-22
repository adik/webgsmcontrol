/*
 * main.h
 *
 *  Created on: 29 июля 2012
 *      Author: adik
 */

#ifndef MAIN_H_
#define MAIN_H_


#define PROGMEM_BUFF_SIZE	64


#ifdef DEBUG_ATCOMMANDS
	#define SERIAL_BUFF_SIZE	64

	inline int8_t SerialProcessCommand(char const*);
	inline void onSerialReceive(char *);
#endif




#endif /* MAIN_H_ */
