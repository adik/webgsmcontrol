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


// +5v       +data (14)
// clock(11)
// latch(12)

//  4 нога Q4
// 15 нога Q0

/*
	14 - gnd

	Pin2Key:

	1+10 = 10
	3+10 = 9
	5+10 = 8
	7+10 = 7
	9+10 = 6
	1+ 8 = 5
	3+ 8 = 4
	5+ 8 = 3
	7+ 8 = 2
	9+ 8 = 1

    KeyPad:
    1      -      2
    3      -      4
    5      -      6
    7      -      8
    9      -     10



        74x4051 | 74x595

				  mask 0x11110000
   9    Y4        0x100
   7    Y6        0x110
   5    Y5        0x101
   3    Y7        0x111
   1    Y3        0x011
   E    -         Q4

                  mask 0x00001111
   10   Y6        0x110
   8    Y7        0x111
   E    -         Q0


      0x10001110 = 9 + 8
*/




//for enable disable debug rem or not the string #define DEBUG_PRINT
// definition of instance of GSM class
GPRS gsm;

// declare parser variable
static json_parser_t json_parser = {};

//
const prog_char HTTP_connectWS[] PROGMEM = {
			"GET /app/%p?client=js&version=1.12&protocol=5 HTTP/1.1\r\n"
			"Upgrade: WebSocket\r\n"
			"Connection: Upgrade\r\n"
			"Host: %p\r\n"
			"Origin: ArduinoWebSocketClient\r\n"
			"\r\n" };


static const prog_char Pusher_Key[] PROGMEM   = {"8185ce71534c69c42b72"};
static const uint8_t   Pusher_Secret[]        = {"c30a8df113dd52dc64e4"};


/**********************************************************

**********************************************************/
// keyboard pins

#define Keyboard_Delay_KeyRelease 200

#define Keyboard_Pin_DATA     5
#define Keyboard_Pin_LATCH    6
#define Keyboard_Pin_CLK      7

static uint8_t keyboard_latch_state = 0;

static inline void Keyboard_SetState(uint8_t data) {
	keyboard_latch_state = data;
}


static inline uint8_t Keyboard_GetState() {
	return keyboard_latch_state;
}


// HEX to int
void Keyboard_SetState(char *data) {

	char c;
	uint8_t dec = 0;

	for (uint8_t i=0; i<2; ++i) {

		c = *(data+i);

		if (!c) {
			break;
		}
		else if ('0' <= c && c <= '9') {
			dec = dec * 16;
			dec = dec + (c - '0');
		}
		else if ('a' <= c && c <= 'f') {
			dec = dec * 16;
			dec = dec + (c - 'a') + 10;
		}
		else {
			dec = 0;
			break;
		}
	}

	Keyboard_SetState(dec);
}


void Keyboard_Latch_Stamp() {

	uint8_t i;

	digitalWrite(Keyboard_Pin_LATCH, LOW);
	digitalWrite(Keyboard_Pin_DATA, LOW);

	for (i = 0; i < 8; i++)  {
		digitalWrite(Keyboard_Pin_CLK, LOW);
		digitalWrite(Keyboard_Pin_DATA, !!(keyboard_latch_state & (1 << (7 - i))));
		digitalWrite(Keyboard_Pin_CLK, HIGH);
	}

	digitalWrite(Keyboard_Pin_LATCH, HIGH);
}



/**********************************************************

**********************************************************/
//
static char auth_token[65];

inline void WebSocket_Generate_AuthToken(char *str) {
	uint8_t *hash;
	char 	*ap;

	// encryption
	Sha256Class *Sha256 = new Sha256Class();

	Sha256->initHmac(Pusher_Secret, 20);
	Sha256->print(str);

	hash = Sha256->resultHmac();

	ap = &auth_token[0];

	for (uint8_t i=0; i<HASH_LENGTH; i++) {
			*ap++ = "0123456789abcdef"[hash[i]>>4];
			*ap++ = "0123456789abcdef"[hash[i]&0xf];
	}
	*ap='\0';

	delete Sha256;
}


/**********************************************************
 Receive GPRS data handler
**********************************************************/
// declare state
enum recv_state_enum {
	HTTP_HEADER_COMPLITED = 0,
	HTTP_HEADER = 1,
	HTTP_HEADER_END = 2,
};

static byte recv_state;
/* Some functions definition */
static inline void WebSocket_Process_JSON();
static inline byte WebSocket_Recv_GetState() { return recv_state; }
static inline void WebSocket_Recv_SetState(byte state) { recv_state = state; }

