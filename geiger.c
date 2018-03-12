/* $Id: geiger.c $
 * Functions for the geiger counter
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "geiger.h"
#include "lufa/console.h"

static uint8_t t3ovfcnt = 0;
static uint16_t currentgeigcount = 0;
static uint16_t ticks = 0;

uint16_t geiger_valuehistory[SIZEOFGEIGERHISTORY];
uint8_t geiger_historypos = 0;

ISR(TIMER3_CAPT_vect)
{
  t3ovfcnt++;
  ticks++;
  if (t3ovfcnt == 5) {
    /* console_printpgm_noirq_P(PSTR(" !30s! ")); */
    /* 30 seconds have passed, record current value. */
    geiger_valuehistory[geiger_historypos] = currentgeigcount;
    currentgeigcount = 0;
    geiger_historypos++;
    if (geiger_historypos >= SIZEOFGEIGERHISTORY) { geiger_historypos = 0; }
    t3ovfcnt = 0;
  }
}

/* This is where the interrupts from the geiger counter end up:
 * It's connected to PD0 / SCL / INT0 */
ISR(INT0_vect)
{
  /* 0xffff is a special value meaning 'invalid', so we make sure to never count to that. */
  if (currentgeigcount < 0xfffe) {
    currentgeigcount++;
  }
}
uint32_t geiger_get1minavg(void)
{
  uint32_t sum = 0;
  uint8_t readpos;
  uint8_t numvalid = 0;
  cli(); /* to make sure the history does not get modified while we count */
  readpos = geiger_historypos;
  for (uint8_t i = 0; i < 2; i++) {
    if (readpos == 0) {
      readpos = SIZEOFGEIGERHISTORY - 1;
    } else {
      readpos--;
    }
    if (geiger_valuehistory[readpos] != 0xffff) {
      numvalid++;
      sum += geiger_valuehistory[readpos];
    }
  }
  sei();
  if (numvalid > 0) {
    return (sum * 2 / numvalid); /* We return the 1 min avg, not the 30s avg! */
  } else {
    return 0xffffff;
  }
}

uint32_t geiger_get60minavg(void)
{
  uint32_t sum = 0;
  uint8_t readpos;
  uint8_t numvalid = 0;
  cli(); /* to make sure the history does not get modified while we count */
  readpos = geiger_historypos;
  for (uint8_t i = 0; i < (2 * 60); i++) {
    if (readpos == 0) {
      readpos = SIZEOFGEIGERHISTORY - 1;
    } else {
      readpos--;
    }
    if (geiger_valuehistory[readpos] != 0xffff) {
      numvalid++;
      sum += geiger_valuehistory[readpos];
    }
  }
  sei();
  if (numvalid > (2 * 30)) { /* Require at least 30 minutes of valid data */
    return (sum * 2 / numvalid);
  } else {
    return 0xffffff;
  }
}

uint16_t geiger_getticks(void)
{
  uint16_t res;
  cli();
  res = ticks;
  sei();
  return res;
}

void geiger_init(void)
{
  /* Mark all values in the history as 'invalid' because they haven't been collected yet */
  for (uint8_t i = 0; i < SIZEOFGEIGERHISTORY; i++) {
    geiger_valuehistory[i] = 0xffff;
  }
  /* the number of timer ticks in 30 seconds can be cleanly divided by 5. */
  ICR3H = (234375UL / 5) >> 8;
  ICR3L = (234375UL / 5) & 0xff;
  /* Set CTC-mode with the MAX taken from ICR3.
   * Select prescaler /1024, this results in 234375 timer-ticks in 30 seconds. */
  TCCR3A = 0x00;
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS32) | _BV(CS30);
  TIMSK3 |= _BV(ICIE3);
  TIFR3 |= _BV(ICF3);
  /* Enable pullups on PD0 (which is where the geiger counter is connected) */
  DDRD &= (uint8_t)~_BV(PD0);
  PORTD |= _BV(PD0);
  /* and enable pin generation on falling edge from that pin. */
  EICRA = (EICRA & 0x03) | _BV(ISC01);
  EIMSK |= _BV(INT0);
}
