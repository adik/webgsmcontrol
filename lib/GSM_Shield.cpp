/*
 GSM.cpp - library for the GSM Playground - GSM Shield for Arduino
 Released under the Creative Commons Attribution-Share Alike 3.0 License
 http://www.creativecommons.org/licenses/by-sa/3.0/
 www.hwkitchen.com
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "GSM_Shield.h"

extern "C" {
  #include <string.h>
}

#define rxPin 2
#define txPin 3

// software serial buffer
ring_buffer rx_buffer_gsm = { { 0 }, 0, 0};

//SoftwareSerial mySerial =  SoftwareSerial(rxPin, txPin);
//SoftwareSerial mySerial(&rx_buffer_gsm, 2, 3);  //rx, tx
SoftwareSerial mySerial(&rx_buffer_gsm, 2, 3);  //rx, tx

/**********************************************************
	DEBUG
**********************************************************/
#ifdef DEBUG_LED_ENABLED
  int DEBUG_LED = 25;                // LED connected to digital pin 25

void  GSM::BlinkDebugLED (byte num_of_blink)
  {
    byte i;

    pinMode(DEBUG_LED, OUTPUT);      // sets the digital pin as output
    for (i = 0; i < num_of_blink; i++) {
      digitalWrite(DEBUG_LED, HIGH);   // sets the LED on
      delay(50);
      digitalWrite(DEBUG_LED, LOW);   // sets the LED off
      delay(500);
    }
  }
#endif

#ifdef DEBUG_PRINT
/**********************************************************
Two methods print out debug information to the standard output
- it means to the serial line.
First method prints string.
Second method prints integer numbers.

Note:
=====
The serial line is connected to the GSM module and is 
used for sending AT commands. There is used "trick" that GSM
module accepts not valid AT command strings because it doesn't
understand them and still waits for some valid AT command.
So after all debug strings are sent we send just AT<CR> as
a valid AT command and GSM module responds by OK. So previous 
debug strings are overwritten and GSM module is not influenced
by these debug texts 


string_to_print:  pointer to the string to be print out
last_debug_print: 0 - this is not last debug info, we will
                      continue with sending... so don't send
                      AT<CR>(see explanation above)
                  1 - we are finished with sending debug info 
                      for this time and finished AT<CR> 
                      will be sent(see explanation above)

**********************************************************/
void GSM::DebugPrint(const char *string_to_print, byte last_debug_print)
{
  if (last_debug_print) {
    Serial.println(string_to_print);
    SendATCmdWaitResp("AT", 500, 50, "OK", 1);
  }
  else Serial.print(string_to_print);
}
void GSM::DebugPrint(__FlashStringHelper *string_to_print, byte last_debug_print)
{
  if (last_debug_print) {
    Serial.println(string_to_print);
    SendATCmdWaitResp("AT", 500, 50, "OK", 1);
  }
  else Serial.print(string_to_print);
}
void GSM::DebugPrint(int number_to_print, byte last_debug_print)
{
  Serial.println(number_to_print);
  if (last_debug_print) {
    SendATCmdWaitResp("AT", 500, 50, "OK", 1);
  }
}
#endif

/**********************************************************
Method returns GSM library version

return val: 100 means library version 1.00
            101 means library version 1.01
**********************************************************/
int GSM::LibVer(void)
{
  return (GSM_LIB_VERSION);
}

/**********************************************************
  Constructor definition
***********************************************************/
GSM::GSM(void)
{
  // set some GSM pins as inputs, some as outputs
  pinMode(GSM_ON, OUTPUT);               // sets pin 5 as output
  pinMode(GSM_RESET, OUTPUT);            // sets pin 4 as output

  //pinMode(DTMF_OUTPUT_ENABLE, OUTPUT);   // sets pin 2 as output
  // deactivation of IC8 so DTMF is disabled by default
  //digitalWrite(DTMF_OUTPUT_ENABLE, LOW);
  
  // not registered yet
  module_status = STATUS_NONE;

  // initialization of speaker volume
  last_speaker_volume = 0; 

}

