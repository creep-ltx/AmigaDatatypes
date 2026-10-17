| fast020.s: qoidec's hot loop for the 68020 and later. The ops of the
| common pixels in registers, small enough for the 68020's 256-byte
| instruction cache. Called by qoidec.c's decode() only when qd_use020 is
| set (a 68020 or better), and only for the RGBA and RGB layouts.
|
| ULONG qd_fast020(QoiDec *d, UBYTE **out, ULONG n, int rgb)
|   Decodes ops while there are pixels to give (n) and the input pointer
|   is below d->lim; gives back the pixels still wanted. A run that goes
|   past n leaves the rest in d->run. The QoiDec offsets are qoidec.h's;
|   the table offsets are struct QoiTab's (qoidec.c). The place in index
|   (h) is kept times 4 in the low byte of d3, so it wraps by itself.
|
| d0 op, d1 d7 scratch, d2 px (red in the top byte), d3 h*4, d4 n,
| d5 $7f7f7f7f, d6 $80808080, a0 input, a1 output, a2 lim, a3 index,
| a4 tables, a5 the ANDed alphas, a6 d.

	.text
	.globl	_qd_fast020

PX	= 0
RUN	= 4
ALPHA	= 8
P	= 12
LIM	= 16
H	= 20
INDEX	= 24

TDIFF	= 0
TDIFFH	= 256
TLGB	= 320
TLR	= 4416
TLHGB	= 8512
TLHR	= 9536
TH3	= 10560
TH5	= 10816
TH7	= 11072
TH11	= 11328

| px += delta, each byte on its own: ((px & 7f..) + (delta & 7f..)) ^ ((px ^ delta) & 80..)
	.macro	SWARADD	delta
	move.l	d2,d7
	eor.l	\delta,d7
	and.l	d6,d7
	and.l	d5,d2
	and.l	d5,\delta
	add.l	\delta,d2
	eor.l	d7,d2
	.endm

| QOI_OP_LUMA (op in d0): two table lookups, one byte-wise add
	.macro	LUMA
	and.w	#63,d0
	lsl.w	#4,d0
	moveq	#0,d1
	move.b	(a0)+,d1
	move.w	d1,d7
	and.w	#15,d7
	or.w	d0,d7			| op bits and low half
	lsr.w	#4,d1
	or.w	d0,d1			| op bits and high half
	move.b	(TLHGB,a4,d7.w),d0
	add.b	(TLHR,a4,d1.w),d0
	add.b	d0,d3
	move.l	(TLGB,a4,d7.w*4),d0
	or.l	(TLR,a4,d1.w*4),d0
	SWARADD	d0
	.endm

_qd_fast020:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	48(sp),a6		| d
	move.l	52(sp),a1
	move.l	(a1),a1			| *out
	move.l	56(sp),d4		| n
	tst.l	60(sp)
	bne	rgbloop
	bsr	enter

| ---- RGBA: 4 bytes a pixel -------------------------------------------
aloop:	cmp.l	a2,a0
	bcc	leave
	moveq	#0,d0
	move.b	(a0)+,d0
	move.w	(atab,pc,d0.w*2),d1
	jmp	(atab,pc,d1.w)
a_full:
	bsr	full
	bra.s	a_put
a_index:
	move.l	(INDEX,a6,d0.w*4),d2
	beq.s	1f
	lsl.b	#2,d0
	move.b	d0,d3
	bra.s	2f
1:	moveq	#0,d3
2:	move.l	a5,d1
	and.l	d2,d1
	move.l	d1,a5
	bra.s	a_put
a_diff:
	and.w	#63,d0
	move.l	(TDIFF,a4,d0.w*4),d1
	add.b	(TDIFFH,a4,d0.w),d3
	SWARADD	d1
	bra.s	a_put
a_luma:
	LUMA
a_put:	move.l	d2,(INDEX,a6,d3.w)
	move.l	d2,(a1)+
	subq.l	#1,d4
	bne	aloop
	bra	leave
a_run:	bsr	runlen			| d0 = pixels to give now - 1
1:	move.l	d2,(a1)+
	dbra	d0,1b
	tst.l	d4
	bne	aloop
	bra	leave


| ---- RGB: 3 bytes a pixel (a long written, the 4th byte overwritten by
| the next pixel; the last pixel of a call as three bytes) ------------
rgbloop:
	bsr	enter
bloop:	cmp.l	a2,a0
	bcc	leave
	moveq	#0,d0
	move.b	(a0)+,d0
	move.w	(btab,pc,d0.w*2),d1
	jmp	(btab,pc,d1.w)
