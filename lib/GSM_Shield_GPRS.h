/*
 * GSM_Shield_GPRS.h
 *
 *  Created on: 29 июля 2012
 *      Author: adik
 */
#ifndef __GSM_Shield_GPRS
#define __GSM_Shield_GPRS
#include <GSM_Shield.h>

#define GPRS_DATA_RECEIVE_TIMEOUT  60000
#define GPRS_DATA_BUFFER_SIZE 256

struct gprs_ring_buffer {
  byte buffer[GPRS_DATA_BUFFER_SIZE];
  volatile byte *head;
  volatile byte *tail;
};

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
	typedef 		void (*EventHandlerRx)(const byte) ;
	typedef 		void (*EventHandlerTx)() ;
	byte 			gprs_state;
	EventHandlerRx 	_onReceiveHandler;
	EventHandlerTx 	_onTransferHandler;

public:
	GPRS(void);

	void setRxHandler( EventHandlerRx handler ) {  _onReceiveHandler = handler; };
	void setTxHandler( EventHandlerTx handler ) {  _onTransferHandler = handler; };

	void handleCommunication();

	byte RX_packet();
	void Handle_Data();
	void TX_Data();

	void GPRS_Context2Nvram();

	void TCP_Connect(const char *);
	void TCP_Connect(const __FlashStringHelper *);

	void TCP_Send(const char PROGMEM *data, ...);
	void TCP_Close();

	void GPRS_attach();
	void GPRS_detach();

	void fetchState();
	void setState(byte state) { gprs_state = state; }
	byte getState() { return gprs_state; }
};



#endif /* __GSM_Shield_GPRS */