/**********************************************************
  Initialization of GSM module serial line
**********************************************************/
/*
void GSM::InitSerLine(long baud_rate)
{
  // open the Serial line for the communication
  mySerial.begin(baud_rate);
  //SendATCmdWaitResp("AT+IPR=9600", 500, 50, "OK", 5);
  for (int i=1;i<7;i++){
		switch (i) {
		case 1:
		  mySerial.begin(4800);
		  break;
		case 2:
		  mySerial.begin(9600);
		  break;
		case 3:
		  mySerial.begin(19200);
		  break;
		case 4:
		  mySerial.begin(38400);
		  break;
		case 5:
		  mySerial.begin(57600);
		  break;
		case 6:
		  mySerial.begin(115200);
		  break;
		  // if nothing else matches, do the default
		  // default is optional
		}
	  mySerial.write("AT+IPR=");
	  mySerial.write(baud_rate);    
	  mySerial.write("\r"); // send <CR>
  }
  
  // communication line is not used yet = free
  SetCommLineStatus(CLS_FREE);
  // pointer is initialized to the first item of comm. buffer
  p_comm_buf = &comm_buf[0];
}
*/
/**********************************************************
  Initializes receiving process

  start_comm_tmout    - maximum waiting time for receiving the first response
                        character (in msec.)
  max_interchar_tmout - maximum tmout between incoming characters 
                        in msec.
  if there is no other incoming character longer then specified
  tmout(in msec) receiving process is considered as finished
**********************************************************/
void GSM::RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout)
{
  rx_state = RX_NOT_STARTED;
  start_reception_tmout = start_comm_tmout;
  interchar_tmout = max_interchar_tmout;
  prev_time = millis();
  comm_buf[0] = 0x00; // end of string
  p_comm_buf = &comm_buf[0];
  comm_buf_len = 0;
  st = 0;
//  mySerial.flush(); // erase rx circular buffer
}

/**********************************************************
Method checks if receiving process is finished or not.
Rx process is finished if defined inter-character tmout is reached

returns:
        RX_NOT_FINISHED = 0,// not finished yet
        RX_FINISHED,        // finished - inter-character tmout occurred
        RX_TMOUT_ERR,       // initial communication tmout occurred
**********************************************************/
byte GSM::IsRxFinished(void)
{
  int  c;
  byte num_of_bytes;
  byte ret_val = RX_NOT_FINISHED;  // default not finished

  // Rx state machine
  // ----------------

  if (rx_state == RX_NOT_STARTED) {
    // Reception is not started yet - check tmout
    if (!mySerial.available()) {
      // still no character received => check timeout
	/*
	#ifdef DEBUG_GSMRX

			DebugPrint("\r\nDEBUG: reception timeout", 0);			
			Serial.print((unsigned long)(millis() - prev_time));	
			DebugPrint("\r\nDEBUG: start_reception_tmout\r\n", 0);			
			Serial.print(start_reception_tmout);	


	#endif
	*/
      if ((unsigned long)(millis() - prev_time) >= start_reception_tmout) {
        // timeout elapsed => GSM module didn't start with response
        // so communication is takes as finished
		/*
			#ifdef DEBUG_GSMRX		
				DebugPrint("\r\nDEBUG: RECEPTION TIMEOUT", 0);	
			#endif
		*/
        comm_buf[comm_buf_len] = 0x00;
        ret_val = RX_TMOUT_ERR;
      }
    }
    else {
      // at least one character received => so init inter-character 
      // counting process again and go to the next state
      prev_time = millis(); // init tmout for inter-character space
      rx_state = RX_ALREADY_STARTED;
    }
  }

  if (rx_state == RX_ALREADY_STARTED) {
    // Reception already started
    // check new received bytes
    // only in case we have place in the buffer
    num_of_bytes = mySerial.available();
      
    // read all received bytes      
    while (num_of_bytes) {
      num_of_bytes--;
      c = mySerial.read();

      // if there are some received bytes postpone the timeout
      if (num_of_bytes == 0) prev_time = millis();

      if (c > 0 && comm_buf_len < COMM_BUF_LEN) {
        // we have still place in the GSM internal comm. buffer =>
        // move available bytes from circular buffer 
        // to the rx buffer
        *p_comm_buf = c;

        p_comm_buf++;
        comm_buf_len++;
        comm_buf[comm_buf_len] = 0x00;  // and finish currently received characters
                                        // so after each character we have
                                        // valid string finished by the 0x00
      } else {
    	  ret_val = RX_FINISHED;
    	  goto finish;
    	  break;
      }
    }

    // finally check the inter-character timeout 
    if ((unsigned long)(millis() - prev_time) >= interchar_tmout) {
      // timeout between received character was reached
      // reception is finished
      // ---------------------------------------------
      comm_buf[comm_buf_len] = 0x00;  // for sure finish string again
                                      // but it is not necessary
      ret_val = RX_FINISHED;
    }
  }

finish:

#ifdef DEBUG_GSMRX
	if (ret_val == RX_FINISHED){
		DebugPrint(F("\r\nDEBUG: Received string\r\n"), 0);
		for (int i=0; i<comm_buf_len; i++){
			Serial.write(byte(comm_buf[i]));
		}
	}
#endif
	
  return (ret_val);
}

