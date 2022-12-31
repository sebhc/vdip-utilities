;C/80 Compiler 3.1 (4/11/84) - (c) 1984 The Software Toolworks

	XTEXT A:CLIBRARY.ASM
;/* test version calling */
;#include "printf.c"
;/* printf.c: library file for format, fformat, and printf;  5/23/84
; * compile: c -l32000
; * J. J. Gillogly, Apr 80
; * sprintf added J. J. Gillogly, Jul 80
; * Modified Jan 82 to work with #define kludge for multiple printf args
; * %u and unsigned octal and hex added WB Feb 82
; * long and floating descriptors added HT Apr 83
; * Split for float/nonfloat and long/nonlong versions WB 5/25/83.
; * Fix floating precision default, %3c, %% WB 11/15/83.
; * Fix for unsigned chars WB 12/26/83.
; * Fix for I/O redirection John Toscano 4/17/84.
; */
;#undef printf
;#undef fprintf
;#undef sprintf
;static int *prnt_p,	/* Pointer into printf arglist */
.d:	DW	0
;	    Pfo;	/* Channel number for output */
.e:	DW	0
;static char *Pf,	/* current location in user's format */
.f:	DW	0
;	    *Pst;	/* current location in user's output string */
.g:	DW	0
;static int Width;	/* minimum field width */
.h:	DW	0
;static int Pr;		/* precision */
.i:	DW	0
;static int ch;		/* really char but int is more efficient */
.j:	DW	0
;static char Pad;
.k:	DB	0
;static int ljust;	/* left-adjust flag */
.l:	DW	0
;static char *nbp,*nbpe; /* pointer to ascii converted number */
.m:	DW	0
.n:	DW	0
;/* define functions to reduce entry points */
;static Ps(),Pc(),getnum(),putsi(),putui(),putfld(),putpad(),hexdig();
;static char *ocvti();
;#define int_typ int
;#define uns_typ unsigned
;#define NBS 80
;/* prnt_1 is used to mark the position of
;   the printf argument list on the stack.
;   Visually, the arguments, starting with the format string, will
;   be stacked right below the marked position (prnt_p). The
;   stacked arguments are values !!! (16- or 32-bits)
;*/
;prnt_1(firstarg) { prnt_p = &firstarg; }	/* Record arglist start */

prnt_1:	LXI	H,2
	DAD	SP
	SHLD	.d
	RET
;prnt_2(lastarg) {
;	format (*--prnt_p);			/* Process arglist */

prnt_2:	LHLD	.d
	DCX	H
	DCX	H
	SHLD	.d
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	format
	POP	B
;#asm
;#asm
@prnt@: DS	0
;#endasm
;/* now we are going to print the values. Instead of passing to printf
;   the value to print (of unknown type), we pass the current pointer
;   to the argument list. It is the responsibility of "printf" to
;   retrieve the value physically located at prnt_p - 2 (if 16-bit) or
;   prnt_p - 4 (if 32-bit). "printf" must also update prnt_p accordingly.
;*/
;	while (prnt_p > &lastarg) printf(prnt_p); }
.x:	LXI	H,2
	DAD	SP
	XCHG
	LHLD	.d
	CALL	c.ult
	JZ	.y
	LHLD	.d
	PUSH	H
	CALL	printf
	POP	B
	JMP	.x
.y:	RET
;prnt_3(lastarg) {
;	Pfo = *--prnt_p;	

prnt_3:	LHLD	.d
	DCX	H
	DCX	H
	SHLD	.d
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.e
;	fformat(Pfo,*--prnt_p);
	LHLD	.e
	PUSH	H
	LHLD	.d
	DCX	H
	DCX	H
	SHLD	.d
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	fformat
	POP	B
	POP	B
;#asm
;#asm
	JMP	@prnt@
;#endasm
;/*	while (prnt_p > &lastarg) printf(prnt_p);	*/
;}
	RET
;prnt_4(lastarg) {	
;	Pst = *--prnt_p;

prnt_4:	LHLD	.d
	DCX	H
	DCX	H
	SHLD	.d
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.g
;	sformat(Pst,*--prnt_p);
	LHLD	.g
	PUSH	H
	LHLD	.d
	DCX	H
	DCX	H
	SHLD	.d
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	sformat
	POP	B
	POP	B
;#asm
;#asm
	JMP	@prnt@
;#endasm
;/*	while (prnt_p > &lastarg) printf(prnt_p);	*/
;}
	RET
