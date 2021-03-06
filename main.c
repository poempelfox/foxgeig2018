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

/* Fill the frame to send with our collected data and a CRC.
 * The protocol we use is that of a "CustomSensor" from the
 * FHEM LaCrosseItPlusReader sketch for the Jeelink.
 * So you'll just have to enable the support for CustomSensor in that sketch
 * and flash it onto a JeeNode and voila, you have your receiver.
 *
 * Byte  0: Startbyte (=0xCC)
 * Byte  1: Sensor-ID (0 - 255/0xff)
 * Byte  2: Number of data bytes that follow (6)
 * Byte  3: Sensortype (=0xf9 for FoxGeig)
 * Byte  4: CountsPerMinute for last minute, MSB
 * Byte  5: CountsPerMinute for last minute,
 * Byte  6: CountsPerMinute for last minute, LSB
 * Byte  7: CountsPerMinute for last 60 minutes, MSB
 * Byte  8: CountsPerMinute for last 60 minutes,
 * Byte  9: CountsPerMinute for last 60 minutes, LSB
 * Byte 10: Battery voltage (0-255, 255 = 6.6V)
 * Byte 11: CRC
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
  uint16_t lastts = 0xf000; /* This forces an update immediately after start */
  uint16_t curts;
  uint16_t tsdiff;
  uint8_t transmitinterval = 5; /* Transmitinterval in ticks of 6s, so 5 = 30s */
  
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
  /* Turn off unused stuff on the AVR via PRR registers */
  /* We don't use TWI/I2C and Timer0/1 */
  PRR0 |= _BV(PRTWI) | _BV(PRTIM0) | _BV(PRTIM1);
  /* We don't use Timer4 and the USART. There seems to be a bug in
   * avr-libc on Ubuntu 16.04, it doesn't define PRTIM4 but instead
   * PRTIM2 for a nonexistant Timer2. Therefore we cannot use the
   * macro and hardcode the correct value. */
  PRR1 |= _BV(/* PRTIM4 */ 4) | _BV(PRUSART1);

  /* Prepare sleep mode */
  /* SLEEP_MODE_IDLE is the only sleepmode we can safely use. */
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();

  /* All set up, enable interrupts and go. */
  sei();

  DDRC |= (uint8_t)_BV(PC7); /* PC7 is the LED pin, drive it */
  PORTC &= (uint8_t)~_BV(PC7); /* Turn it off */

  while (1) {
    wdt_reset();
    curts = geiger_getticks();
    tsdiff = curts - lastts;
    if (tsdiff >= transmitinterval) {
      /* Time to update values and send */
      adc_power(1);
      adc_select(12);
      adc_start();
      geigcntavg1min = geiger_get1minavg();
      geigcntavg60min = geiger_get60minavg();
      batvolt = adc_read();
      adc_power(0);
      /* SEND */
      rfm69_setsleep(0);  /* This mainly turns on the oscillator again */
      prepareframe();
      console_printpgm_P(PSTR(" TX "));
      rfm69_sendarray(frametosend, 12);
      rfm69_setsleep(1);
      pktssent++;
      lastts = curts; /* Remember when we last sent a packet */
      /* We use the lower two bits of batvolt as the random noise that it is */
      uint8_t rnd = batvolt & 3;
      if (rnd == 3) {
        transmitinterval = 6;
      } else if (rnd == 0) {
        transmitinterval = 4;
      } else { /* 1 or 2 */
        transmitinterval = 5;
      }
    }
    console_work();
    if (!console_isusbconfigured()) {
      /* Don't go to sleep when USB is configured. Because then there is no
       * lack of power, and more importantly, we want the console to feel
       * "snappy" and we can't get that if we sleep for 6 seconds. */
      wdt_reset(); /* Buy us 8 seconds time because the next IRQ might only arrive in 6 seconds */
      sleep_cpu(); /* Go to sleep until the next IRQ arrives */
    }
  }
}
