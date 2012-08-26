/*
 * GSM_Shield_GPRS.h
 *
 *  Created on: 29 июля 2012
 *      Author: adik
 */

#ifndef __GSM_Shield_GPRS
#define __GSM_Shield_GPRS

#include <GSM_Shield.h>


enum gprs_state_enum {
	IP_BUSY = -1,
	IP_INITIAL = 0,
	IP_START,
	IP_CONFIG,
	IP_GPRSACT,
	IP_STATUS,
	TCP_CONNECTING,  //UDP CONNECTING /SERVER LISTENING
	CONNECT_OK,
	TCP_CLOSING,   	//UDP CLOSING
	TCP_CLOSED, 	//UDP CLOSED

	PDP_DEACT
};


class GPRS : public GSM
{
private:
	typedef 		void (*EventHandler)(const byte) ;
	byte 			gprs_state;
	EventHandler 	_onReceiveHandler;

public:
	GPRS(void);

	void setRecvHandler( EventHandler handler ) {  _onReceiveHandler = handler; };

	void ReceiveGprsData();

	void GPRS_Context2Nvram();

	void TCP_Connect(const char *);
	void TCP_Connect(__FlashStringHelper *);

	void TCP_Send(const prog_char *data, ...);
	void TCP_Close();

	void GPRS_attach();
	void GPRS_detach();

	void fetchState();
	void setState(byte state) { gprs_state = state; }
	byte getState() { return gprs_state; }
};



#endif /* __GSM_Shield_GPRS */