;format(form)
;char *form;
;{	extern fout;
;	fformat(fout,form);

format:	LHLD	fout
	PUSH	H
	LXI	H,4
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	fformat
	POP	B
	POP	B
;}
	RET
;fformat(chan,form)
;char *form;
;{	Pfo = chan;

fformat:	LXI	H,4
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.e
;	Pf = form;
	LXI	H,2
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.f
;	Ps();
	CALL	.o
;}
	RET
;sformat(string,form)	/* user wants data to go to a string */
;char *string,*form;
;{	Pst = string;

sformat:	LXI	H,4
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.g
;	fformat(-2,form);/* illegal value for a channel */
	LXI	H,-2
	PUSH	H
	LXI	H,4
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	fformat
	POP	B
	POP	B
;}
	RET
;static Ps()    /* output format up to next % */
;{	while (*Pf != 0 && *Pf != '%') Pc(*Pf++);

.o:	DS	0
.z:	LHLD	.f
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	CALL	e.0
	JZ	.bb
	LHLD	.f
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	LXI	D,-37
	DAD	D
	CALL	e.0
.bb:	CALL	e.0
	JZ	.ab
	LHLD	.f
	INX	H
	SHLD	.f
	DCX	H
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	PUSH	H
	CALL	.p
	POP	B
	JMP	.z
.ab:	DS	0
;}
	RET
;static Pc(c)
;char c;
;{	if (Pfo == -2) {

.p:	LHLD	.e
	INX	H
	INX	H
	MOV	A,H
	ORA	L
	JNZ	.cb
;		*Pst++ = c;
	LHLD	.g
	INX	H
	SHLD	.g
	DCX	H
	PUSH	H
	LXI	H,4
	DAD	SP
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	POP	D
	MOV	A,L
	STAX	D
;		*Pst = 0;	/* terminate each intermediate string */
	LHLD	.g
	MVI	M,0
;		return; }
	RET
;	putc(c,Pfo);
.cb:	LXI	H,2
	DAD	SP
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	PUSH	H
	LHLD	.e
	PUSH	H
	CALL	putc
	POP	B
	POP	B
;}
	RET
