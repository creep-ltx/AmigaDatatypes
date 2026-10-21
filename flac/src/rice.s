| rice.s: flacdec.c's Rice loop for every 68k (plain 68000 code).
|
|   int fl_rice(FlacDec *d, LONG *r, ULONG cnt, ULONG k)    k 0-24
|
| CNT values with parameter K into r[], the bit reader taken from and put
| back into d (p, end, c, n at 72-84, as in flacdec.h). The same values as
| the C loop. It reads up to 12 bytes past d->end without looking: it stops
| once past d->end + 4, but a value may still fill twice after that check
| (4 bytes, then up to 4 more for its k bits). The caller's buffer has 16
| bytes of room there; such a frame fails its checks anyway.
| Returns 1, or 0 when it ran past d->end + 4.
|
| registers: a0 p, a1 r, a2 the zero-count table, a3 end + 4,
|            d0 the bits (from the top), d1 how many are valid,
|            d2 count, d3 k, d4 32 - k, d5-d7 work (d7 the value)

	.text
	.globl	_fl_rice
	.globl	_fl_clz8

D_P	= 72
D_END	= 76
D_C	= 80
D_N	= 84

| at least 25 valid bits in d0
	.macro	FILL
	cmp.w	#24,d1
	bgt.s	9f
8:	moveq	#0,d5
	move.b	(a0)+,d5
	moveq	#24,d6
	sub.w	d1,d6
	lsl.l	d6,d5
	or.l	d5,d0
	addq.w	#8,d1
	cmp.w	#24,d1
	ble.s	8b
9:
	.endm

_fl_rice:
	movem.l	d2-d7/a2-a3,-(sp)
	move.l	36(sp),a3		| d
	move.l	40(sp),a1		| r
	move.l	44(sp),d2		| cnt
	move.l	48(sp),d3		| k
	move.l	D_P(a3),a0
	move.l	D_C(a3),d0
	move.l	D_N(a3),d1
	move.l	a3,-(sp)		| d, for the end
	move.l	D_END(a3),a3
	addq.l	#4,a3
	lea	_fl_clz8,a2
	moveq	#32,d4
	sub.l	d3,d4
	tst.l	d2
	beq	.done
	subq.l	#1,d2			| dbra counts to -1; cnt is at most 65535

.loop:
	FILL
	moveq	#0,d7			| the zeros before the 1
	tst.l	d0
	bne.s	.nz
.zero:					| all the valid bits are zero
	add.l	d1,d7
	moveq	#0,d1
	cmp.l	a3,a0
	bhi	.bad
	FILL
	tst.l	d0
	beq.s	.zero
.nz:
	move.l	d0,d5
	rol.l	#8,d5
	moveq	#0,d6
	move.b	d5,d6
	move.b	(a2,d6.w),d6		| zeros in the top byte
	cmp.b	#8,d6
	bne.s	.one
	lsl.l	#8,d0			| a whole zero byte: the 1 is further on
	subq.w	#8,d1
	addq.l	#8,d7
	bra.s	.nz
.one:
	add.l	d6,d7
	addq.w	#1,d6			| the zeros and the 1 go
	lsl.l	d6,d0			| (a count of 32 gives 0, as wanted)
	sub.w	d6,d1
	tst.w	d3
	beq.s	.sign
	cmp.w	d3,d1
	bge.s	.have
	FILL
.have:
	move.l	d0,d5
	lsr.l	d4,d5			| the k low bits
	lsl.l	d3,d0
	sub.w	d3,d1
	lsl.l	d3,d7
	or.l	d5,d7
.sign:					| (u >> 1) ^ -(u & 1)
	lsr.l	#1,d7
	subx.l	d5,d5
	eor.l	d5,d7
	move.l	d7,(a1)+
	cmp.l	a3,a0
	bhi.s	.bad
	dbra	d2,.loop

.done:
	moveq	#1,d7
	bra.s	.out
.bad:
	moveq	#0,d7
.out:
	move.l	(sp)+,a3		| d
	move.l	a0,D_P(a3)
	move.l	d0,D_C(a3)
	ext.l	d1
	move.l	d1,D_N(a3)
	move.l	d7,d0
	movem.l	(sp)+,d2-d7/a2-a3
	rts
