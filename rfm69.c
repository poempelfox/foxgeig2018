/* $Id: rfm69.c $
 * Functions for communication with RF12 module
 *
 * This code was heavily copy+pasted from RFMxx.cpp in the FHEM LaCrosseIT+
 * Jeenode Firmware Code.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>
#include "rfm69.h"

/* Pin mappings:
 *  SS     PB4 (? not verified, the schematics do not tell!)
 *  MOSI   PB2
 *  MISO   PB3
 *  SCK    PB1
 */
#define RFMDDR   DDRB
#define RFMPIN   PINB
#define RFMPORT  PORTB

#define RFMPIN_SS    PB4
#define RFMPIN_MOSI  PB2
#define RFMPIN_MISO  PB3
#define RFMPIN_SCK   PB1
#define RFMPIN_OURSS PB0

#define RFM_FREQUENCY 868300ul
#define RFM_DATARATE 17241.0

#define PAYLOADSIZE 64

ISR(INT6_vect)
{
  /* Nothing to do here. */
}

ISR(SPI_STC_vect)
{
  /* Nothing to do here. */
}

/* Note: Internal use only. Does not set the SS pin, the calling function
 * has to do that! */
static uint8_t rfm69_spi8(uint8_t value) {
  SPDR = value;
  /* busy-wait for transmission. this loop spins 4 usec with a 2 MHz SPI clock */
  while (!(SPSR & _BV(SPIF))) { }

  return SPDR;
}

uint16_t rfm69_spi16(uint16_t value) {
  RFMPORT &= (uint8_t)~_BV(RFMPIN_SS);
  uint16_t reply = rfm69_spi8(value >> 8) << 8;
  reply |= rfm69_spi8(value & 0xff);
  RFMPORT |= _BV(RFMPIN_SS);
  
  return reply;
}

static uint8_t rfm69_readreg(uint8_t reg) {
  return rfm69_spi16(((uint16_t)(reg & 0x7f) << 8) | 0x00) & 0xff;
}

static void rfm69_writereg(uint8_t reg, uint8_t val) {
  rfm69_spi16(((uint16_t)(reg | 0x80) << 8) | val);
}

void rfm69_clearfifo(void) {
  /* There is no need for reading / ORing the register here because all
   * bits except the FiFoOverrun-bit we set to clear the FIFO are read-only */
  rfm69_writereg(0x28, (1 << 4)); /* RegIrqFlags2 */
}

void rfm69_settransmitter(uint8_t e) {
  if (e) {
    /* RegOpMode => TRANSMIT */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | (0x03 << 2));
  } else {
    /* RegOpMode => STANDBY */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | (0x01 << 2));
  }
}

void rfm69_setsleep(uint8_t s) {
  if (s) {
    /* RegOpMode => SLEEP */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | (0x00 << 2));
  } else {
    /* RegOpMode => STANDBY */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | (0x01 << 2));
  }
}

void rfm69_sendarray(uint8_t * data, uint8_t length) {
  rfm69_clearfifo(); /* Clear the FIFO */
  /* Now fill the FIFO. We manually set SS and use spi8 because this
   * is the only "register" that is larger than 8 bits. */
  RFMPORT &= (uint8_t)~_BV(RFMPIN_SS);
  rfm69_spi8(0x00); /* Select RegFifo */
  for (int i = 0; i < length; i++) {
    rfm69_spi8(data[i]);
  }
  RFMPORT |= _BV(RFMPIN_SS);
  /* FIFO has been filled. Tell the RFM69 to send by just turning on the transmitter. */
  rfm69_settransmitter(1);
  /* Wait for transmission to finish, visible in RegIrqFlags2. */
  while (!(rfm69_readreg(0x28) & 0x08)) {
    /* Yes, this has the potential to hang indefinitely if something is wrong
     * with the RFM69, but then we're useless anyways, so we'll just let the
     * watchdog timer reset us. */
  }
  rfm69_settransmitter(0);
}

