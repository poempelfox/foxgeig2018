/* $Id: main.c $
 * main for foxgeig2018 (Foxis Geigercounter)
 * (C) Michael "Fox" Meier 2018
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "adc.h"
#include "eeprom.h"
#include "geiger.h"
#include "rfm69.h"
#include "lufa/console.h"

/* The values last measured */
/* Battery level. Range 0-1023, 1023 = our supply voltage * 2 = 6,6V
 * We send this shifted to the right by two as uint8_t (because the lower two
 * bits are just noise anyways) */
uint16_t batvolt = 0;
/* How often did we send a packet? */
uint32_t pktssent = 0;
/* Geigercounter values */
uint32_t geigcntavg1min = 0;
uint32_t geigcntavg60min = 0;

/* This is just a fallback value, in case we cannot read this from EEPROM
 * on Boot */
uint8_t sensorid = 3; // 0 - 255 / 0xff

/* The frame we're preparing to send. */
static uint8_t frametosend[12];

/* We need to disable the watchdog very early, because it stays active
 * after a reset with a timeout of only 15 ms. */
void dwdtonreset(void) __attribute__((naked)) __attribute__((section(".init3")));
void dwdtonreset(void) {
  MCUSR = 0;
  wdt_disable();
}

static uint8_t calculatecrc(uint8_t * data, uint8_t len)
{
  uint8_t i, j;
  uint8_t res = 0;
  for (j = 0; j < len; j++) {
    uint8_t val = data[j];
    for (i = 0; i < 8; i++) {
      uint8_t tmp = (uint8_t)((res ^ val) & 0x80);
      res <<= 1;
      if (0 != tmp) {
        res ^= 0x31;
      }
      val <<= 1;
    }
  }
  return res;
}

/* Fill the frame to send with out collected data and a CRC.
 * The protocol we use is that of a "CustomSensor" from the
 * FHEM LaCrosseItPlusReader sketch for the Jeelink.
 * So you'll just have to enable the support for CustomSensor in that sketch
 * and flash it onto a JeeNode and voila, you have your receiver.
 *
 * Byte  0: Startbyte (=0xCC)
 * Byte  1: Sensor-ID (0 - 255/0xff)
 * Byte  2: Number of data bytes that follow (6)
 * Byte  3: Sensortype (=0xf9 for FoxGeig)
 * Byte  4: 
 * Byte  5: 
 * Byte  6: 
 * Byte  7: 
 * Byte  8: Battery voltage (0-255, 255 = 6.6V)
 * Byte  9: CRC
 */
void prepareframe(void)
{
  frametosend[ 0] = 0xCC;
  frametosend[ 1] = sensorid;
  frametosend[ 2] = 8; /* 8 bytes of data follow (CRC not counted) */
  frametosend[ 3] = 0xf9; /* Sensor type: FoxGeig */
  frametosend[ 4] = (geigcntavg1min >> 16) & 0xff;
  frametosend[ 5] = (geigcntavg1min >>  8) & 0xff;
  frametosend[ 6] = (geigcntavg1min >>  0) & 0xff;
  frametosend[ 7] = (geigcntavg60min >> 16) & 0xff;
  frametosend[ 8] = (geigcntavg60min >>  8) & 0xff;
  frametosend[ 9] = (geigcntavg60min >>  0) & 0xff;
  frametosend[10] = batvolt >> 2;
  frametosend[11] = calculatecrc(frametosend, 11);
}

void loadsettingsfromeeprom(void)
{
  uint8_t e1 = eeprom_read_byte(&ee_sensorid);
  uint8_t e2 = eeprom_read_byte(&ee_invsensorid);
  if ((e1 ^ 0xff) == e2) { /* OK, the 'checksum' matches. Use this as our ID */
    sensorid = e1;
  }
}

int main(void)
{
  /* Initialize stuff */
  
  loadsettingsfromeeprom();
  
  console_init();
  adc_init();
  geiger_init();
  rfm69_initport();
  /* The RFM69 needs some time to start up (5 ms according to data sheet, we wait 10 to be sure) */
  _delay_ms(10);
  rfm69_initchip();
  rfm69_setsleep(1);
  
  /* Enable watchdog timer with a timeout of 8 seconds */
  wdt_enable(WDTO_8S); /* Longest possible on ATmega328P */
  
  /* Disable unused chip parts and ports */
  /* PE6 is the IRQ line from the RFM69. We don't use it. Make sure that pin
   * is tristated on our side (it won't float, the RFM69 pulls it) */
  PORTE &= (uint8_t)~_BV(PE6);
  DDRE &= (uint8_t)~_BV(PE6);

  /* Prepare sleep mode */
  /* SLEEP_MODE_IDLE is the only sleepmode we can safely use. */
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  /* All set up, enable interrupts and go. */
  sei();

  DDRC |= (uint8_t)_BV(PC7); /* PC7 is the LED pin, drive it */
  PORTC |= (uint8_t)_BV(PC7);
  uint8_t ledstate = 1;

  /* Nur ein ping vassili */
  rfm69_setsleep(0);  /* This mainly turns on the oscillator again */
  prepareframe();
  console_printpgm_P(PSTR(" TX "));
  rfm69_sendarray(frametosend, 12);
  pktssent++;
  rfm69_setsleep(1);

  while (1) {
    wdt_reset();
    if (ledstate == 0) {
      ledstate = 1;
      PORTC |= (uint8_t)_BV(PC7);
    } else {
      ledstate = 0;
      PORTC &= (uint8_t)~_BV(PC7);
    }
    adc_power(1);
    adc_select(12);
    adc_start();
    geigcntavg1min = geiger_get1minavg();
    geigcntavg60min = geiger_get60minavg();
    batvolt = adc_read();
    adc_power(0);
    console_work();
    wdt_reset();
    sleep_cpu(); /* Go to sleep until the next IRQ arrives */
  }
}