/**********************************************************
Method checks received bytes

compare_string - pointer to the string which should be find

return: 0 - string was NOT received
        1 - string was received
**********************************************************/
byte GSM::IsStringReceived(char const *compare_string)
{
  char *ch;
  byte ret_val = 0;

  if(comm_buf_len) {
  /*
		#ifdef DEBUG_GSMRX
			DebugPrint(F("DEBUG: Compare the string: \r\n"), 0);
			for (int i=0; i<comm_buf_len; i++){
				Serial.print(byte(comm_buf[i]));	
			}
			
			DebugPrint(F("\r\nDEBUG: with the string: \r\n"), 0);
			Serial.print(compare_string);	
			DebugPrint(F("\r\n"), 0);
		#endif
	*/
    ch = strstr((char *)comm_buf, compare_string);
    if (ch != NULL) {
      ret_val = 1;
	  /*#ifdef DEBUG_PRINT
		DebugPrint(F("\r\nDEBUG: expected string was received\r\n"), 0);
	  #endif
	  */
    }
	else
	{
	  /*#ifdef DEBUG_PRINT
		DebugPrint(F("\r\nDEBUG: expected string was NOT received\r\n"), 0);
	  #endif
	  */
	}
  }

  return (ret_val);
}

/**********************************************************
Method waits for response

      start_comm_tmout    - maximum waiting time for receiving the first response
                            character (in msec.)
      max_interchar_tmout - maximum tmout between incoming characters 
                            in msec.  
return: 
      RX_FINISHED         finished, some character was received

      RX_TMOUT_ERR        finished, no character received 
                          initial communication tmout occurred
**********************************************************/
byte GSM::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout)
{
  byte status;

  RxInit(start_comm_tmout, max_interchar_tmout); 
  // wait until response is not finished
  do {
    status = IsRxFinished();
  } while (status == RX_NOT_FINISHED);

  return (status);
}


/**********************************************************
Method waits for response with specific response string
    
      start_comm_tmout    - maximum waiting time for receiving the first response
                            character (in msec.)
      max_interchar_tmout - maximum tmout between incoming characters 
                            in msec.  
      expected_resp_string - expected string
return: 
      RX_FINISHED_STR_RECV,     finished and expected string received
      RX_FINISHED_STR_NOT_RECV  finished, but expected string not received
      RX_TMOUT_ERR              finished, no character received 
                                initial communication tmout occurred
**********************************************************/
byte GSM::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, 
                   char const *expected_resp_string)
{
  byte status;
  byte ret_val;

  status = WaitResp(start_comm_tmout, max_interchar_tmout);

  if (status == RX_FINISHED) {
    // something was received but what was received?
    // ---------------------------------------------
	
    if(IsStringReceived(expected_resp_string)) {
      // expected string was received
      // ----------------------------
      ret_val = RX_FINISHED_STR_RECV;      
    }
    else ret_val = RX_FINISHED_STR_NOT_RECV;
  }
  else {
    // nothing was received
    // --------------------
    ret_val = RX_TMOUT_ERR;
  }
  return (ret_val);
}