void rfm69_initport(void) {
  /* Configure Pins for output / input */
  RFMDDR |= _BV(RFMPIN_MOSI);
  RFMDDR &= (uint8_t)~_BV(RFMPIN_MISO);
  RFMDDR |= _BV(RFMPIN_SCK);
  RFMDDR |= _BV(RFMPIN_SS);
  /* We do not really use the SS pin of the AVR, but if it were pulled low,
   * the AVR would drop out of master mode! To avoid this, we configure it as
   * output pin. */
  RFMDDR |= _BV(RFMPIN_OURSS);

  RFMPORT |= _BV(RFMPIN_SS);
  
  /* Enable hardware SPI, no need to manually do it.
   * set master mode with rate clk/4 = 2 MHz (maximum of RFM69 is unknown) */
  SPCR = _BV(SPE) | _BV(MSTR);
  SPSR = 0x00; /* To set SPI2X to 0, rest is read-only anyways */
}

void rfm69_initchip(void) {
  /* RegOpMode -> standby. The jeenode sketch also set sequencer to forced,
   * but that seems pointless, automatic should work too. */
  rfm69_writereg(0x01, 0x00 | (0x03 << 2));
  /* RegDataModul -> PacketMode, FSK, Shaping 0 */
  rfm69_writereg(0x02, 0x18);
  /* RegFDevMsb / RegFDevLsb -> 0x05C3 (90 kHz). */
  rfm69_writereg(0x05, 0x05);
  rfm69_writereg(0x06, 0xC3);
  /* RegPaLevel -> Pa0=1 Pa1=0 Pa2=0 Outputpower=31 -> 13 dbM */
  rfm69_writereg(0x11, 0x9F);
  /* RegOcp -> OCP off */
  rfm69_writereg(0x13, 0x00);
  /* We do not care about the receiver registers the jeelink sketch would set now */
  /* RegDioMapping2 -> disable clkout (but thats the default anyways) */
  rfm69_writereg(0x26, 0x07);
  rfm69_clearfifo(); /* this is reg 0x28 */
  /* RegRssiThresh -> 220 */
  rfm69_writereg(0x29, 220);
  /* RegSyncConfig -> SyncOn FiFoFillAuto SyncSize=2 SyncTol=0 */
  rfm69_writereg(0x2E, 0x88);
  /* RegSyncValue1/2 (3-8 exist too but we only use 2 so do not need to set them) */
  rfm69_writereg(0x2F, 0x2D);
  rfm69_writereg(0x30, 0xD4);
  /* RegPacketConfig1 -> CrcOn=0 */
  rfm69_writereg(0x37, 0x00);
  /* RegPayloadLength -> PAYLOADSIZE (64) - FIXME? I don't think this makes sense */
  rfm69_writereg(0x38, PAYLOADSIZE);
  /* RegFifoThreshold -> TxStartCond=1 value=0x0f */
  rfm69_writereg(0x3C, 0x8F);
  /* RegPacketConfig2 -> AesOn=0 and AutoRxRestart=1 even if we do not care about RX */
  rfm69_writereg(0x3D, 0x02);
  /* RegTestDagc -> improvedlowbeta0 - I haven't got the faintest... */
  rfm69_writereg(0x6F, 0x30);
  /* Set Frequency */
  /* FIXME I do not think this calculation is correct */
  uint32_t freq = RFM_FREQUENCY * (32000000UL / ((uint32_t)2 << 1));
  rfm69_writereg(0x07, (freq >> 16) & 0xff);
  rfm69_writereg(0x08, (freq >>  8) & 0xff);
  rfm69_writereg(0x09, (freq >>  0) & 0xff);
  /* Set Datarate - the whole floating point mess seems to be optimized out 
   * by gcc at compile time, thank god. */
  uint16_t dr = (uint16_t)round(32000000.0 / RFM_DATARATE);
  rfm69_writereg(0x03, (dr >> 8));
  rfm69_writereg(0x04, (dr & 0xff));

  rfm69_clearfifo();
}
