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
#include <SimpleJsonParser.h>

#define json_fill_buff_index (((size_t)p->pdata - (size_t)&p->data[0]))

/*
 *
 */
void json_init(json_parser_t *p) {
	json_clean_tokens(p);
}

/*
 *  Clean buffers
 */
void json_clean_tokens(json_parser_t *p) {
// find token
	for (int i=0; i<JSON_MAX_TOKENS; ++i ) {
		p->tokens[i].left 	= 0;
		p->tokens[i].right  = 0;
	}
	p->data[0] = '\0';
	p->pdata  = &p->data[0];
	p->ptoken = &p->tokens[0];
}


/*
 *
 */
void json_fill_token(json_token_t *& ptoken, size_t addr) {
	ptoken->right = addr;
	ptoken++;
	ptoken->left  = addr;
}

int json_parse(json_parser_t *p, char chr) {

	// skip until find first {
	if (p->level <= 0) {
		if (chr == '{') {
			// Set first start pointer
			p->ptoken->left  = (size_t) p->pdata;
		}
		else { goto skip; }
	}

	// check if this not
	if (chr == '"') {
		p->type ^= JSON_FIELD_MASK;
	}

	//if field closed
	if ( (p->type & JSON_FIELD_MASK) == 0 ) {

		// calculate level
		switch(chr) {
			case '{':
				// save reference to a start group token
				p->ref[p->level] = (size_t) p->ptoken;
				p->level++;
				break;
			case '}':

				// FIXIT:
				if (p->level >= JSON_MAX_DEPTH){
					if ( json_fill_buff_index >= JSON_MAX_DATA_BUFFER)
						goto error;
					*p->pdata++ = chr;;
				}

				p->level--;
				// update start group right reference
				((json_token_t*) p->ref[p->level])->right = (size_t) p->pdata;

				// nothing to parse anymore
				// end function
				if (p->level == 0)
					goto finish;
				else
					goto skip;
		}

		if (p->level >= JSON_MAX_DEPTH)
			goto find;

		// parse primitive
		switch(chr) {
			// change type
			case ':': case ',': case '{':
			case '}':
				p->ptoken->right = (size_t) p->pdata;
				p->ptoken++;
				p->ptoken->left  = (size_t) p->pdata;
				goto skip;
				break;

			// remove spaces
			case ' ': case '\r': case '\n':
				goto skip;
				break;
		}
	}

	if (chr == '"')
		goto skip;

find:
	// buffer overflow
	if (json_fill_buff_index >= JSON_MAX_DATA_BUFFER)
		goto error;

	*p->pdata++ = chr;

// wait next character
skip:
	return 0;

// error
error:
	json_clean_tokens(p);
	return 0;

// parse finished
finish:
	p->ptoken->right = (size_t) p->pdata;
	// clear other
	p->ptoken++;
	p->ptoken->left = 0;
	p->ptoken->right = 0;

	return 1;
}

/*
 *  Calculate token size
 */
size_t json_token_size(json_parser_t *p, json_token_t *tok) {
	return tok->right - tok->left;
}

/*
 *  Copy token value to buffer
 */
void *json_get_token(json_parser_t *p, json_token_t *tok, char *buff, size_t len) {
	memcpy(buff, (char *)(tok->left), len);
	buff[len] = '\0';
}

/*
 * Return reference to the finded token
 */
json_token_t *json_find_token(json_parser_t *p, char *str) {

	char *start_addr;

	if ( !(start_addr = strstr(p->data, str)) )
		return 0;

	size_t addr = (size_t)start_addr + strlen(str);

	// find token
	for (int i=0; i<JSON_MAX_TOKENS; i += 2 ) {
		if ( p->tokens[i].left == addr ) {
			return &p->tokens[i];
		}
	}
	return 0;
}