/**********************************************************

**********************************************************/
void GSM::vprintf_P(Stream &stream, const prog_char *fmt, ...) {
	va_list   args;
	va_start(args, fmt);
	vprintf_P(stream, fmt, args);
	va_end(args);
}

/**********************************************************

**********************************************************/
void GSM::vprintf_P(Stream &stream, const prog_char *fmt, va_list args ) {
	char 			*p, c, c2, zero;
	uint8_t 		i8;

	while ((c = pgm_read_byte(fmt))) {
		if (c == '%') {
			fmt++;
			c2 = pgm_read_byte(fmt);
			zero = (char)((c2 == '0') ? '0' : ' ');
			switch (c2) {
			case 's':
				p = va_arg(args, char *);
				// write to
				if (p != NULL) {
					while (*p) {
						stream.write(*p++);
					} //
				}
				fmt++;
				continue;
			case 'p':
				p = va_arg(args, prog_char *);
				// write to
				if (p != NULL) {
					while (1) {
					    unsigned char c = pgm_read_byte(p++);
					    if (c == 0) break;
						stream.write(c);
					} //
				}
				fmt++;
				continue;
			case 'd':
				i8 = (uint8_t) va_arg(args, int);
				// write to
				stream.write(i8);
				fmt++;
				continue;
			default:
				//write to
				stream.write('%');
				stream.write(c2);
				//Serial.write('%');
				//Serial.write(c2);
				break;
			}
			fmt++;
		} else {
			//write to buffer
			stream.write(c);
			//Serial.write(c);
			fmt++;
		}
	}
}

/**********************************************************

**********************************************************/
char GSM::SendATCmdWaitResp(uint16_t start_comm_tmout,
		uint16_t max_interchar_tmout, char const *response_string,
		byte no_of_attempts, const prog_char *fmt, ...)
{
	va_list args;
	byte	status;
	char 	ret_val = AT_RESP_ERR_NO_RESP;
	byte 	i;

	//SetCommLineStatus(CLS_ATCMD);

	for (i = 0; i < no_of_attempts; i++) {
		// delay 500 msec. before sending next repeated AT command
		// so if we have no_of_attempts=1 tmout will not occurred
		if (i > 0)
			delay(500);

		va_start(args, fmt);
		vprintf_P(mySerial, fmt, args); mySerial.write('\r');
		va_end(args);

		// get response
		status = WaitResp(start_comm_tmout, max_interchar_tmout);

		if (status == RX_FINISHED) {
			// something was received but what was received?
			// ---------------------------------------------
			if (IsStringReceived(response_string)) {
				ret_val = AT_RESP_OK;
				break; // response is OK => finish
			} else
				ret_val = AT_RESP_ERR_DIF_RESP;
		} else {
			// nothing was received
			// --------------------
			ret_val = AT_RESP_ERR_NO_RESP;
		}
	}

	//SetCommLineStatus(CLS_FREE);
	return (ret_val);
}


