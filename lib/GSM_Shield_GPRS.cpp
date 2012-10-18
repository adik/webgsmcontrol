/*
 * GSM_Shield_GPRS.cpp
 *
 *  Created on: 29 июля 2012
 *      Author: adik
 *
 *
 *      https://github.com/shklovich/Picon2GSA3/blob/50749272628da61d967e7053455c0ae3c3270b7f/src/modem.c
 */
#include <GSM_Shield_GPRS.h>
#include <GSM_Shield.h>


static gprs_ring_buffer gprs_rx;


/**********************************************************
 Class constructor
**********************************************************/
GPRS::GPRS(void) : GSM(), gprs_state(PDP_DEACT) { }

/**********************************************************

**********************************************************/
void GPRS::handleCommunication()
{
	while (RX_packet()) { // Receive data
		while (gprs_rx.head < gprs_rx.tail) {
			_onReceiveHandler(*gprs_rx.head++);
		}
	}
}


byte GPRS::RX_packet() {

	unsigned long 	prev_time;
	int 			c = 0;
	byte			read_byte_count = 0;
	uint16_t		recv_data_size = 0,
					left_data_size = 0;

	uint16_t		*pt;

	enum req_state_enum
	{
		SEND_PULL_HEADER = 0,
		REQ_HEADER_PARSE,
		GET_DATA
	};

	static byte req_state;

	// init rx buffer
	gprs_rx.head = &gprs_rx.buffer[0];
	gprs_rx.tail = &gprs_rx.buffer[0];


	// init start variables
	req_state = SEND_PULL_HEADER;
	prev_time = millis();

	for (;;) {
		// FIXIT:
		// break if timeout
		if (millis() - prev_time > GPRS_DATA_RECEIVE_TIMEOUT ) {
			//Serial.println("Timeout");
			return 0;
		}

		// receive nn bytes of data
		if (req_state == SEND_PULL_HEADER) {
			mySerial.flush();
			vprintf_P(mySerial, PSTR("AT+CIPRXGET=2,230")); // TODO: calculate buffer size
			mySerial.write('\r');
			req_state = REQ_HEADER_PARSE;
			read_byte_count = 0;
			recv_data_size = 0;
			left_data_size = 0;
			pt = &recv_data_size;
			continue;
		}

		//
		c = mySerial.read();
		if (c < 0) {
			delay(100);
			continue;
		}
		//Serial.write(c);

		// if there are some received bytes postpone the timeout
		prev_time = millis();

		// process states
		switch(req_state) {
		case REQ_HEADER_PARSE:
			//\r\n+CIPRXGET:2,0,0
			if ( ++read_byte_count > 4 ) {
				if (c == '\n') {
					/*#ifdef DEBUG_PRINT
					DebugPrint(F("read_byte_count:"), 0);
					Serial.println(read_byte_count);
					DebugPrint(F("recv_data_size:"), 0);
					Serial.println(recv_data_size);
					DebugPrint(F("left_data_size:"), 0);
					Serial.println(left_data_size);
					#endif*/
					read_byte_count = 0;
					req_state = GET_DATA;
				}
				else if(c >= '0' && c <= '9') {
					*pt = *pt * 10 + c - '0';
				}
				else {
					pt = &left_data_size;
				}
			}
			else {
				WaitUntil_P(mySerial, PSTR("XGET:2,"));
				read_byte_count=5;
			}
			break;

		//
		case GET_DATA:
			if (read_byte_count >= recv_data_size) {
				// going to new iteration because we haven't read all
				if ((left_data_size > 0))
					return 1;
				//	req_state = SEND_PULL_HEADER;
				// end receiving data
				else if (read_byte_count>0)
					return 1;
				else
					return 0;
			}

			// fill RX buffer
			*gprs_rx.tail++ = c;
			++read_byte_count;
			break;
		}
	}//end while
}


/**********************************************************

**********************************************************/
void GPRS::fetchState() {

	// FIXIT:
	delay(200); mySerial.flush();
	vprintf_P(mySerial, PSTR("AT+CIPSTATUS\r"));

	// 5 sec. for initial comm tmout
	// 50 msec. for inter character timeout
	if (RX_FINISHED == WaitResp(5000, 50)) {
	    if(IsStringReceived("INIT")) {
			setState(IP_INITIAL);
	    }
	    else if(IsStringReceived("CP CO")) {
	    	setState(TCP_CONNECTING);
	    }
	    else if(IsStringReceived("T OK")) {
	    	setState(CONNECT_OK);
	    }
	    else if(IsStringReceived("CLOS")) {
	    	setState(TCP_CLOSED);
	    }
	    else {
	    	setState(PDP_DEACT);
	    }
	}
}


/**********************************************************
 Module saves current TCPIP Application Contexts to NVRAM. When
 system is rebooted, the parameters will be loaded automatically.
 **********************************************************/
void GPRS::GPRS_Context2Nvram() {
	SendATCmdWaitResp(1000, 50, "OK", 3, PSTR("AT+CIPSCONT"));
}


/**********************************************************
 Start Up TCP Connection
**********************************************************/
void GPRS::TCP_Connect(const char *str) {
	/*
	 * This command allows establishment of a TCP/UDP connection only
	 *   when the state is IP INITIAL or IP STATUS when it is in single state.
	 *   In multi-IP state, the state is in IP STATUS only. So it is necessary to
	 *   process "AT+CIPSHUT" before user establishes a TCP/UDP
	 *   connection with this command when the state is not IP INITIAL or IP
	 *   STATUS.
	 * When module is in multi-IP state, before this command is executed, it
	 *   is necessary to process "AT+CSTT, AT+CIICR, AT+CIFSR".
	 */
	if (AT_RESP_OK == SendATCmdWaitResp(1000, 5000, "T OK", 1,
					PSTR("AT+CIPSTART=\"TCP\",\"%s\",\"80\""), str)) {

		setState(CONNECT_OK);
	}
}

