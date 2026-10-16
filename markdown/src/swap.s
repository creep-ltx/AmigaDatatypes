| callswapped(struct StackSwapStruct *sss, void (*fn)(void *), void *arg)
| Calls fn(arg) on the stack sss describes, then swaps back. In assembly
| so that nothing C keeps on the old stack (locals addressed from sp) is
| touched while the new one is in use. a2, a3 and d7 are kept by fn (gcc's
| callee-saved registers), so they carry the values across the call.
	.text
	.globl	_callswapped
_callswapped:
	movem.l	d7/a2/a3/a6,-(sp)
	move.l	20(sp),a2		| sss
	move.l	24(sp),a3		| fn
	move.l	28(sp),d7		| arg
	move.l	4.w,a6
	move.l	a2,a0
	jsr	-732(a6)		| StackSwap(sss): on the new stack
	move.l	d7,-(sp)
	jsr	(a3)
	addq.l	#4,sp
	move.l	4.w,a6
	move.l	a2,a0
	jsr	-732(a6)		| StackSwap(sss): back on the caller's
	movem.l	(sp)+,d7/a2/a3/a6
	rts

| rawputch: RawDoFmt's PutChProc for the debug trace (DEBUG builds):
| the character in d0 to exec's RawPutChar (LVO -516), which Wasabi's
| `debug` shows.
	.globl	_rawputch
_rawputch:
	move.l	a6,-(sp)
	move.l	4.w,a6
	jsr	-516(a6)
	move.l	(sp)+,a6
	rts
