/* $Id: geiger.h $
 * Functions for the geiger counter - i.e. mainly counting the interrupts
 * coming from it.
 */

#ifndef _GEIGER_H_
#define _GEIGER_H_

/* These can also be accessed directly, but be aware that these could be
 * modified while you read them, unless you disable interrupts! */
#define SIZEOFGEIGERHISTORY (2 * 60) /* 1 hour in 30 second steps */
extern uint16_t geiger_valuehistory[SIZEOFGEIGERHISTORY];
extern uint8_t geiger_historypos;

/* General initialization */
void geiger_init(void);

/* Get data */
uint32_t geiger_get1minavg(void);
uint32_t geiger_get60minavg(void);

#endif /* _GEIGER_H_ */