/**********************************************************
Method sends AT command and waits for response

return: 
      AT_RESP_ERR_NO_RESP = -1,   // no response received
      AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
      AT_RESP_OK = 1,             // response_string was included in the response
**********************************************************/
char GSM::SendATCmdWaitResp(char const *AT_cmd_string,
                uint16_t start_comm_tmout, uint16_t max_interchar_tmout,
                char const *response_string,
                byte no_of_attempts)
{
  byte status;
  char ret_val = AT_RESP_ERR_NO_RESP;
  byte i;

  //SetCommLineStatus(CLS_ATCMD);

  for (i = 0; i < no_of_attempts; i++) {
    // delay 500 msec. before sending next repeated AT command 
    // so if we have no_of_attempts=1 tmout will not occurred
    if (i > 0) delay(500); 

    mySerial.println(AT_cmd_string);
    status = WaitResp(start_comm_tmout, max_interchar_tmout); 
    if (status == RX_FINISHED) {
      // something was received but what was received?
      // ---------------------------------------------
      if(IsStringReceived(response_string)) {
        ret_val = AT_RESP_OK;      
        break;  // response is OK => finish
      }
      else ret_val = AT_RESP_ERR_DIF_RESP;
    }
    else {
      // nothing was received
      // --------------------
      ret_val = AT_RESP_ERR_NO_RESP;
    }
    
  }

  //SetCommLineStatus(CLS_FREE);
  return (ret_val);
}


/**********************************************************
Methods return the state of corresponding
bits in the status variable

- these methods do not communicate with the GSM module

return values: 
      0 - not true (not active)
      >0 - true (active)
**********************************************************/
byte GSM::IsRegistered(void)
{
  return (module_status & STATUS_REGISTERED);
}

byte GSM::IsInitialized(void)
{
  return (module_status & STATUS_INITIALIZED);
}

