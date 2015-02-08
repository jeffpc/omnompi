/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2013 (c) Joyent, Inc.  All rights reserved.
 * Copyright 2015 (c) Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 */

/*
 * A simple uart driver for the RPi.
 */
#include <sys/types.h>

#include "bcm2835_uart.h"
#include "tgt_support.h"

/*
 * The primary serial console that we end up using is the normal UART, not
 * the mini-uart that shares interrupts and registers with the SPI masters
 * as well.
 */

#define	UART_BASE		0x20201000
#define	UART_DR			0x0
#define	UART_FR			0x18
#define	UART_IBRD		0x24
#define	UART_FBRD		0x28
#define	UART_LCRH		0x2c
#define	UART_CR			0x30
#define	UART_ICR		0x44

#define	UART_FR_RXFE		0x10		/* RX fifo empty */
#define	UART_FR_TXFF		0x20		/* TX fifo full */

#define	UART_LCRH_FEN		0x00000010	/* fifo enable */
#define	UART_LCRH_WLEN_8	0x00000060	/* 8 bits */

#define	UART_CR_UARTEN		0x001		/* uart enable */
#define	UART_CR_TXE		0x100		/* TX enable */
#define	UART_CR_RXE		0x200		/* RX enable */


/*
 * All we care about are pins 14 and 15 for the UART.  Specifically, alt0
 * for GPIO14 is TXD0 and GPIO15 is RXD0. Those are controlled by FSEL1.
 */
#define	GPIO_BASE	0x20200000
#define	GPIO_FSEL1	0x4
#define	GPIO_PUD	0x94
#define	GPIO_PUDCLK0	0x98

#define GPIO_SEL_ALT0	0x4
#define	GPIO_UART_MASK	0xfffc0fff
#define	GPIO_UART_TX_SHIFT	12
#define	GPIO_UART_RX_SHIFT	15

#define	GPIO_PUD_DISABLE	0x0
#define	GPIO_PUDCLK_UART	0x0000c000

/*
 * A simple nop
 */
static void
bcm2835_uart_nop(void)
{
	__asm__ volatile("mov r0, r0\n" : : :);
}

void
bcm2835_uart_init(void)
{
	uint32_t v;
	int i;

	/* disable UART */
	arm_reg_write(UART_BASE + UART_CR, 0);

	/* TODO: Factor out the gpio bits */
	v = arm_reg_read(GPIO_BASE + GPIO_FSEL1);
	v &= GPIO_UART_MASK;
	v |= GPIO_SEL_ALT0 << GPIO_UART_RX_SHIFT;
	v |= GPIO_SEL_ALT0 << GPIO_UART_TX_SHIFT;
	arm_reg_write(GPIO_BASE + GPIO_FSEL1, v);

	arm_reg_write(GPIO_BASE + GPIO_PUD, GPIO_PUD_DISABLE);
	for (i = 0; i < 150; i++)
		bcm2835_uart_nop();
	arm_reg_write(GPIO_BASE + GPIO_PUDCLK0, GPIO_PUDCLK_UART);
	for (i = 0; i < 150; i++)
		bcm2835_uart_nop();
	arm_reg_write(GPIO_BASE + GPIO_PUDCLK0, 0);

	/* clear all interrupts */
	arm_reg_write(UART_BASE + UART_ICR, 0x7ff);

	/* set the baud rate */
	arm_reg_write(UART_BASE + UART_IBRD, 1);
	arm_reg_write(UART_BASE + UART_FBRD, 40);

	/* select 8-bit, enable FIFOs */
	arm_reg_write(UART_BASE + UART_LCRH, UART_LCRH_WLEN_8 | UART_LCRH_FEN);

	/* enable UART */
	arm_reg_write(UART_BASE + UART_CR, UART_CR_UARTEN | UART_CR_TXE |
	    UART_CR_RXE);
}

void
bcm2835_uart_putc(uint8_t c)
{
	while (arm_reg_read(UART_BASE + UART_FR) & UART_FR_TXFF)
		;
	arm_reg_write(UART_BASE + UART_DR, c & 0x7f);
	if (c == '\n')
		bcm2835_uart_putc('\r');
}

void
bcm2835_uart_putbyte(uint8_t c)
{
	while (arm_reg_read(UART_BASE + UART_FR) & UART_FR_TXFF)
		;
	arm_reg_write(UART_BASE + UART_DR, c);
}

uint8_t
bcm2835_uart_getc(void)
{
	while (arm_reg_read(UART_BASE + UART_FR) & UART_FR_RXFE)
		;
	return (arm_reg_read(UART_BASE + UART_DR) & 0x7f);
}

uint8_t
bcm2835_uart_getbyte(void)
{
	while (arm_reg_read(UART_BASE + UART_FR) & UART_FR_RXFE)
		;
	return (arm_reg_read(UART_BASE + UART_DR));
}

int
bcm2835_uart_isc(void)
{
	return ((arm_reg_read(UART_BASE + UART_FR) & UART_FR_RXFE) == 0);
}
