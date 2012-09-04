/*
 * Author: smirnov.arkady@gmail.com
 *
 * Copyright (c) 2012 Arkady Smirnov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#ifndef Simple_Json_Parser_H_
#define Simple_Json_Parser_H_

/*****************************************************************************/
#ifndef ARDUINO
#include "stddef.h"
#include "string.h"
#include "stdlib.h"
#define _BV(bit) (1 << (bit))
typedef unsigned int uint8_t;
typedef uint8_t boolean;
typedef uint8_t byte;
#else
#include "Arduino.h"
#endif
/*****************************************************************************/


/******************************************************************************
* Definitions
******************************************************************************/

// Max depth that could be parsed
#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH	2
#endif

// a lenght of data buffer
#ifndef JSON_MAX_DATA_BUFFER
#define JSON_MAX_DATA_BUFFER 128
#endif

// max available tokens
#ifndef JSON_MAX_TOKENS
#define JSON_MAX_TOKENS 12
#endif



struct json_token_t {
	size_t left;
	size_t right;
};

struct json_flag_t {
	byte is_val:1;
	byte skip_next:1;
};

struct json_parser_t {
	uint8_t			level;	// depth level
	json_flag_t		flags;	//
	size_t			ref[JSON_MAX_DEPTH];	// references array

	json_token_t	tokens[JSON_MAX_TOKENS];
	json_token_t	*ptoken;

	char			data[JSON_MAX_DATA_BUFFER];
	char			*pdata;
};

void json_init(json_parser_t *parser);
void json_clean_tokens(json_parser_t *p);
int  json_parse(json_parser_t *p, char chr);

void		 *json_get_token(json_parser_t *p, json_token_t *tok, char *buff, size_t len);
size_t		  json_token_size(json_parser_t *p, json_token_t *tok);
json_token_t *json_find_tag_value_token(json_parser_t *p, char *str);
char		 *json_get_tag_value(json_parser_t *p, char *tag);


#endif /* Simple_Json_Parser_H_ */