/**********************************************************
  Checks if the GSM module is responding 
  to the AT command
  - if YES nothing is made 
  - if NO GSM module is turned on 
**********************************************************/
void GSM::TurnOn(long baud_rate)
{
  SetCommLineStatus(CLS_ATCMD);
  mySerial.begin(baud_rate);
  
  #ifdef DEBUG_PRINT
    // parameter 0 - because module is off so it is not necessary 
    // to send finish AT<CR> here
    DebugPrint(F("DEBUG: baud "), 0);
	DebugPrint(baud_rate, 0);
#endif
  
  if (AT_RESP_ERR_NO_RESP == SendATCmdWaitResp("AT", 500, 100, "OK", 5)) {		//check power
    // there is no response => turn on the module
  
		#ifdef DEBUG_PRINT
			// parameter 0 - because module is off so it is not necessary 
			// to send finish AT<CR> here
			DebugPrint(F("DEBUG: GSM module is off\r\n"), 0);
			DebugPrint(F("DEBUG: start the module\r\n"), 0);
		#endif
		
		// generate turn on pulse
		digitalWrite(GSM_ON, HIGH);
		delay(1200);
		digitalWrite(GSM_ON, LOW);
		delay(5000);
	}
	else
	{
		#ifdef DEBUG_PRINT
			// parameter 0 - because module is off so it is not necessary 
			// to send finish AT<CR> here
			DebugPrint(F("DEBUG: GSM module is on\r\n"), 0);
		#endif
	}
	if (AT_RESP_ERR_DIF_RESP == SendATCmdWaitResp("AT", 500, 100, "OK", 5)) {		//check OK
			
		#ifdef DEBUG_PRINT
			// parameter 0 - because module is off so it is not necessary 
			// to send finish AT<CR> here
			DebugPrint(F("DEBUG: the baud is not ok\r\n"), 0);
		#endif
			  //SendATCmdWaitResp("AT+IPR=9600", 500, 50, "OK", 5);
			  for (int i=1;i<7;i++){
					switch (i) {
					case 1:
					  mySerial.begin(4800);
						#ifdef DEBUG_PRINT
							DebugPrint(F("DEBUG: provo Baud 4800\r\n"), 0);
						#endif
					  break;
					case 2:
					  mySerial.begin(9600);
					  #ifdef DEBUG_PRINT
							DebugPrint(F("DEBUG: provo Baud 9600\r\n"), 0);
						#endif
					  break;
					case 3:
					  mySerial.begin(19200);
					  #ifdef DEBUG_PRINT
							DebugPrint(F("DEBUG: provo Baud 19200\r\n"), 0);
						#endif
					  break;
					case 4:
					  mySerial.begin(38400);
					  #ifdef DEBUG_PRINT
							DebugPrint(F("DEBUG: provo Baud 38400\r\n"), 0);
						#endif
					  break;
					case 5:
					  mySerial.begin(57600);
					  #ifdef DEBUG_PRINT
							DebugPrint(F("DEBUG: provo Baud 57600\r\n"), 0);
						#endif
					  break;
					case 6:
					  mySerial.begin(115200);
					  #ifdef DEBUG_PRINT
							DebugPrint(F("DEBUG: provo Baud 115200\r\n"), 0);
						#endif
					  break;
					  // if nothing else matches, do the default
					  // default is optional
					}
					
					/*
					  p_char = strchr((char *)(comm_buf),',');
					  p_char1 = p_char+2; // we are on the first phone number character
					  p_char = strchr((char *)(p_char1),'"');
					  if (p_char != NULL) {
						*p_char = 0; // end of string
						strcpy(phone_number, (char *)(p_char1));
					  }
					*/  
	  
	  
				  delay(100);
				  /*sprintf (buff,"AT+IPR=%f",baud_rate);
					#ifdef DEBUG_PRINT
						// parameter 0 - because module is off so it is not necessary 
						// to send finish AT<CR> here
						DebugPrint(F("DEBUG: Stringa "), 0);
						DebugPrint(buff, 0);
					#endif
					*/
				  mySerial.write("AT+IPR=");
				  mySerial.write(baud_rate);    
				  mySerial.write("\r"); // send <CR>
				  delay(500);
				  mySerial.begin(baud_rate);
				  delay(100);
				  if (AT_RESP_OK == SendATCmdWaitResp("AT", 500, 100, "OK", 5)){
						#ifdef DEBUG_PRINT
							// parameter 0 - because module is off so it is not necessary 
							// to send finish AT<CR> here
							DebugPrint(F("DEBUG: ricevuto ok da modulo, baud impostato: "), 0);
							DebugPrint(baud_rate, 0);	
						#endif
						break;					
				}
				  
			  }
			  
			  // communication line is not used yet = free
			  SetCommLineStatus(CLS_FREE);
			  // pointer is initialized to the first item of comm. buffer
			  p_comm_buf = &comm_buf[0];
  
  
		if (AT_RESP_ERR_NO_RESP == SendATCmdWaitResp("AT", 500, 50, "OK", 5)) {
			#ifdef DEBUG_PRINT
				// parameter 0 - because module is off so it is not necessary 
				// to send finish AT<CR> here
				DebugPrint(F("DEBUG: No answer from the module\r\n"), 0);
			#endif
		}
		else{
	
			#ifdef DEBUG_PRINT
				// parameter 0 - because module is off so it is not necessary 
				// to send finish AT<CR> here
				DebugPrint(F("DEBUG: 1 baud ok\r\n"), 0);
			#endif
		}


	}
	else
	{
		#ifdef DEBUG_PRINT
			DebugPrint(F("DEBUG: 2 GSM module is on and baud is ok\r\n"), 0);
		#endif
  
	}
  SetCommLineStatus(CLS_FREE);

  // send collection of first initialization parameters for the GSM module    
  InitParam(PARAM_SET_0);
}


