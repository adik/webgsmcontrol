/*
 * GSM_Shield_GPRS.h
 *
 *  Created on: 29 июля 2012
 *      Author: adik
 */

#ifndef GSM_SHIELD_SAPBR_H_
#define GSM_SHIELD_SAPBR_H_

#include <GSM_Shield.h>

#define PROGMEM_BUFF_SIZE	64

# define PGM_STR(name, val)  const char name[] PROGMEM = val
# define PGM_STRING_ARRAY(name, values...) const PROGMEM char *name##_arr[] = { values };

# define PGM_FETCH_STR(name, val) { strncpy_P(progmem_buffer, (char*)pgm_read_word(&(name##_arr[val])), PROGMEM_BUFF_SIZE); }

# define SFETCH(val) PGM_FETCH_STR(PROGMEM_str_sapbr, C_##val)
# define SRET()  progmem_buffer



class GPRS_SAPBR : public GSM
{
public:
	GPRS_SAPBR(void);

	char SendATCmd(char const*);

	char InitBearer();
	char DeactBearer();

	char HTTPInit();
	char HTTPTerm();

	char HTTPUrl(char *);

	char HTTPGet();
	char HTTPPost();

private:
};



#endif /* GSM_SHIELD_GPRS_H_ */