b_full:
	bsr	full
	bra.s	b_put
b_index:
	move.l	(INDEX,a6,d0.w*4),d2
	beq.s	1f
	lsl.b	#2,d0
	move.b	d0,d3
	bra.s	2f
1:	moveq	#0,d3
2:	move.l	a5,d1
	and.l	d2,d1
	move.l	d1,a5
	bra.s	b_put
b_diff:
	and.w	#63,d0
	move.l	(TDIFF,a4,d0.w*4),d1
	add.b	(TDIFFH,a4,d0.w),d3
	SWARADD	d1
	bra.s	b_put
b_luma:
	LUMA
b_put:	move.l	d2,(INDEX,a6,d3.w)
	subq.l	#1,d4
	beq.s	1f
	move.l	d2,(a1)
	addq.l	#3,a1
	bra	bloop
1:	bsr.s	put3
	bra	leave
b_run:	bsr	runlen
	bra.s	2f
1:	move.l	d2,(a1)
	addq.l	#3,a1
2:	dbra	d0,1b
	bsr.s	put3			| the run's last pixel: three bytes
	tst.l	d4
	bne	bloop
	bra	leave

put3:	move.l	d2,d1
	rol.l	#8,d1
	move.b	d1,(a1)+
	rol.l	#8,d1
	move.b	d1,(a1)+
	rol.l	#8,d1
	move.b	d1,(a1)+
	rts

| ---- shared -----------------------------------------------------------

| QOI_OP_RUN (op in d0): stores px in index, takes min(run, n) off n and
| leaves the rest in d->run; d0 = pixels to give now - 1 (for dbra)
runlen:	and.w	#63,d0
	addq.l	#1,d0
	move.l	d2,(INDEX,a6,d3.w)
	cmp.l	d4,d0
	bls.s	1f
	move.l	d0,d1
	sub.l	d4,d1
	move.l	d1,RUN(a6)
	move.l	d4,d0
1:	sub.l	d0,d4
	subq.l	#1,d0
	rts

| QOI_OP_RGB (0xfe) and QOI_OP_RGBA (0xff): the colour from the bytes,
| its place from the four tables
full:	moveq	#0,d1
	moveq	#0,d3
	move.b	(a0)+,d1
	move.b	(TH3,a4,d1.w),d3
	move.b	d1,d7
	lsl.l	#8,d7
	move.b	(a0)+,d1
	add.b	(TH5,a4,d1.w),d3
	move.b	d1,d7
	lsl.l	#8,d7
	move.b	(a0)+,d1
	add.b	(TH7,a4,d1.w),d3
	move.b	d1,d7
	lsl.l	#8,d7
	cmp.b	#0xfe,d0
	beq.s	1f
	move.b	(a0)+,d2		| RGBA: a new alpha
	move.l	a5,d0
	and.l	d2,d0
	move.l	d0,a5
1:	moveq	#0,d1
	move.b	d2,d1			| the alpha (old or new)
	add.b	(TH11,a4,d1.w),d3
	move.b	d1,d7
	move.l	d7,d2
	rts

| load the state from d; the return address of this bsr stays on the stack
| below the saved registers until leave
enter:	move.l	(sp)+,d1
	move.l	PX(a6),d2
	move.l	H(a6),d3
	and.l	#0xff,d3
	move.l	#0x7f7f7f7f,d5
	move.l	#0x80808080,d6
	move.l	P(a6),a0
	move.l	LIM(a6),a2
	lea	(INDEX,a6),a3
	lea	_qd_tab,a4
	move.l	ALPHA(a6),a5
	move.l	d1,-(sp)
	rts

leave:	move.l	d2,PX(a6)
	move.l	d3,H(a6)
	move.l	a0,P(a6)
	move.l	a5,ALPHA(a6)
	move.l	52(sp),a3
	move.l	a1,(a3)			| *out
	move.l	d4,d0
	movem.l	(sp)+,d2-d7/a2-a6
	rts

| the op byte's handler, as an offset from the table
	.macro	JTAB	L
	.rept	64
	.word	\L\()_index-\L\()tab
	.endr
	.rept	64
	.word	\L\()_diff-\L\()tab
	.endr
	.rept	64
	.word	\L\()_luma-\L\()tab
	.endr
	.rept	62
	.word	\L\()_run-\L\()tab
	.endr
	.word	\L\()_full-\L\()tab, \L\()_full-\L\()tab
	.endm

	.even
atab:	JTAB	a
btab:	JTAB	b
