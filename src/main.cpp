/*
 * Target: AVR328P
 * Crystal: 16.000Mhz

 *  DOCS:
 *  http://imall.iteadstudio.com/im120417009.html
 *
 *  minicom -c on -D /dev/ttyACM0

 */
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <GSM_Shield_GPRS.h>
#include <GSM_Shield.h>
#include <main.h>
#include "SimpleJsonParser.h"


#ifdef DEBUG_PRINT
char _serial_buffer[SERIAL_BUFF_SIZE+1];
#endif


//for enable disable debug rem or not the string #define DEBUG_PRINT
// definition of instance of GSM class
GPRS gsm;

const prog_char HTTP_connectWS[] PROGMEM = {
			"GET %p HTTP/1.1\r\n"
			"Upgrade: WebSocket\r\n"
			"Connection: Upgrade\r\n"
			"Host: %p\r\n"
			"Origin: ArduinoWebSocketClient\r\n"
			"\r\n" };

// declare parser variable
json_parser_t json_parser = {};

// declare s
static enum {
	GET_HEADER = 0,
	WAIT_HEADER_END = 1,
	HEADER_RECEIVED = 3
} recv_state;

/**********************************************************
 Receive GPRS data handler
**********************************************************/
void recv_data(byte chr) {

	// skip header
	switch (recv_state) {
	case GET_HEADER:
		if (chr == '\r')
			goto process_header;
		if (chr == '\n')
			recv_state = WAIT_HEADER_END;
		goto process_header;
		break;

	case WAIT_HEADER_END:
		if (chr == '\r')
			goto process_header;
		if (chr == '\n')
			recv_state = HEADER_RECEIVED;
		else
			recv_state = GET_HEADER;

		goto process_header;
		break;

	case HEADER_RECEIVED:
		break;
	}

process_data:

	Serial.write(chr);

	// parse received json stream
	if ( json_parse(&json_parser, chr) ) {
		// get event
		char *event, *data;

		event = json_get_tag_value(&json_parser, "event");

		if (event) {
			Serial.print("Event:");
			Serial.println(event);

			if ((data = json_get_tag_value(&json_parser, "data"))) {
				Serial.print("Data:");
				Serial.println(data);
			}
			free(data);
		}

		// clean garbage
		free(event);

		json_clean_tokens(&json_parser);
	}

process_header:
	return;
}


/**********************************************************

**********************************************************/
void ws_event( const prog_char *fmt, ... ) {
	va_list  args;
	va_start(args, fmt);
	//gsm.TCP_Send(HTTP_WSEvent, 0, fmt, args, 255 );
	va_end(args);
}

/**********************************************************
	- Connect to WebSocket
	- Send Handshake
	- Subscribe to a channel
**********************************************************/
void connect_ws() {
	// connect to WS
	gsm.TCP_Connect(F("ws.pusherapp.com"));

	if (CONNECT_OK == gsm.getState()) {

		recv_state = GET_HEADER;

		// send handshake
		gsm.TCP_Send(HTTP_connectWS,
				PSTR("/app/8185ce71534c69c42b72?client=js&version=1.9.0"),
				PSTR("ws.pusherapp.com"));

		// ensure
		recv_state = HEADER_RECEIVED;

		// subscribe a channel
		//gsm.TCP_Send(HTTP_WSSubscribe, 0, 255);
        gsm.TCP_Send(PSTR("%d{\"event\":\"%p\",\"data\":{%p}}%d"),
        		0, PSTR("pusher:subscribe"), PSTR("\"channel\":\"private-cmd\"") ,255) ;

	}
}



/**********************************************************

**********************************************************/
inline void setup() {
#ifdef DEBUG_PRINT
	Serial.begin(38400);
#endif
	//gsm.InitSerLine(9600); //initialize serial 1
	gsm.TurnOn(9600); //module power on
	//gsm.TurnOn(19200); //module power on
	//gsm.InitSerLine(9600); //initialize serial 1
	gsm.InitParam(PARAM_SET_0); //configure the module
	//
	json_init(&json_parser);
	gsm.setRecvHandler(recv_data);
}

/**********************************************************

**********************************************************/
inline void loop() {

	static unsigned long last_fetch_time;


	if ((unsigned long)(millis() - last_fetch_time) >= 10000) {
		// wash buffer
		mySerial.flush();
		gsm.fetchState();
		last_fetch_time = millis();
	}

	switch(gsm.getState()) {
	case TCP_CONNECTING:
		break;

	case CONNECT_OK:
		//if ( CLS_DATA == gsm.GetCommLineStatus()) {
		mySerial.flush();
		gsm.ReceiveGprsData();
		//};
		delay(1000);
		break;

	case TCP_CLOSED:
		connect_ws();
		// need reconnect timeout
		break;

	default:
		gsm.GPRS_detach();
		delay(3000);
		gsm.GPRS_attach();
		delay(3000);
		connect_ws();
		break;
	}

	#ifdef DEBUG_PRINT
	// process serial commands
	if (Serial.available())
		onSerialReceive(_serial_buffer);
	#endif
}

////////// ---------------------------------- ////////
int main(void) {
	init();
	setup();
	for (;;) {
		loop();
		//if (serialEventRun) serialEventRun();
	}
	return 0;
}

/****************************************************************************
 *
 * export TTYDEV=/dev/ttyACM0
 * stty raw speed 9600 -crtscts cs8 -parenb -parodd cstopb -hupcl cread clocal ignbrk ignpar -F $TTYDEV
 * xxd -c1 $TTYDEV
 *
 * echo -en "AT\r" > $TTYDEV
 *
 */
#ifdef DEBUG_PRINT

byte incomingByte;
byte recv_byte = 0;

inline void onSerialReceive(char *buffer) {
	incomingByte = Serial.read();
	buffer[recv_byte] = incomingByte;
	recv_byte++;

	if (incomingByte == '\0' || recv_byte > SERIAL_BUFF_SIZE ) {
		buffer[recv_byte] = '\0';
		SerialProcessCommand(buffer);

		*buffer = '\0';
		recv_byte = 0;
	}
}
inline int8_t SerialProcessCommand(char const *buffer) {
	//Serial.println(buffer);
	gsm.SendATCmdWaitResp(buffer, 1000, 50, "", 1);
	return 1;
}
#endif
