#ifndef NOFLAGS_H
#define NOFLAGS_H

/* Undefine everything that will *set* flags. Note: Leave *reading*
   flags alone ;-). We assume that nobody does something like 
   SET_ZFLG(a=b+c), i.e. expect side effects of the macros. That would 
   be a stupid thing to do when using macros.
*/

/* Gwenole Beauchesne pointed out that CAS and CAS2 use flag_cmp to set
   flags that are then used internally, and that thus the noflags versions
   of those instructions were broken. Oops! 
   Easy fix: Leave flag_cmp alone. It is only used by CMP* and CAS* 
   instructions. For CAS*, noflags is a bad idea. For CMP*, which has
   setting flags as its only function, the noflags version is kinda pointless,
   anyway. 
   Note that this will only work while using the optflag_* routines ---
   as we do on all (one ;-) platforms that will ever use the noflags
   versions, anyway.
   However, if you try to compile without optimized flags, the "SET_ZFLAG"
   macro will be left unchanged, to make CAS and CAS2 work right. Of course,
   this is contrary to the whole idea of noflags, but better be right than
   be fast.

   Another problem exists with one of the bitfield operations. Once again,
   one of the operations sets a flag, and looks at it later. And the CHK2
   instruction does so as well. For those, a different solution is possible.
   the *_ALWAYS versions of the SET_?FLG macros shall remain untouched by 
   the redefinitions in this file.
   Unfortunately, they are defined in terms of the macros we *do* redefine.
   So here comes a bit of trickery....
*/
#define NOFLAGS_CMP 0

#undef SET_NFLG_ALWAYS
static __inline__ void SET_NFLG_ALWAYS(uae_u32 x)
{
    SET_NFLG(x);  /* This has not yet been redefined */
}

#undef SET_CFLG_ALWAYS
static __inline__ void SET_CFLG_ALWAYS(uae_u32 x)
{
    SET_CFLG(x);  /* This has not yet been redefined */
}

#undef CPUFUNC
#define CPUFUNC(x) x##_nf

#ifndef OPTIMIZED_FLAGS
#undef SET_ZFLG
#define SET_ZFLG(y) do {uae_u32 dummy=(y); } while (0)
#endif

#undef SET_CFLG
#define SET_CFLG(y) do {uae_u32 dummy=(y); } while (0)
#undef SET_VFLG
#define SET_VFLG(y) do {uae_u32 dummy=(y); } while (0)
#undef SET_NFLG
#define SET_NFLG(y) do {uae_u32 dummy=(y); } while (0)
#undef SET_XFLG
#define SET_XFLG(y) do {uae_u32 dummy=(y); } while (0)

#undef CLEAR_CZNV
#define CLEAR_CZNV
#undef IOR_CZNV
#define IOR_CZNV(y) do {uae_u32 dummy=(y); } while (0)
#undef SET_CZNV
#define SET_CZNV(y) do {uae_u32 dummy=(y); } while (0)
#undef COPY_CARRY
#define COPY_CARRY 

#ifdef  optflag_testl
#undef  optflag_testl
#endif

#ifdef  optflag_testw
#undef  optflag_testw
#endif

#ifdef  optflag_testb
#undef  optflag_testb
#endif

#ifdef  optflag_addl
#undef  optflag_addl
#endif

#ifdef  optflag_addw
#undef  optflag_addw
#endif

#ifdef  optflag_addb
#undef  optflag_addb
#endif

#ifdef  optflag_subl
#undef  optflag_subl
#endif

#ifdef  optflag_subw
#undef  optflag_subw
#endif

#ifdef  optflag_subb
#undef  optflag_subb
#endif

#if NOFLAGS_CMP
#ifdef  optflag_cmpl
#undef  optflag_cmpl
#endif

#ifdef  optflag_cmpw
#undef  optflag_cmpw
#endif

#ifdef  optflag_cmpb
#undef  optflag_cmpb
#endif
#endif

#define optflag_testl(v) do { } while (0)
#define optflag_testw(v) do { } while (0)
#define optflag_testb(v) do { } while (0)

#define optflag_addl(v, s, d) (v = (uae_s32)(d) + (uae_s32)(s))
#define optflag_addw(v, s, d) (v = (uae_s16)(d) + (uae_s16)(s))
#define optflag_addb(v, s, d) (v = (uae_s8)(d) + (uae_s8)(s))

#define optflag_subl(v, s, d) (v = (uae_s32)(d) - (uae_s32)(s))
#define optflag_subw(v, s, d) (v = (uae_s16)(d) - (uae_s16)(s))
#define optflag_subb(v, s, d) (v = (uae_s8)(d) - (uae_s8)(s))

#if NOFLAGS_CMP
/* These are just for completeness sake */
#define optflag_cmpl(s, d) do { } while (0)
#define optflag_cmpw(s, d) do { } while (0)
#define optflag_cmpb(s, d) do { } while (0)
#endif

#endif
