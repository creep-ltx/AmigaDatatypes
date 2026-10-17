| dither.s: quant.c's Floyd-Steinberg row (qu_map) in 68000 assembly,
| for every Amiga: the C version spills its many values to the stack.
| Gives the same palette numbers as the C loop, byte for byte.
|
| void qu_dither(const ULONG *px, UBYTE *out, ULONG n, WORD *cur, WORD *nx,
|                Quant *q, const UBYTE *cl)
|   px: the row, red in the top byte. cur: this row's incoming errors,
|   three WORDs a pixel. nx: the next row's cells from the one down left of
|   the first pixel (its first two cells zeroed by the caller). q: Quant,
|   pal (4 bytes an entry) at 0, inv at 1024, invok at 5120. cl: the clamp
|   table at its 0 (cl[-512..767]).
|
| d1 d2 d3 red green blue, d4 d5 d6 the error going right (7/16), d0 d7
| scratch; a0 px, a1 out, a2 cur, a3 nx, a4 cl, a5 q, a6 the end of out.

	.text
	.globl	_qu_dither

INV	= 1024
INVOK	= 5120

| one channel's error: e (in \c) = \c - pal byte \pb; 3/16 down left,
| 5/16 down (added), 1/16 down right (set), 7/16 right (into \er)
	.macro	SPREAD	c, pb, er, off
	moveq	#0,d0
	move.b	\pb(a5,d7.w),d0
	sub.w	d0,\c
	move.w	\c,d0
	add.w	d0,d0
	add.w	\c,d0			| 3e
	move.w	d0,\er
	asr.w	#4,d0
	add.w	d0,\off(a3)
	add.w	\c,\er
	add.w	\c,\er			| 5e
	move.w	\er,d0
	asr.w	#4,d0
	add.w	d0,\off+6(a3)
	move.w	\c,d0
	asr.w	#4,d0
	move.w	d0,\off+12(a3)
	add.w	\c,\er
	add.w	\c,\er			| 7e
	asr.w	#4,\er
	.endm

_qu_dither:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	48(sp),a0
	move.l	52(sp),a1
	move.l	a1,a6
	add.l	56(sp),a6
	move.l	60(sp),a2
	move.l	64(sp),a3
	move.l	68(sp),a5
	move.l	72(sp),a4
	moveq	#0,d4
	moveq	#0,d5
	moveq	#0,d6
	cmp.l	a6,a1
	beq	done

pixel:	move.l	(a0)+,d0
	move.l	d0,d1			| red
	clr.w	d1
	swap	d1
	lsr.w	#8,d1
	add.w	(a2)+,d1
	add.w	d4,d1
	move.b	0(a4,d1.w),d1
	and.w	#255,d1
	move.l	d0,d2			| green
	swap	d2
	and.w	#255,d2
	add.w	(a2)+,d2
	add.w	d5,d2
	move.b	0(a4,d2.w),d2
	and.w	#255,d2
	move.w	d0,d3			| blue
	lsr.w	#8,d3
	add.w	(a2)+,d3
	add.w	d6,d3
	move.b	0(a4,d3.w),d3
	and.w	#255,d3
	move.w	d1,d7			| k = r>>4 <<8 | g & f0 | b>>4
	lsl.w	#4,d7
	and.w	#0x0f00,d7
	move.w	d2,d0
	and.w	#0xf0,d0
	or.w	d0,d7
	move.w	d3,d0
	lsr.w	#4,d0
	or.w	d0,d7
	move.w	d7,d0			| known yet? invok bit k&7 of byte k>>3
	lsr.w	#3,d0
	add.w	#INVOK,d0
	btst	d7,0(a5,d0.w)
	beq	miss
hit:	add.w	#INV,d7
	move.b	0(a5,d7.w),d0
	move.b	d0,(a1)+
	moveq	#0,d7
	move.b	d0,d7
	add.w	d7,d7
	add.w	d7,d7			| the palette entry's offset
	SPREAD	d1, 0, d4, 0
	SPREAD	d2, 1, d5, 2
	SPREAD	d3, 2, d6, 4
	addq.l	#6,a3
	cmp.l	a6,a1
	bne	pixel
done:	movem.l	(sp)+,d2-d7/a2-a6
	rts

| not yet known: quant.c's qu_miss(q, k) finds and keeps it
miss:	movem.l	d1/a0/a1,-(sp)
	moveq	#0,d0
	move.w	d7,d0
	move.l	d0,-(sp)
	move.l	a5,-(sp)
	jsr	_qu_miss
	addq.l	#8,sp
	movem.l	(sp)+,d1/a0/a1
	bra	hit
