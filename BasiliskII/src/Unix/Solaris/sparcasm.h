#ifndef SPARC_ASSEMBLY__HEADER
#define SPARC_ASSEMBLY__HEADER

#ifdef SPARC_V8_ASSEMBLY

static inline char *str_flags(void)
{
	static char str[8];
	sprintf(str, "%c%c%c%c%c",
		GET_XFLG ? 'X' : '-',
		GET_NFLG ? 'N' : '-',
		GET_ZFLG ? 'Z' : '-',
		GET_VFLG ? 'V' : '-',
		GET_CFLG ? 'C' : '-'
		);
	return str;
}

static inline uae_u32 sparc_v8_flag_add_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_add_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_add_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	addcc	%2, %3, %0\n"
		"	addx	%%g0, %%g0, %%o0	! X,C flags\n"
		"	stb		%%o0, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_sub_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_sub_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_sub_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	subcc	%2, %3, %0\n"
		"	addx	%%g0, %%g0, %%o0	! X,C flags\n"
		"	stb		%%o0, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline void sparc_v8_flag_cmp_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	sll		%2, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	addx	%%g0, %%g0, %%o0	! C flag\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v8_flag_cmp_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	sll		%2, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	addx	%%g0, %%g0, %%o0	! C flag\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v8_flag_cmp_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	subcc	%1, %2, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline uae_u32 sparc_v8_flag_addx_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o1		! Get the X Flag\n"
		"	subcc	%%g0, %%o1, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

#if 0
VERY SLOW...
static inline uae_u32 sparc_v8_flag_addx_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	ldub	[%1 + 1], %%o2\n"
		"	subcc	%%g0, %%o2, %%g0\n"
		"	addx	%%g0, %%g0, %%o2\n"
		"	sll		%%o2, 24, %%o2\n"
		"	addcc	%%o0, %%o2, %%o0\n"
		"	srl		%%o0, 24, %0\n"
		"	addx	%%g0, %%g0, %%o2\n"
		"	or		%%o1, %%o2, %%o1	! update X,C flags\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	ldub	[%1], %%o0			! retreive the old NZVC flags (XXX)\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! update V flag\n"
		"	and		%%o0, 0x04, %%o0	! (XXX) but keep only Z flag\n"
		"	and		%%o1, 1, %%o2		! keep C flag in %%o2\n"
		"	bnz,a	.+8\n"
		"	or		%%g0, %%g0, %%o0	! Z flag cleared if non-zero result\n"
		"	stb		%%o2, [%1 + 1]		! store the X flag\n"
		"	or		%%o1, %%o0, %%o1\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1", "o2"
	);
	return value;
}
#endif

static inline uae_u32 sparc_v8_flag_addx_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o0		! Get the X Flag\n"
		"	subcc	%%g0, %%o0, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
		"	ldub	[%1], %%o0			! retreive the old NZVC flags\n"
		"	and		%%o0, 0x04, %%o0	! but keep only Z flag\n"
		"	addx	%%o0, %%g0, %%o0	! X,C flags\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	bnz,a	.+8\n"
		"	and		%%o0, 0x0B, %%o0	! Z flag cleared if result is non-zero\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1 + 1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

#endif /* SPARC_V8_ASSEMBLY */

#ifdef SPARC_V9_ASSEMBLY

static inline uae_u32 sparc_v9_flag_add_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_add_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_add_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	addcc	%2, %3, %0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_sub_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_sub_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_sub_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	subcc	%2, %3, %0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline void sparc_v9_flag_cmp_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	sll		%2, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_cmp_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	sll		%2, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_cmp_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	subcc	%1, %2, %%g0\n"
#if 0
		"	subcc	%1, %2, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
#endif
#if 0
		"	subcc	%1, %2, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,pt,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
#endif
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

#if 1
static inline void sparc_v9_flag_test_8(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	subcc	%%o0, %%g0, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0"
	);
}

static inline void sparc_v9_flag_test_16(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	subcc	%%o0, %%g0, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0"
	);
}

static inline void sparc_v9_flag_test_32(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	subcc	%1, %%g0, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0"
	);
}
#else
static inline void sparc_v9_flag_test_8(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	subcc	%%o0, %%g0, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_test_16(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	subcc	%%o0, %%g0, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_test_32(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	subcc	%1, %%g0, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0", "o1"
	);
}
#endif

static inline uae_u32 sparc_v9_flag_addx_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o1		! Get the X Flag\n"
		"	subcc	%%g0, %%o1, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_addx_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o0		! Get the X Flag\n"
		"	subcc	%%g0, %%o0, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
		"	ldub	[%1], %%o0			! retreive the old NZVC flags\n"
		"	and		%%o0, 0x04, %%o0	! but keep only Z flag\n"
		"	addx	%%o0, %%g0, %%o0	! X,C flags\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	bnz,a	.+8\n"
		"	and		%%o0, 0x0B, %%o0	! Z flag cleared if result is non-zero\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1 + 1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

#endif /* SPARC_V9_ASSEMBLY */

#endif /* SPARC_ASSEMBLY__HEADER */
