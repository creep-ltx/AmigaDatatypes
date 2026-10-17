| The recognition code's entry: datatypes.library loads the DTCD chunk with
| InternalLoadSeg and calls the start of its first hunk with the
| DTHookContext. Commodore's library passes it in a0; AROS's calls it as a
| C function (on the stack). a0 is taken when it points at a context (its
| first long is SysBase), the stack otherwise.
	.text
	.globl	_start
_start:
	move.l	a0,d0
	beq.s	1f
	btst	#0,d0
	bne.s	1f
	move.l	4.w,d1
	cmp.l	(a0),d1
	beq.s	2f
1:	move.l	4(sp),a0
2:	move.l	a0,-(sp)
	jsr	_ansihook
	addq.l	#4,sp
	rts