/**********************************************************
 Start Up TCP Connection
**********************************************************/
void GPRS::TCP_Connect(__FlashStringHelper *prog_str) {
	char buf[32];
	strcpy_P(buf, (const prog_char *)prog_str);
	TCP_Connect(buf);
}

/**********************************************************
 Send data
**********************************************************/
void GPRS::TCP_Send(const prog_char *data, ... ) {

	va_list   		args;
	byte	  		state;
	unsigned long 	prev_time;

	/* This Command is used to send changeable length data
	 * If single IP is connected (+CIPMUX=0)
	 * If connection is not established or module is disconnected: +CME ERROR <err>
	 * If sending is successful:  When +CIPQSEND=0 = "SEND OK"  When +CIPQSEND=1  "DATA ACCEPT:<length>"
	 * If sending fails: "SEND FAIL"
	 */
	if (AT_RESP_OK == SendATCmdWaitResp(2000, 150, ">", 1, PSTR("AT+CIPSEND")))
	{
		va_start(args, data);
		vprintf_P(mySerial, data, args); mySerial.write(0x1A);
		va_end(args);

		WaitUntil_P(mySerial, PSTR("SEND OK"));
		//WaitResp(1000,50);
	}
}


/**********************************************************
 Close TCP connection
**********************************************************/
void GPRS::TCP_Close() {
	/*
	 * AT+CIPCLOSE only closes connection at the status of TCP/UDP which
	 * returns CONNECTING or CONNECT OK, otherwise it will return
	 * ERROR, after the connection is closed, the status is IP CLOSE in single IP mode *
	 */
	if (AT_RESP_OK == SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CIPCLOSE=0"))) {
		setState(TCP_CLOSED);
	}
}


/**********************************************************
 *  Start Task and Set APN
 *  Bring Up Wireless Connection with GPRS and Get Local IP Address
 **********************************************************/
void GPRS::GPRS_attach() {
	byte status;
    byte no_of_attempts = 0;

	/*
	 * Reject Incoming Call
	 */
	SendATCmdWaitResp(1000, 50, "OK", 3, PSTR("AT+GSMBUSY=1"));

	if (AT_RESP_OK == SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CGATT=1"))) {

		/*
		 * Get Data from Network Manually
		 */
		SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CIPRXGET=1"));

		/* The write command and execution command of this command is valid only
		 * at the state of IP INITIAL. After this command is executed, the state will be
		 * changed to IP START.  */
		SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CSTT=\"internet\",\"\",\"\""));

		/* AT+CIICR only activates moving scene at the status of IP START,
		 * after operating this Command is executed, the state will be changed to
		 * IP CONFIG.
		 * After module accepts the activated operation, if it is activated
		 * successfully, module state will be changed to IP GPRSACT, and it
		 * responds OK, otherwise it will responsd ERROR.  */
		SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CIICR"));

		/*
		 * Only after PDP context is activated, local IP Address can be obtained by
		 * AT+CIFSR, otherwise it will respond ERROR. The active status are IP
		 * GPRSACT, TCP/UDP CONNECTING, CONNECT OK, IP CLOSE. */
		do {
			if (++no_of_attempts > 4) {
				setState(IP_BUSY);
				return;
			}
			vprintf_P(mySerial, PSTR("AT+CIFSR")); mySerial.write('\r');
			status = WaitResp(1000, 150);
			delay(500);

		} while ((status != RX_FINISHED) || IsStringReceived("ERROR"));

		setState(IP_STATUS);

		// Add an IP Head at the Beginning of a Package Received
		//      (0)	not add IP header
		//      (1) add IP header, the format is "+IPD,data length:"
		SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CIPHEAD=0"));

		/*
		 * Select TCPIP Application Mode
		 * 0 normal mode
		 * 1 transparent mode
		 */
		SendATCmdWaitResp(1000, 150, "OK", 1, PSTR("AT+CIPMODE=0"));

		/*
		 * Select Data Transmitting Mode
		 *
		 * (0) Normal mode – when the server receives TCP data, it will
		 * responsd SEND OK.
		 *
		 * (1) Quick send mode – when the data is sent to module, it will
		 * responsd DATA ACCEPT:<n>,<length>, while not responding
		 * SEND OK.
		 */
		SendATCmdWaitResp(1000, 150, "OK", 1, PSTR("AT+CIPQSEND=0"));

		/*
		 * prompt '>' char when received AT+CIPSEND command
		 */
		SendATCmdWaitResp(1000, 100, "", 1, PSTR("AT+CIPSPRT=1"));
	}
}


/**********************************************************
  Deactivate GPRS PDP Context
**********************************************************/
void GPRS::GPRS_detach() {
	/* If this command is executed in multi-connection mode, all of the IP
	 * connection will be shut.
	 *   User can close gprs pdp context by AT+CIPSHUT. After it is closed,
	 * the status is IP INITIAL.
	 *   If "+PDP: DEACT" urc is reported which means the gprs is released by
	 * the network, then user still needs to execute "AT+CIPSHUT"
	 * command to make PDP context come back to original state. */
	SendATCmdWaitResp(1000, 150, "OK", 3, PSTR("AT+CIPSHUT"));
	setState(IP_INITIAL);
}
