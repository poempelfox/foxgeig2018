/* $Id: adc.c $
 * Functions for the analog digital converter
 */

#include <avr/io.h>
#include "adc.h"

void adc_init(void)
{
  /* Select prescaler for ADC, disable autotriggering, turn off ADC */
  ADCSRA = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
  /* Select reference voltage (AVCC with external cap) */
  ADMUX = _BV(REFS0);
  ADCSRB = _BV(MUX5);
  /* Disable digital input buffer on the port */
  DIDR2 |= _BV(ADC12D);
  /* Disable ADC for now (gets reenabled for the measurements */
  PRR0 |= _BV(PRADC);
}

void adc_power(uint8_t p)
{
  if (p) {
    /* Reenable ADC */
    PRR0 &= (uint8_t)~_BV(PRADC);
  } else {
    /* Send ADC to sleep */
    ADCSRA &= (uint8_t)~_BV(ADEN);
    PRR0 |= _BV(PRADC);
  }
}

void adc_select(uint8_t pin)
{
  uint8_t muxval = pin; /* That is correct for pin 0-7 */
  if (pin > 7) {
    muxval = 0x20 - 8 + pin;
  }
  /* Note: MUX is split over ADMUX (bits 4-0) and ADCSRB (bit 5) */
  ADMUX = (ADMUX & 0xe0) | (muxval & 0x1f);
  if (muxval & 0x20) {
    ADCSRB |= _BV(MUX5);
  } else {
    ADCSRB &= (uint8_t)~_BV(MUX5);
  }
}

/* Start ADC conversion */
void adc_start(void)
{
  ADCSRA |= _BV(ADEN) | _BV(ADSC);
}

uint16_t adc_read(void)
{
  /* Wait for ADC */
  while ((ADCSRA & _BV(ADSC))) { }
  /* Read result */
  uint16_t res = ADCL;
  res |= (ADCH << 8);
  return res;
}
