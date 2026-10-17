| slice.s: qoadec.c's sample loop in 68000 assembly, for every Amiga: the
| C version spills its many values to the stack. The same samples as the
| C loop and the reference decoder (make check runs the C loop; the
| Amiga's tests/bench compares the two).
|
| void qa_run(WORD *out, ULONG bits, ULONG n, ULONG stride, const WORD *tab,
|             QoaLms *l)
|   up to 10 samples of one slice: bits holds their residuals from the top,
|   3 bits each; out steps stride bytes a sample; tab is the slice's
|   dequantisation row. l: history h[4] then weights w[4], LONGs; the
|   caller makes sure every weight stays inside 16 bits for these n
|   samples (each changes by at most 896 a sample), so the weights are
|   added and multiplied as WORDs here.
|
| d0-d3 weights, d4 prediction then the sample, d5 scratch then the
| residual, d6 the bits (rotated left 3 a sample: each residual lands in
| the low 3 bits in turn), d7 count; a0 out, a1 tab, a2 stride, a3-a6
| history (sign-extended WORDs), a6 the most recent.

	.text
	.globl	_qa_run

_qa_run:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	48(sp),a0		| out
	move.l	52(sp),d6		| bits
	move.l	56(sp),d7		| n
	move.l	60(sp),a2		| stride
	move.l	68(sp),a1		| l
	move.l	a1,-(sp)
	movem.l	16(a1),d0-d3		| w0-w3
	movem.l	(a1),a3-a6		| h0-h3 (all inside 16 bits)
	move.l	68(sp),a1		| tab (64 before the push)
	subq.w	#1,d7

loop:
	move.w	a3,d4			| prediction: sum of w * h, >> 13
	muls.w	d0,d4
	move.w	a4,d5
	muls.w	d1,d5
	add.l	d5,d4
	move.w	a5,d5
	muls.w	d2,d5
	add.l	d5,d4
	move.w	a6,d5
	muls.w	d3,d5
	add.l	d5,d4
	asr.l	#8,d4
	asr.l	#5,d4
	rol.l	#3,d6			| the residual, dequantised
	moveq	#7,d5
	and.w	d6,d5
	add.w	d5,d5
	move.w	(a1,d5.w),d5
	ext.l	d5
	add.l	d5,d4			| the sample, clamped to 16 bits
	cmp.l	#32767,d4
	bgt	high
	cmp.l	#-32768,d4
	blt	low
store:
	move.w	d4,(a0)
	add.l	a2,a0
	asr.w	#4,d5			| delta: the weights follow the history's signs
	cmpa.w	#0,a3
	blt	1f
	add.w	d5,d0
	bra	2f
1:	sub.w	d5,d0
2:	cmpa.w	#0,a4
	blt	1f
	add.w	d5,d1
	bra	2f
1:	sub.w	d5,d1
2:	cmpa.w	#0,a5
	blt	1f
	add.w	d5,d2
	bra	2f
1:	sub.w	d5,d2
2:	cmpa.w	#0,a6
	blt	1f
	add.w	d5,d3
	bra	2f
1:	sub.w	d5,d3
2:	move.l	a4,a3			| the history moves on
	move.l	a5,a4
	move.l	a6,a5
	move.w	d4,a6
	dbra	d7,loop

	move.l	(sp)+,a0		| l back
	ext.l	d0
	ext.l	d1
	ext.l	d2
	ext.l	d3
	movem.l	a3-a6,(a0)
	movem.l	d0-d3,16(a0)
	movem.l	(sp)+,d2-d7/a2-a6
	rts

high:	move.l	#32767,d4
	bra	store
low:	move.l	#-32768,d4
	bra	store