/**********************************************************

**********************************************************/
void EVENT_WebSocket_RecvByte(byte chr) {

	uint8_t state = WebSocket_Recv_GetState();

	// skip header
	if (HTTP_HEADER_COMPLITED == state) {
		// received json stream has parsed
		if ( json_parse(&json_parser, chr) )
		{
			WebSocket_Process_JSON();
		};

	} else if (HTTP_HEADER == state) {
		if (chr == '\r') return;
		if (chr == '\n') WebSocket_Recv_SetState(HTTP_HEADER_END);

	} else if (HTTP_HEADER_END == state) {
		if (chr == '\r') return;
		if (chr == '\n') WebSocket_Recv_SetState(HTTP_HEADER_COMPLITED);
		else             WebSocket_Recv_SetState(HTTP_HEADER);
	}
}

/**********************************************************

**********************************************************/
inline void WebSocket_Process_JSON() {
	//
	char *event_name, *event_data;

	// get event
	event_name = json_get_tag_value(&json_parser, "event");

	if (event_name) {
	/* -------------------
	 * pusher namespace
	 * -------------------*/
		if (strstr(event_name, "pusher")) {

			// connection handshake
			if (strstr(event_name, "connection_established")) {
				if ((event_data = json_get_tag_value(&json_parser, "data"))) {
					char tmp[24];
					char channel[]=":private-cmd";

					//"{\"socket_id\":\"12035.86349\"}";
					memset(tmp, '\0', 24);
					strncpy(tmp, event_data+14, (strlen(event_data)-2-14));
					strcat(tmp, channel);

					WebSocket_Generate_AuthToken(tmp);

					gsm.TCP_Send(
							PSTR("%d{\"event\":\"%p\",\"data\":{\"channel\":\"private-cmd\",\"auth\":\"%p:%s\"}}%d"),
							0,
							PSTR("pusher:subscribe"),
							Pusher_Key,
							auth_token,
							255	);

					free(event_data);
				}
			}
			// ping-ping
			else if (strstr(event_name, "ping")) {
					gsm.TCP_Send(
						PSTR("%d{\"event\":\"pusher:pong\",\"data\":\"pong\"}%d"),
						0,
						255 );
			}

	/* -------------------
	 * other namespace
	 * -------------------*/
		} else if (strstr(event_name, "-key_")) {

			if ((event_data = json_get_tag_value(&json_parser, "data"))) {

				if (strstr(event_name, "stamp")) {

					Keyboard_SetState(event_data);
					Keyboard_Latch_Stamp();

				} else if (strstr(event_name, "press")) {

					// save old state
					uint8_t oldstamp = Keyboard_GetState();

					Keyboard_SetState(event_data);
					Keyboard_Latch_Stamp();

					delay(Keyboard_Delay_KeyRelease);

					Keyboard_SetState(oldstamp);
					Keyboard_Latch_Stamp();
				}

				gsm.TCP_Send(
					PSTR("%d{\"event\":\"client-responce\",\"data\":\"ok\",\"channel\":\"private-cmd\"}%d"),
					0,
					255 );

				free(event_data);
			}

		}

	}

	// clean garbage
	free(event_name);
	json_clean_tokens(&json_parser);
}


/**********************************************************
	- Connect to WebSocket
	- Send Handshake
	- Subscribe to a channel
**********************************************************/
void WebSocket_Connect() {

	// connect to WS
	gsm.TCP_Connect(F("ws.pusherapp.com"));

	if (CONNECT_OK == gsm.getState()) {
		// set state
		WebSocket_Recv_SetState(HTTP_HEADER);

		// send handshake
		gsm.TCP_Send(
				HTTP_connectWS,
				Pusher_Key,
				PSTR("ws.pusherapp.com")
		);
	}
}


/**********************************************************

**********************************************************/
inline void setup() {
#ifdef DEBUG_PRINT
	Serial.begin(38400);
#endif
	gsm.TurnOn(38400); //module power on
	gsm.InitParam(PARAM_SET_0); //configure the module
	gsm.setRecvHandler(EVENT_WebSocket_RecvByte);

	pinMode(Keyboard_Pin_DATA, OUTPUT);
	pinMode(Keyboard_Pin_LATCH, OUTPUT);
	pinMode(Keyboard_Pin_CLK, OUTPUT);

	// json
	json_init(&json_parser);
}


/**********************************************************

**********************************************************/
static unsigned long last_fetch_time;

inline void loop() {

	if ((unsigned long)(millis() - last_fetch_time) >= 10000) {
		gsm.fetchState();
		last_fetch_time = millis();
	}

	uint8_t state = gsm.getState();

	if (CONNECT_OK == state) {
		gsm.handleCommunication();

	} else if (TCP_CLOSED == state) {
		// TODO: reconnect timeout
		WebSocket_Connect();

	} else if (TCP_CONNECTING == state) {
		// nothing

	} else {
		gsm.GPRS_detach();
		gsm.GPRS_attach();
		WebSocket_Connect();
	}
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

