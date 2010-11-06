/*
 * Copyright 2010 Michael Ossmann
 *
 * This file is part of Project Ubertooth.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ubertooth.h"

/* delay a number of seconds while on internal oscillator (4 MHz) */
void wait(u8 seconds)
{
	u32 i = 400000 * seconds;
	while (--i);
}

/*
 * This should be called very early by every firmware in order to ensure safe
 * operating conditions for the CC2400.
 */
void gpio_init()
{
	/* 
	 * Set all pins for GPIO.  This shouldn't be necessary after a reset, but
	 * we might get called at other times.
	 */
	PINSEL0 = 0;
	PINSEL1 = 0;
	PINSEL2 = 0;
	PINSEL3 = 0;
	PINSEL4 = 0;
	PINSEL7 = 0;
	PINSEL8 = 0;
	PINSEL9 = 0;
	PINSEL10 = 0;

	/* set certain pins as outputs, all others inputs */
	FIO0DIR = PIN_USRLED;
	FIO1DIR = (PIN_CC3V3 | PIN_RX | PIN_TX | PIN_CSN |
			PIN_SCLK | PIN_MOSI | PIN_CC1V8 | PIN_BTGR);
	FIO2DIR = 0;
	FIO3DIR = 0;
	FIO4DIR = (PIN_RXLED | PIN_TXLED);

	/* set all outputs low */
	FIO0PIN = 0;
	FIO1PIN = 0;
	FIO2PIN = 0;
	FIO3PIN = 0;
	FIO4PIN = 0;
}

/*
 * Every application that uses both USB and the CC2400 should start with this.
 */
void ubertooth_init()
{
	gpio_init();
	//FIXME cc2400_init();
	//FIXME clock setup
	//FIXME usb setup
}

void atest_init()
{
	/*
	 * ADC can optionally be configured for ATEST1 and ATEST2, but for now we
	 * set them as floating inputs.
	 */

	/* P0.25 is ATEST1, P0.26 is ATEST2 */
	PINSEL1 &= ~((0x3 << 20) | (0x3 << 18)); // set as GPIO
	FIO0DIR &= ~((0x3 << 25)); // set as input
	PINMODE1 |= ((0x3 << 19)); // no pull-up/pull-down
	PINMODE1 &= ~((0x3 << 18)); // no pull-up/pull-down
}

void cc2400_init()
{
	atest_init();

	/* activate 1V8 supply for CC2400 */
	CC1V8_SET;
	wait(1); //FIXME only need to wait 50us

	/* CSN (slave select) is active low */
	CSN_SET;

	/* activate 3V3 supply for CC2400 IO */
	CC3V3_SET;
}

inline static void spi_delay()
{
	u32 i = 10;
	while (--i);
}

/*
 * This is a single SPI transaction of variable length, usually 8 or 24 bits.
 * The CC2400 also supports longer transactions (e.g. for the FIFO), but we
 * haven't implemented anything longer than 32 bits.
 *
 * We're bit-banging because:
 *
 * 1. We're using one SPI peripheral for the CC2400's unbuffered data
 *    interace.
 * 2. We're saving the second SPI peripheral for an expansion port.
 * 3. The CC2400 needs CSN held low for the entire transaction which the
 *    LPC17xx SPI peripheral won't do without some workaround anyway.
 */
u32 cc2400_spi(u8 len, u32 data)
{
	u32 msb = 1 << (len - 1);

	/* start transaction by dropping CSN */
	CSN_CLR;

	while (len--) {
		if (data & msb)
			MOSI_SET;
		else
			MOSI_CLR;
		data <<= 1;

		spi_delay();

		SCLK_SET;
		if (MISO)
			data |= 1;

		spi_delay();

		SCLK_CLR;
	}

	spi_delay();

	/* end transaction by raising CSN */
	CSN_SET;

	return data;
}

/* read the value from a register */
u16 cc2400_get(u8 reg)
{
	int i;
	u32 in;
	u16 val;

	u32 out = (reg | 0x80) << 16;
	in = cc2400_spi(24, out);
	val = in & 0xFFFF;

	return val;
}

/* write a value to a register */
void cc2400_set(u8 reg, u32 val)
{
	u32 out = (reg << 16) | val;
	cc2400_spi(24, out);
}

/* get the status */
u8 cc2400_status()
{
	return cc2400_spi(8, 0);
}

/* strobe register, return status */
u8 cc2400_strobe(u8 reg)
{
	return cc2400_spi(8, reg);
}

void cc2400_reset()
{
	//FIXME handle clock
	cc2400_set(MAIN, 0x0000);
	while (cc2400_get(MAIN) != 0x0000);
	cc2400_set(MAIN, 0x8000);
	while (cc2400_get(MAIN) != 0x8000);
}

//FIXME clock setup
//FIXME ssp
//FIXME tx/rx