/**********************************************************
  Sends parameters for initialization of GSM module

  group:  0 - parameters of group 0 - not necessary to be registered in the GSM
          1 - parameters of group 1 - it is necessary to be registered
**********************************************************/
void GSM::InitParam(byte group)
{

  switch (group) {
    case PARAM_SET_0:
      // check comm line
      if (CLS_FREE != GetCommLineStatus()) return;
	  
	  	#ifdef DEBUG_PRINT
			DebugPrint(F("DEBUG: configure the module PARAM_SET_0\r\n"), 0);
		#endif
      SetCommLineStatus(CLS_ATCMD);

      // Reset to the factory settings
      SendATCmdWaitResp(1000, 50, "OK", 5, PSTR("AT&F"));
      // switch off echo
      SendATCmdWaitResp(500, 50, "OK", 5, PSTR("ATE0"));
      // setup fixed baud rate
      //SendATCmdWaitResp("AT+IPR=9600", 500, 50, "OK", 5);
      // setup mode
      //SendATCmdWaitResp("AT#SELINT=1", 500, 50, "OK", 5);
      // Switch ON User LED - just as signalization we are here
      //SendATCmdWaitResp("AT#GPIO=8,1,1", 500, 50, "OK", 5);
      // Sets GPIO9 as an input = user button
      //SendATCmdWaitResp("AT#GPIO=9,0,0", 500, 50, "OK", 5);
      // allow audio amplifier control
      //SendATCmdWaitResp("AT#GPIO=5,0,2", 500, 50, "OK", 5);
      // Switch OFF User LED- just as signalization we are finished
      //SendATCmdWaitResp("AT#GPIO=8,0,1", 500, 50, "OK", 5);
      SetCommLineStatus(CLS_FREE);
      break;

    case PARAM_SET_1:
      // check comm line
      if (CLS_FREE != GetCommLineStatus()) return;
	  
	  	#ifdef DEBUG_PRINT
			DebugPrint(F("DEBUG: configure the module PARAM_SET_1\r\n"), 0);
		#endif
      SetCommLineStatus(CLS_ATCMD);

      // Request calling line identification
      SendATCmdWaitResp(500, 50, "OK", 5, PSTR("AT+CLIP=1"));
      // Mobile Equipment Error Code
      SendATCmdWaitResp(500, 50, "OK", 5, PSTR("AT+CMEE=0"));
      // Echo canceller enabled 
      //SendATCmdWaitResp("AT#SHFEC=1", 500, 50, "OK", 5);
      // Ringer tone select (0 to 32)
      //SendATCmdWaitResp("AT#SRS=26,0", 500, 50, "OK", 5);
      // Microphone gain (0 to 7) - response here sometimes takes 
      // more than 500msec. so 1000msec. is more safety
      //SendATCmdWaitResp("AT#HFMICG=7", 1000, 50, "OK", 5);
      // set the SMS mode to text 
      SendATCmdWaitResp(500, 50, "OK", 5, PSTR("AT+CMGF=1"));
      // Auto answer after first ring enabled
      // auto answer is not used
      //SendATCmdWaitResp("ATS0=1", 500, 50, "OK", 5);

      // select ringer path to handsfree
      //SendATCmdWaitResp("AT#SRP=1", 500, 50, "OK", 5);
      // select ringer sound level
      //SendATCmdWaitResp("AT+CRSL=2", 500, 50, "OK", 5);
      // we must release comm line because SetSpeakerVolume()
      // checks comm line if it is free
      SetCommLineStatus(CLS_FREE);
      // select speaker volume (0 to 14)
      //SetSpeakerVolume(9);
      // init SMS storage
      //InitSMSMemory();
      // select phonebook memory storage
      SendATCmdWaitResp(1000, 50, "OK", 5, PSTR("AT+CPBS=\"SM\""));
      break;
  }
  
}

/**********************************************************
NEW TDGINO FUNCTION
***********************************************************/


/******************	****************************************
Function to enable or disable echo
Echo(1)   enable echo mode
Echo(0)   disable echo mode
**********************************************************/
void GSM::Echo(byte state)
{
	if (state == 0 or state == 1)
	{
	  SetCommLineStatus(CLS_ATCMD);
	  #ifdef DEBUG_PRINT
		DebugPrint(F("DEBUG Echo\r\n"),1);
	  #endif
	  vprintf_P(mySerial, PSTR("ATE"));
	  mySerial.write((int)state);    
	  mySerial.write("\r");
	  WaitResp(1000, 50);
	  delay(500);
	  SetCommLineStatus(CLS_FREE);
	}
}