;printf(arg)	/* print one arg using current format */
;union {
;	int *ival;
;	} arg;
;	{
;		static int racine,realPr;
.db:	DW	0
.eb:	DW	0
;		static unsigned int uarg;
.fb:	DW	0
;		static char *ptr,*end;
.gb:	DW	0
.hb:	DW	0
;		arg.ival = prnt_p ;

printf:	LXI	H,2
	DAD	SP
	PUSH	H
	LHLD	.d
	CALL	q.
;		prnt_p -= 1;	/* update assuming a 16-bit value to print */
	LHLD	.d
	DCX	H
	DCX	H
	SHLD	.d
;	again:	racine = Pr = 0;
.ib:	LXI	H,0
	SHLD	.i
	SHLD	.db
;		realPr = -1;
	LXI	H,-1
	SHLD	.eb
;		if ((ch = *++Pf) == 0) return;
	LHLD	.f
	INX	H
	SHLD	.f
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	SHLD	.j
	CALL	c.not
	JZ	.jb
	RET
;		Pad = ' ';
.jb:	LXI	H,32
	MOV	A,L
	STA	.k
;		if (ljust = (ch == '-')) ch = *Pf++;
	LHLD	.j
	LXI	D,-45
	DAD	D
	CALL	c.not
	SHLD	.l
	MOV	A,H
	ORA	L
	JZ	.kb
	LHLD	.f
	INX	H
	SHLD	.f
	DCX	H
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	SHLD	.j
;		if (ch == '0') { Pad = '0'; ch = *Pf++; }
.kb:	LHLD	.j
	LXI	D,-48
	DAD	D
	MOV	A,H
	ORA	L
	JNZ	.lb
	LXI	H,48
	MOV	A,L
	STA	.k
	LHLD	.f
	INX	H
	SHLD	.f
	DCX	H
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	SHLD	.j
;		Width = getnum();
.lb:	CALL	.q
	SHLD	.h
;		if (ch == '.') { ++Pf ; realPr = Pr = getnum(); }
	LHLD	.j
	LXI	D,-46
	DAD	D
	MOV	A,H
	ORA	L
	JNZ	.mb
	LHLD	.f
	INX	H
	SHLD	.f
	CALL	.q
	SHLD	.i
	SHLD	.eb
;		if ((ch = *Pf++) != 's' && Pr > 7) Pr = 7;
.mb:	LHLD	.f
	INX	H
	SHLD	.f
	DCX	H
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	SHLD	.j
	LXI	D,-115
	DAD	D
	CALL	e.0
	JZ	.ob
	LHLD	.i
	LXI	D,7
	CALL	c.lt
.ob:	CALL	e.0
	JZ	.nb
	LXI	H,7
	SHLD	.i
;		switch (ch) {	/* decode the conversion type */
.nb:	LHLD	.j
	JMP	.qb
;		case 'c':	ptr = --arg.ival;
.rb:	LXI	H,2
	DAD	SP
	PUSH	H
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	DCX	H
	DCX	H
	CALL	q.
	SHLD	.gb
;				putfld(ptr,ptr+1); break;
	LHLD	.gb
	PUSH	H
	LHLD	.gb
	INX	H
	PUSH	H
	CALL	.t
	POP	B
	POP	B
	JMP	.pb
;		case 'd':	putsi((int_type)*--arg.ival,10); break;
.sb:	LXI	H,2
	DAD	SP
	PUSH	H
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	DCX	H
	DCX	H
	CALL	q.
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	LXI	H,10
	PUSH	H
	CALL	.r
	POP	B
	POP	B
	JMP	.pb
;		case 'o' :	racine = 8; goto p_ui;
.tb:	LXI	H,8
	SHLD	.db
	JMP	.ub
;		case 'u' :	racine = 10; goto p_ui;
.vb:	LXI	H,10
	SHLD	.db
	JMP	.ub
;		case 'x' :	racine = 16;
.wb:	LXI	H,16
	SHLD	.db
;	p_ui:			uarg = *--arg.ival;
.ub:	LXI	H,2
	DAD	SP
	PUSH	H
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	DCX	H
	DCX	H
	CALL	q.
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.fb
;				putui((uns_type)uarg,racine);
	LHLD	.fb
	PUSH	H
	LHLD	.db
	PUSH	H
	CALL	.s
	POP	B
	POP	B
;				break;
	JMP	.pb
;		case 's' :	
.xb:	DS	0
;				for (end = ptr = *--arg.ival; *end++; );
	LXI	H,2
	DAD	SP
	PUSH	H
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	DCX	H
	DCX	H
	CALL	q.
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.gb
	SHLD	.hb
.ac:	LHLD	.hb
	INX	H
	SHLD	.hb
	DCX	H
	MOV	A,M
	ORA	A
	JZ	.zb
	JMP	.ac
.zb:	DS	0
;				if (--end - ptr > Pr && Pr) end = ptr + Pr;
	LHLD	.hb
	DCX	H
	SHLD	.hb
	PUSH	H
	LHLD	.gb
	CALL	s.
	XCHG
	LHLD	.i
	CALL	c.gt
	JZ	.cc
	LHLD	.i
.cc:	CALL	e.0
	JZ	.bc
	LHLD	.gb
	XCHG
	LHLD	.i
	DAD	D
	SHLD	.hb
;				putfld(ptr,end);
.bc:	LHLD	.gb
	PUSH	H
	LHLD	.hb
	PUSH	H
	CALL	.t
	POP	B
	POP	B
;				break;
	JMP	.pb
;		case 0	 :	return;
.dc:	RET
;		default  :	Pc(ch); Ps(); goto again;
.ec:	LHLD	.j
	PUSH	H
	CALL	.p
	POP	B
	CALL	.o
	JMP	.ib
;		}
	JMP	.pb
.qb:	CALL	.switch
	DW	.rb,99
	DW	.sb,100
	DW	.tb,111
	DW	.vb,117
	DW	.wb,120
	DW	.xb,115
	DW	.dc,0
	DW	0
	JMP	.ec
.pb:	DS	0
;		
;	Ps();
	CALL	.o
;}
	RET
