/*
 * Target: AVR328P
 * Crystal: 16.000Mhz
 */
#include <Arduino.h>

/*
 *  DOCS:
 *  http://imall.iteadstudio.com/im120417009.html
 *
 *  minicom -c on -D /dev/ttyACM0
 */

#include <main.h>

#include <SoftwareSerial.h>
#include <GSM_Shield_GPRS.h>
#include <GSM_Shield.h>


#ifdef DEBUG_ATCOMMANDS
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

const prog_char HTTP_WSSubscribe[] PROGMEM 	= {
			"%d{\"event\": \"pusher:subscribe\""
			", \"data\": {\"channel\": \"channel_1\" } }%d" };

const prog_char HTTP_WSEvent[] PROGMEM 	= {
			"%d{\"event\": \"%s\", \"data\": {%s} }%d" };


/**********************************************************

**********************************************************/
void recv_data(byte chr) {

	Serial.write(chr);
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
		// send handshake
		gsm.TCP_Send(HTTP_connectWS,
				PSTR("/app/8185ce71534c69c42b72?client=js&version=1.9.0"),
				PSTR("ws.pusherapp.com"));

		// subscribe a channel
		gsm.TCP_Send(HTTP_WSSubscribe, 0, 255);
	}
}

/**********************************************************

**********************************************************/
inline void setup() {
	// Setup GPRS connection
	Serial.begin(9600);
	//gsm.InitSerLine(9600); //initialize serial 1
	gsm.TurnOn(9600); //module power on
	//gsm.InitSerLine(9600); //initialize serial 1
	gsm.InitParam(PARAM_SET_0); //configure the module
	//
	gsm.setRecvHandler(recv_data);
}

/**********************************************************

**********************************************************/
inline void loop() {

	static unsigned long last_fetch_time;

	// wash buffer
	mySerial.flush();

	if ((unsigned long)(millis() - last_fetch_time) >= 10000) {
		gsm.fetchState();
		last_fetch_time = millis();
	}

	switch(gsm.getState()) {
	case TCP_CONNECTING:
		break;

	case CONNECT_OK:
		if ( CLS_DATA == gsm.GetCommLineStatus()) {
			gsm.ReceiveGprsData();
		};
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

	#ifdef DEBUG_ATCOMMANDS
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
#ifdef DEBUG_ATCOMMANDS
//
//
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
