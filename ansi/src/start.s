| ansi.datatype's first code: started as a program, it ends at once.
	.text
	.globl	_start
_start:
	moveq	#-1,d0
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
