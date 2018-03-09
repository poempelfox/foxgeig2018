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

#include "eeprom.h"
#include "adc.h"
#include "lufa/console.h"
#if 0
#include "rfm12.h"
#endif

/* The values last measured */
/* Battery level. Range 0-255, 255 = our supply voltage * 2 = 6,6V */
uint8_t batvolt = 0;
/* How often did we send a packet? */
uint32_t pktssent = 0;

/* This is just a fallback value, in case we cannot read this from EEPROM
 * on Boot */
uint8_t sensorid = 3; // 0 - 255 / 0xff

/* The frame we're preparing to send. */
static uint8_t frametosend[10];

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
 * Byte  3: Sensortype (=0xf7 for FoxTemp)
 * Byte  4: temperature MSB (raw value from SHT31)
 * Byte  5: temperature LSB
 * Byte  6: humidity MSB (raw value from SHT31)
 * Byte  7: humidity LSB
 * Byte  8: Battery voltage
 * Byte  9: CRC
 */
void prepareframe(void)
{
  frametosend[ 0] = 0xCC;
  frametosend[ 1] = sensorid;
  frametosend[ 2] = 6; /* 6 bytes of data follow (CRC not counted) */
  frametosend[ 3] = 0xf8; /* Sensor type: FoxGeig */
  /* frametosend[ 4] = (temp >> 8) & 0xff;
  frametosend[ 5] = (temp >> 0) & 0xff;
  frametosend[ 6] = (hum >> 8) & 0xff;
  frametosend[ 7] = (hum >> 0) & 0xff; */
  frametosend[ 8] = batvolt;
  frametosend[ 9] = calculatecrc(frametosend, 9);
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
#if 0
  _delay_ms(500); /* The RFM12 needs some time to start up */
  
  rfm12_initchip();
  rfm12_setsleep(1);
#endif
  
  /* Disable unused chip parts and ports */
  /* PE6 is the IRQ line from the RFM12. We don't use it. Make sure that pin
   * is tristated on our side (it won't float, the RFM12B pulls it) */
  PORTE &= (uint8_t)~_BV(PE6);
  DDRE &= (uint8_t)~_BV(PE6);

  /* All set up, enable interrupts and go. */
  /* Not yet - it'll crash after a short while, probably because of some interrupt from USB! */
  sei();

  DDRC |= (uint8_t)_BV(PC7); /* PC7 is the LED pin, drive it */
  PORTC |= (uint8_t)_BV(PC7);
  uint8_t ledstate = 1;
  while (1) {
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
    batvolt = adc_read() >> 2;
    adc_power(0);
    _delay_ms(1500);
    console_work();
  }
#if 0
  sht31_startmeas();

  uint16_t transmitinterval = 2; /* this is in multiples of the watchdog timer timeout (8S)! */
  uint8_t mlcnt = 0;
  while (1) { /* Main loop, we should never exit it. */
    mlcnt++;
    swserialo_printpgm_P(PSTR("."));
    if (mlcnt > transmitinterval) {
      rfm12_setsleep(0);  /* This mainly turns on the oscillator again */
      adc_power(1);
      adc_start();
      /* Fetch values from PREVIOUS measurement */
      struct sht31data hd;
      sht31_read(&hd);
      temp = 0xffff;
      hum = 0xffff;
      if (hd.valid) {
        temp = hd.temp;
        hum = hd.hum;
      }
      sht31_startmeas();
      /* read voltage from ADC */
      uint16_t adcval = adc_read();
      adc_power(0);
      /* we use a 1 M / 10 MOhm voltage divider, thus the voltage we see on
       * the ADC is 10/11 of the real voltage. */
      batvolt = adcval >> 2;
      prepareframe();
      swserialo_printpgm_P(PSTR(" TX "));
      rfm12_sendarray(frametosend, 10);
      pktssent++;
      /* Semirandom delay: the lowest bits from the ADC are mostly noise, so
       * we use that */
      transmitinterval = 3 + (adcval & 0x0001);
      rfm12_setsleep(1);
      mlcnt = 0;
    }
    wdt_reset();
    sleep_cpu(); /* Go to sleep until the watchdog timer wakes us */
    /* We should only reach this if we were just woken by the watchdog timer.
     * We need to re-enable the watchdog-interrupt-flag, else the next watchdog
     * -reset will not just trigger the interrupt, but be a full reset. */
    WDTCSR = _BV(WDCE) | _BV(WDE);
    WDTCSR = _BV(WDE) | _BV(WDIE) | _BV(WDP0) | _BV(WDP3);
  }
#endif
}
