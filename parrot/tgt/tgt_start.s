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

#define	_ENTRY(x, sec)			\
	.section	sec;		\
	.align		4;		\
	.globl		x;		\
	.type		x, %function;	\
x:

#define ENTRY(x)	_ENTRY(x, .text)

#define SET_SIZE(x)			\
	.size		x, (.-x)

/*
 * Copyright 2013 (c) Joyent, Inc. All rights reserved.
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 */

	.data
	.comm	stack, 4096, 32

	_ENTRY(_start, .text.init)
	ldr	sp, =stack
	add	sp, #4096

	bl	main
	SET_SIZE(_start)
