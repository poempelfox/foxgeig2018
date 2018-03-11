/* $Id: geiger.c $
 * Functions for the geiger counter
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "geiger.h"
#include "lufa/console.h"

static uint8_t t3ovfcnt = 0;

ISR(TIMER3_CAPT_vect)
{
  t3ovfcnt++;
  if (t3ovfcnt == 5) {
    /* FIXME do internal work here, 30 seconds have passed. */
    console_printpgm_noirq_P(PSTR(" !30s! "));
    t3ovfcnt = 0;
  }
}

void geiger_init(void)
{
  /* the number of timer ticks in 30 seconds can be cleanly divided by 5. */
  ICR3H = (234375UL / 5) >> 8;
  ICR3L = (234375UL / 5) & 0xff;
  /* Set CTC-mode with the MAX taken from ICR3.
   * Select prescaler /1024, this results in 234375 timer-ticks in 30 seconds. */
  TCCR3A = 0x00;
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS32) | _BV(CS30);
  TIMSK3 |= _BV(ICIE3);
  TIFR3 |= _BV(ICF3);
}