;static getnum () {
;	int n;
.fc:	DW	0
;	for (n=0; (ch = *Pf) >= '0' && ch <='9'; )

.q:	LHLD	.fc
	PUSH	H
	LXI	H,0
	SHLD	.fc
.ic:	LHLD	.f
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	SHLD	.j
	LXI	D,48
	CALL	c.le
	JZ	.jc
	LHLD	.j
	LXI	D,57
	CALL	c.ge
.jc:	CALL	e.0
	JZ	.hc
;		n = 10*n + *Pf++ - '0';
	LHLD	.fc
	LXI	D,10
	CALL	c.mult
	PUSH	H
	LHLD	.f
	INX	H
	SHLD	.f
	DCX	H
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	POP	D
	DAD	D
	LXI	D,-48
	DAD	D
	SHLD	.fc
	JMP	.ic
.hc:	DS	0
;	return n;
	LHLD	.fc
.kc:	XCHG
	POP	H
	SHLD	.fc
	XCHG
	RET
;}
;/* unsigned integer/long conversions to ascii. the number sn is
;   converted in the specified radix sr. the ascii digits are
;   generated in a buffer. the value returned is a pointer to
;   the first character in the buffer.  The following routines use
;   either longs or ints, depending on whether NOLONG is defined.
;*/
;static char *
;ocvti (buflim, aln, alr)
;char *buflim; uns_type aln;
;	{
;	static int lr;
.lc:	DW	0
;	static uns_type ln;
.mc:	DW	0
;	lr = alr; ln = aln;

.w:	LXI	H,2
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.lc
	LXI	H,4
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	SHLD	.mc
;	do {
.pc:	DS	0
;		*--buflim = hexdig(lr == 10 ? ln % 10 : ln & (lr -1));
	LXI	H,6
	DAD	SP
	PUSH	H
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	DCX	H
	CALL	q.
	PUSH	H
	LHLD	.lc
	LXI	D,-10
	DAD	D
	CALL	c.not
	JZ	.qc
	LHLD	.mc
	XCHG
	LXI	H,10
	CALL	c.udv
	XCHG
	JMP	.rc
.qc:	LHLD	.lc
	DCX	H
	PUSH	H
	LHLD	.mc
	CALL	a.
.rc:	PUSH	H
	CALL	.v
	POP	B
	POP	D
	MOV	A,L
	STAX	D
;		ln = lr == 8 ? ln >>3 : lr == 10 ? ln/10 : ln>>4 ;
	LHLD	.lc
	LXI	D,-8
	DAD	D
	CALL	c.not
	JZ	.sc
	LHLD	.mc
	XCHG
	LXI	H,3
	CALL	c.usr
	JMP	.tc
.sc:	LHLD	.lc
	LXI	D,-10
	DAD	D
	CALL	c.not
	JZ	.uc
	LHLD	.mc
	XCHG
	LXI	H,10
	CALL	c.udv
	JMP	.vc
.uc:	LHLD	.mc
	XCHG
	LXI	H,4
	CALL	c.usr
.vc:	DS	0
.tc:	SHLD	.mc
;		}
;	while (ln) ;
.nc:	LHLD	.mc
	MOV	A,H
	ORA	L
	JNZ	.pc
.oc:	DS	0
;	return buflim;
	LXI	H,6
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	RET
;}
;/* Output a signed integer in the specified radix. */
;static putsi(n,radix)
;int_type n;	{
;	char nbuf[NBS];  /* conversion buffer */
;	static int_type dsn;
.wc:	DW	0
;	dsn = n < 0 ? -n : n;

.r:	LXI	H,-80
	DAD	SP
	SPHL
	LXI	H,84
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	LXI	D,0
	CALL	c.gt
	JZ	.xc
	LXI	H,84
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	CALL	c.neg
	JMP	.yc
.xc:	LXI	H,84
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
.yc:	SHLD	.wc
;	nbp = ocvti ( nbpe = nbuf+NBS, dsn, radix);
	LXI	H,0
	DAD	SP
	LXI	D,80
	DAD	D
	SHLD	.n
	PUSH	H
	LHLD	.wc
	PUSH	H
	LXI	H,86
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	.w
	POP	B
	POP	B
	POP	B
	SHLD	.m
;	if (n < 0) {	if (Pad != ' ') { Pc('-'); --Width; }
	LXI	H,84
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	LXI	D,32768
	MOV	A,H
	XRI	128
	MOV	H,A
	DAD	D
	JC	.zc
	LDA	.k
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	LXI	D,-32
	DAD	D
	MOV	A,H
	ORA	L
	JZ	.ad
	LXI	H,45
	PUSH	H
	CALL	.p
	POP	B
	LHLD	.h
	DCX	H
	SHLD	.h
;			      else *--nbp = '-';
	JMP	.bd
.ad:	LHLD	.m
	DCX	H
	SHLD	.m
	MVI	M,45
.bd:	DS	0
;		   }
;	putfld (nbp, nbpe);
.zc:	LHLD	.m
	PUSH	H
	LHLD	.n
	PUSH	H
	CALL	.t
	POP	B
	POP	B
;}
.cd:	LXI	H,80
	DAD	SP
	SPHL
	RET
