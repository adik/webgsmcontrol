/*
 * GSM_Shield_GPRS.cpp
 *
 *  Created on: 29 июля 2012
 *      Author: adik
 */

#include <GSM_Shield_SAPBR.h>
#include <GSM_Shield.h>



static char progmem_buffer[PROGMEM_BUFF_SIZE];


enum gprs_sapbr_atcommands
{
	C_GSM_BUSY = 0,
	C_GPRS_INIT,
	C_BEARER_INIT,
	C_BEARER_TERM,
	C_BEARER_SET_CONTYPE,
	C_BEARER_SET_APN,
	C_HTTP_INIT,
	C_HTTP_TERM,
	C_HTTP_SETURL,
	C_HTTP_ACTION,
};

PGM_STR( S_GSM_BUSY, "AT+GSMBUSY=1");
PGM_STR( S_GPRS_INIT, "AT+CGATT=1");
PGM_STR( S_BEARER_INIT, "AT+SAPBR=1,1");
PGM_STR( S_BEARER_TERM, "AT+SAPBR=0,1");
PGM_STR( S_BEARER_SET_CONTYPE, "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
PGM_STR( S_BEARER_SET_APN, "AT+SAPBR=3,1,\"APN\",\"internet\"");
PGM_STR( S_HTTP_INIT, "AT+HTTPINIT");
PGM_STR( S_HTTP_TERM, "AT+HTTPTERM");
PGM_STR( S_HTTP_SETURL, "AT+HTTPPARA=\"URL\",\"%s\"");
PGM_STR( S_HTTP_ACTION, "AT+HTTPACTION=%d");


PGM_STRING_ARRAY( PROGMEM_str_sapbr,
		S_GSM_BUSY,
		S_GPRS_INIT,
		S_BEARER_INIT,
		S_BEARER_TERM,
		S_BEARER_SET_CONTYPE,
		S_BEARER_SET_APN,
		S_HTTP_INIT,
		S_HTTP_TERM,
		S_HTTP_SETURL,
		S_HTTP_ACTION,
);


// Constructor
//
GPRS_SAPBR::GPRS_SAPBR(void) {
}

char GPRS_SAPBR::SendATCmd(char const *AT_cmd_string) {
	if (CLS_FREE != GetCommLineStatus())
		return 0;
	SetCommLineStatus(CLS_ATCMD);
	mySerial.println(AT_cmd_string);
	WaitResp(1000, 50);
	SetCommLineStatus(CLS_FREE);
	return 1;
}

char GPRS_SAPBR::InitBearer() {
	SFETCH(GPRS_INIT);
	if (AT_RESP_OK == SendATCmdWaitResp(SRET(), 1000, 50, "OK", 2))
	{
		SFETCH(BEARER_SET_CONTYPE);
		SendATCmdWaitResp(SRET(), 1000, 50, "OK", 5);

		SFETCH(BEARER_SET_APN);
		SendATCmdWaitResp(SRET(), 1000, 50, "OK", 1);

		// Open bearer
		SFETCH(BEARER_INIT);
		SendATCmdWaitResp(SRET(), 1000, 50, "OK", 1);
	}
	return 1;
}

char GPRS_SAPBR::DeactBearer() {
	SFETCH(BEARER_TERM);
	return SendATCmd(SRET());
}
char GPRS_SAPBR::HTTPInit() {
	SFETCH(HTTP_INIT);
	return SendATCmdWaitResp(SRET(), 1000, 50, "OK", 3);
}
char GPRS_SAPBR::HTTPTerm() {
	SFETCH(HTTP_TERM);
	return SendATCmdWaitResp(SRET(), 1000, 50, "OK", 3);
}

char GPRS_SAPBR::HTTPUrl(char *url) {
	char buff[50];

	if (CLS_FREE != GetCommLineStatus())
		return 0;

	sprintf_P(buff, (char*)pgm_read_word(&(PROGMEM_str_sapbr_arr[C_HTTP_SETURL])), url);
	mySerial.println(buff);
	WaitResp(1000, 50);
	SetCommLineStatus(CLS_FREE);
	return 1;
}

char GPRS_SAPBR::HTTPGet() {
	char buff[50];

	if (CLS_FREE != GetCommLineStatus())
		return 0;

	sprintf_P(buff, (char*)pgm_read_word(&(PROGMEM_str_sapbr_arr[C_HTTP_ACTION])), 0);
	mySerial.println(buff);
	WaitResp(1000, 50);
	SetCommLineStatus(CLS_FREE);
	return 1;
}

char GPRS_SAPBR::HTTPPost() {
	//AT+HTTPACTION=1

	return 0;
}

