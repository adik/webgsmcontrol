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
#include "sha256.h"

#ifdef DEBUG_PRINT
char _serial_buffer[SERIAL_BUFF_SIZE+1];
#endif


//for enable disable debug rem or not the string #define DEBUG_PRINT
// definition of instance of GSM class
GPRS gsm;

// encryption
Sha256Class Sha256;

// declare parser variable
json_parser_t json_parser = {};


const prog_char HTTP_connectWS[] PROGMEM = {
			"GET %p HTTP/1.1\r\n"
			"Upgrade: WebSocket\r\n"
			"Connection: Upgrade\r\n"
			"Host: %p\r\n"
			"Origin: ArduinoWebSocketClient\r\n"
			"\r\n" };




const uint8_t Pusher_Key[] = "8185ce71534c69c42b72";
const uint8_t Pusher_Secret[]= "c30a8df113dd52dc64e4";

char auth_token[65];

/**********************************************************

**********************************************************/



inline void generate_auth(char *str) {
	uint8_t *hash;
	char 	*ap;

	Sha256.initHmac(Pusher_Secret, 20);
	Sha256.print(str);

	hash = Sha256.resultHmac();

	ap = &auth_token[0];

	for (int i=0; i<HASH_LENGTH; i++) {
			*ap++ = "0123456789abcdef"[hash[i]>>4];
			*ap++ = "0123456789abcdef"[hash[i]&0xf];
	}
	*ap='\0';
}

/**********************************************************
 Receive GPRS data handler
**********************************************************/

// declare state
volatile enum recv_state_enum {
	GET_HEADER = 0,
	WAIT_HEADER_END = 1,
	HEADER_RECEIVED = 3
} recv_state;


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

#ifdef DEBUG_PRINT
	Serial.write(chr);
#endif

	// parse received json stream
	if ( json_parse(&json_parser, chr) ) {
		// get event
		char *event, *data;

		event = json_get_tag_value(&json_parser, "event");

		if (event) {
			if (strstr(event, "pusher:connection_established")) {
				if ((data = json_get_tag_value(&json_parser, "data"))) {
					char tmp[24];
					memset(tmp,'\0', 24);
					char channel[]=":private-cmd";

					//"{\"socket_id\":\"12035.86349\"}";
					strncpy(tmp, data+14, (strlen(data)-2-14));
					strcat(tmp, channel);

					generate_auth(tmp);
					gsm.SendATCmdWaitResp(1000, 100, ">", 1, PSTR("AT+CIPSEND"));
					//gsm.vprintf_P(mySerial, PSTR("AT+CIPSEND\r"));
					gsm.vprintf_P(mySerial, PSTR("%d{\"event\":\"%p\",\"data\":{\"channel\":\"private-cmd\",\"auth\":\"%s:%s\"}}%d"),
			        		0, PSTR("pusher:subscribe"), Pusher_Key, auth_token, 255) ;
					mySerial.write(0x1a);

					//Serial.println(data);
					//Serial.println(channel);
					//Serial.println(auth_token);
					//Serial.println((char*)Pusher_Key);
					//Serial.println((char*)Pusher_Key);

				}
				free(data);
			}
			else if (strstr(event, "pusher") == 0) {

				gsm.SendATCmdWaitResp(1000, 100, ">", 1, PSTR("AT+CIPSEND"));
				//gsm.vprintf_P(mySerial, PSTR("AT+CIPSEND\r"));
				gsm.vprintf_P(mySerial,
					PSTR("%d{\"event\":\"client-responce\",\"data\":\"pong\",\"channel\":\"private-cmd\"}%d"),
					0,
					255);
				mySerial.write(0x1a);
			}
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
		/*
        gsm.TCP_Send(PSTR("%d{\"event\":\"%p\",\"data\":{\"channel\":\"private-cmd\",\"auth\":\"%s:%s\"}}%d"),
        		0, PSTR("pusher:subscribe"), Pusher_Key, auth_token, 255) ;
        */

	}
}



/**********************************************************

**********************************************************/


inline void setup() {
#ifdef DEBUG_PRINT
	//Serial.begin(38400);
	Serial.begin(9600);
#endif
	//gsm.InitSerLine(9600); //initialize serial 1
	gsm.TurnOn(9600); //module power on
	//gsm.TurnOn(19200); //module power on
	//gsm.InitSerLine(9600); //initialize serial 1
	gsm.InitParam(PARAM_SET_0); //configure the module
	//
	//
	json_init(&json_parser);
	gsm.setRecvHandler(recv_data);


}


/**********************************************************

**********************************************************/
inline void loop() {

	static unsigned long last_fetch_time;

	if ((unsigned long)(millis() - last_fetch_time) >= 10000) {
		gsm.fetchState();
		last_fetch_time = millis();
	}

	switch(gsm.getState()) {
	case TCP_CONNECTING:
		break;

	case CONNECT_OK:
		//if ( CLS_DATA == gsm.GetCommLineStatus()) {
		gsm.handleCommunication();
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
	if (Serial.available()){
		onSerialReceive(_serial_buffer);
	}
#endif

	//delay(6000);
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
volatile byte recv_byte = 0;

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