;static putui(ui,radix) /* output an unsigned integer in the specified radix */
;uns_type ui;
;	{
;	char nbuf[NBS];  /* conversion buffer */
;	nbp = ocvti (nbpe = nbuf + NBS, ui, radix);

.s:	LXI	H,-80
	DAD	SP
	SPHL
	LXI	H,0
	DAD	SP
	LXI	D,80
	DAD	D
	SHLD	.n
	PUSH	H
	LXI	H,86
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	LXI	H,86
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	.w
	POP	B
	POP	B
	POP	B
	SHLD	.m
;	putfld (nbp, nbpe);
	LHLD	.m
	PUSH	H
	LHLD	.n
	PUSH	H
	CALL	.t
	POP	B
	POP	B
;}
.dd:	LXI	H,80
	DAD	SP
	SPHL
	RET
;	
;static putfld(string, end)/* output a field containing a specified string ending
;			     at end. Takes care of justification and padding.
;			  */
;char *string,*end;
;	{
;	static int length;
.ed:	DW	0
;	length = end - string;

.t:	LXI	H,2
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	LXI	H,6
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	CALL	s.
	SHLD	.ed
;	if (! ljust)
	LHLD	.l
	CALL	c.not
	JZ	.fd
;		putpad(Pad,length);
	LDA	.k
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	PUSH	H
	LHLD	.ed
	PUSH	H
	CALL	.u
	POP	B
	POP	B
;	while (length--) {
.fd:	DS	0
.gd:	LHLD	.ed
	DCX	H
	SHLD	.ed
	INX	H
	MOV	A,H
	ORA	L
	JZ	.hd
;		Pc (*string++);
	LXI	H,4
	DAD	SP
	PUSH	H
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	INX	H
	CALL	q.
	DCX	H
	MOV	A,M
	MOV	L,A
	RLC
	SBB	A
	MOV	H,A
	PUSH	H
	CALL	.p
	POP	B
;		--Width;
	LHLD	.h
	DCX	H
	SHLD	.h
;		}
	JMP	.gd
.hd:	DS	0
;	if (ljust) putpad(' ',0);
	LHLD	.l
	MOV	A,H
	ORA	L
	JZ	.id
	LXI	H,32
	PUSH	H
	LXI	H,0
	PUSH	H
	CALL	.u
	POP	B
	POP	B
;}
.id:	RET
;static putpad(ch,count) {
;	while (Width > count) { Pc(ch); --Width; }

.u:	DS	0
.jd:	LXI	H,2
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	XCHG
	LHLD	.h
	CALL	c.lt
	JZ	.kd
	LXI	H,4
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	PUSH	H
	CALL	.p
	POP	B
	LHLD	.h
	DCX	H
	SHLD	.h
	JMP	.jd
.kd:	DS	0
;	}
	RET
;static hexdig(c) {
;	return c + (c <= 9 ? '0' : 'A' - 10); }

.v:	LXI	H,2
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	LXI	D,9
	CALL	c.ge
	JZ	.ld
	LXI	H,48
	JMP	.md
.ld:	LXI	H,55
.md:	XCHG
	LXI	H,2
	DAD	SP
	MOV	A,M
	INX	H
	MOV	H,M
	MOV	L,A
	DAD	D
	RET
;/* Definitions for printf and fprintf to allow multiple args. */
;#define printf prnt_1(),prnt_2
;#define fprintf prnt_1(),prnt_3
;#define sprintf prnt_1(),prnt_4
;
;/* #define HDOS 1 */
;char *osname;
osname:	DW	0
;int osver;
osver:	DW	0
;/********************************************************
;**
;** getosver
;**
;** set globals 'osname' and 'osver' to reflect the current
;** operating system and version. These are used to direct
;** code to the appropriate OS-specific function calls.
;**
;********************************************************/
;int getosver()
;{
;	int ver;
.nd:	DW	0
;	
;	ver = bdoshl(0x0C);

getosve:	LHLD	.nd
	PUSH	H
	LXI	H,12
	PUSH	H
	CALL	bdoshl
	POP	B
	SHLD	.nd
;  /* H == 0 -> CP/M, H == 1 -> MP/M
;	** release is BCD stored in L
;	*/
;	if ((ver>>8) == 0)
	LHLD	.nd
	XCHG
	LXI	H,8
	CALL	c.asr
	CALL	c.not
	JZ	.od
;		osname = "CPM";
	LXI	H,.c
	SHLD	osname
;	else
	JMP	.pd
.od:	DS	0
;		osname = "MPM";
	LXI	H,.c+4
	SHLD	osname
.pd:	DS	0
;	osver = ver && 0xFF;
	LHLD	.nd
	MOV	A,H
	ORA	L
	JZ	.qd
	LXI	H,255
.qd:	CALL	e.0
	SHLD	osver
;}
.rd:	POP	H
	SHLD	.nd
	RET
;/********************************************************
;**
;**	*** Valid for use only in CP/M ***
;**
;** bdoshl - Call CP/M BDOS function with given values
;**		for registers C and DE.  Return the value
;**		from the HL register.
;**
;********************************************************/
;int bdoshl() {
;#asm

bdoshl:	DS	0
;#asm
        POP     H
        POP     D
        POP     B
        PUSH    B
        PUSH    D
        PUSH    H
        CALL    5
;#endasm
;}
	RET
;main(argc, argv)
;int argc;
;char *argv[];
;{
;	
;	getosver();

main:	CALL	getosve
;	
;	printf("%s Operating System\n", osname);
	CALL	prnt_1
	LXI	H,.c+8
	PUSH	H
	LHLD	osname
	PUSH	H
	CALL	prnt_2
	POP	B
	POP	B
;	printf("Version: %x\n", osver);
	CALL	prnt_1
	LXI	H,.c+29
	PUSH	H
	LHLD	osver
	PUSH	H
	CALL	prnt_2
	POP	B
	POP	B
;}
	RET
;#include "stdlib.c"
;/* stdlib.c - Standard C library - (c) 1983 The Software Toolworks.
;   Some of these routines were provided by Jim and Gary Gilbreath.
;   The fast str routines were donated by Dr. Jim Gillogly.
;   If this file is #included at the end of a source program, routines
;   will be compiled automatically as needed.
;   In the following, s, t are strings terminated by a 0 byte, and c is
;   a char or an int containing an ASCII character.  Other args are ints
;   except where noted otherwise.
;   abs(i)	Absolute value of i.
;   alloc(i)	Returns pointer to i bytes of available memory.
;   atoi(s)	Integer value of string s, optional leading +/-,
;		stops at first non-digit.
;   bdos(c,de)	Calls CP/M bdos with registers shown; returns contents of A,
;		as a 16-bit sign-extended number.
;   free(p)	Frees up memory at p.  P must have been value returned by alloc.
;   getline(s,i) Reads a line from console into s, stopping at newline or i char-
;		acters.  0-terminates string; returns length including 0 byte.
;   index(s,t)	Position in string s of string t, or -1 if not there.
;   isalpha(c)	1 if c is an alphabetic character A-Z or a-z.
;   isdigit(c)	1 if c is an ASCII digit 0-9.
;   islower(c)	1 if c is a lower case character a-z.
;   isspace(c)	1 if c is a blank, tab or newline.
;   isupper(c)	1 if c is an upper case character A-Z.
;   itoa(i,s)	Converts i to an ASCII string in char s[7]; returns s.
;   makfcb(s,fcb) Unpack file name s into char fcb[36] to make a CP/M fcb.
;   max(i,j)	The greater of i and j.
;   min(i,j)	The lesser of i and j.
;   rename(s,t)	Renames the file named s to be called t.  Returns -1 for
;		failure, a non-negative number for success.
;   strcat(s,t)	Appends t to s.  No checking for overflow of s.
;   strcmp(s,t)	-1, 0 or 1 if s < t, s == t, s > t respectively.
;   strcpy(s,t)	Copies t into s.  No checking for overflow of s.
;   strlen(s)	The number of bytes in s, exclusive of the 0 byte.
;		(Note: it takes strlen(s)+1 bytes to hold s.)
;   tolower(c)	c, but if upper case it is converted to lower case.
;   toupper(c)	c, but if lower case it is converted to upper case.
;   unlink(s)	Deletes the file s, if it exists.
;   None of these routines calls any other functions, except that the file
;   routines call bdos and makfcb. */
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;/*	*	*/
;
;
.c:
	DB	'CPM'
	DB	0
	DB	'MPM'
	DB	0
	DB	'%s Operating System'
	DB	10
	DB	0
	DB	'Version: %x'
	DB	10
	DB	0
$END	DS	0
	END	$INIT
