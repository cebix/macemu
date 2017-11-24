#define ADD_RAX_RCX 0x01,0xc8
#define ADD_RDX_RCX 0x01,0xca
#define ADD_RAX_RDX 0x01,0xd0
#define TRANS_RAX \
	0x48,0x3D,0x00,0x30,0x00,0x00,\
	0x72,0x16,\
	0x48,0x3D,0x00,0xE0,0xFF,0x5F,\
	0x72,0x14,\
	0x48,0x25,0xFF,0x1F,0x00,0x00,\
	0x48,0x05,0x00,0x00,0x00,0x00,\
	0xEB,0x06,\
	0x48,0x05,0x00,0x00,0x00,0x00

#define TRANS_RDX \
	0x48,0x81,0xFA,0x00,0x30,0x00,0x00,\
	0x72,0x19,\
	0x48,0x81,0xFA,0x00,0xE0,0xFF,0x5F,\
	0x72,0x17,\
	0x48,0x81,0xE2,0xFF,0x1F,0x00,0x00,\
	0x48,0x81,0xC2,0x00,0x00,0x00,0x00,\
	0xEB,0x07,\
	0x48,0x81,0xC2,0x00,0x00,0x00,0x00

#ifdef DYNGEN_IMPL
extern uint8 gZeroPage[0x3000], gKernelData[0x2000];
#endif

#ifndef DEFINE_CST
#define DEFINE_CST(NAME, VALUE)
#endif
DEFINE_GEN(gen_op_invoke,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke
{
    static const uint8 helper_op_invoke_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_code, 12);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0
{
    static const uint8 helper_op_invoke_T0_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89,
       0xe7, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_T0_code, 15);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(15);
}
#endif

DEFINE_GEN(gen_op_invoke_T0_T1,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0_T1
{
    static const uint8 helper_op_invoke_T0_T1_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89,
       0xee, 0x44, 0x89, 0xe7, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_T0_T1_code, 18);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(18);
}
#endif

DEFINE_GEN(gen_op_invoke_T0_T1_T2,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0_T1_T2
{
    static const uint8 helper_op_invoke_T0_T1_T2_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89,
       0xf2, 0x44, 0x89, 0xee, 0x44, 0x89, 0xe7, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_T0_T1_T2_code, 21);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(21);
}
#endif

DEFINE_GEN(gen_op_invoke_T0_ret_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0_ret_T0
{
    static const uint8 helper_op_invoke_T0_ret_T0_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89,
       0xe7, 0xff, 0xd0, 0x41, 0x89, 0xc4
    };
    copy_block(helper_op_invoke_T0_ret_T0_code, 18);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(18);
}
#endif

DEFINE_GEN(gen_op_invoke_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_im
{
    static const uint8 helper_op_invoke_im_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x3d,
       0x00, 0x00, 0x00, 0x00, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_im_code, 18);
    *(uint32_t *)(code_ptr() + 12) = (int32_t)((long)param2 - (long)(code_ptr() + 12 + 4)) + 0;
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(18);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU
{
    static const uint8 helper_op_invoke_CPU_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89,
       0xef, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_CPU_code, 15);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(15);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_T0
{
    static const uint8 helper_op_invoke_CPU_T0_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89,
       0xe6, 0x48, 0x89, 0xef, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_CPU_T0_code, 18);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(18);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_im
{
    static const uint8 helper_op_invoke_CPU_im_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89,
       0xef, 0x8d, 0x35, 0x00, 0x00, 0x00, 0x00, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_CPU_im_code, 21);
    *(uint32_t *)(code_ptr() + 15) = (int32_t)((long)param2 - (long)(code_ptr() + 15 + 4)) + 0;
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(21);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_im_im,void,(long param1, long param2, long param3))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_im_im
{
    static const uint8 helper_op_invoke_CPU_im_im_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89,
       0xef, 0x8d, 0x15, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x35, 0x00, 0x00, 0x00,
       0x00, 0xff, 0xd0
    };
    copy_block(helper_op_invoke_CPU_im_im_code, 27);
    *(uint32_t *)(code_ptr() + 21) = (int32_t)((long)param2 - (long)(code_ptr() + 21 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 15) = (int32_t)((long)param3 - (long)(code_ptr() + 15 + 4)) + 0;
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(27);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_A0_ret_A0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_A0_ret_A0
{
    static const uint8 helper_op_invoke_CPU_A0_ret_A0_code[] = {
       0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x89,
       0xe6, 0x48, 0x89, 0xef, 0xff, 0xd0, 0x49, 0x89, 0xc4
    };
    copy_block(helper_op_invoke_CPU_A0_ret_A0_code, 21);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(21);
}
#endif

DEFINE_GEN(gen_op_invoke_direct,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct
{
    static const uint8 helper_op_invoke_direct_code[] = {
       0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (int32_t)((long)param1 - (long)(code_ptr() + 1 + 4)) + 0;
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0
{
    static const uint8 helper_op_invoke_direct_T0_code[] = {
       0x44, 0x89, 0xe7, 0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_T0_code, 8);
    *(uint32_t *)(code_ptr() + 4) = (int32_t)((long)param1 - (long)(code_ptr() + 4 + 4)) + 0;
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0_T1,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0_T1
{
    static const uint8 helper_op_invoke_direct_T0_T1_code[] = {
       0x44, 0x89, 0xee, 0x44, 0x89, 0xe7, 0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_T0_T1_code, 11);
    *(uint32_t *)(code_ptr() + 7) = (int32_t)((long)param1 - (long)(code_ptr() + 7 + 4)) + 0;
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0_T1_T2,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0_T1_T2
{
    static const uint8 helper_op_invoke_direct_T0_T1_T2_code[] = {
       0x44, 0x89, 0xf2, 0x44, 0x89, 0xee, 0x44, 0x89, 0xe7, 0xe8, 0x00, 0x00,
       0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_T0_T1_T2_code, 14);
    *(uint32_t *)(code_ptr() + 10) = (int32_t)((long)param1 - (long)(code_ptr() + 10 + 4)) + 0;
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0_ret_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0_ret_T0
{
    static const uint8 helper_op_invoke_direct_T0_ret_T0_code[] = {
       0x44, 0x89, 0xe7, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x41, 0x89, 0xc4
    };
    copy_block(helper_op_invoke_direct_T0_ret_T0_code, 11);
    *(uint32_t *)(code_ptr() + 4) = (int32_t)((long)param1 - (long)(code_ptr() + 4 + 4)) + 0;
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_im
{
    static const uint8 helper_op_invoke_direct_im_code[] = {
       0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_im_code, 11);
    *(uint32_t *)(code_ptr() + 7) = (int32_t)((long)param1 - (long)(code_ptr() + 7 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param2 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU
{
    static const uint8 helper_op_invoke_direct_CPU_code[] = {
       0x48, 0x89, 0xef, 0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_code, 8);
    *(uint32_t *)(code_ptr() + 4) = (int32_t)((long)param1 - (long)(code_ptr() + 4 + 4)) + 0;
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_T0
{
    static const uint8 helper_op_invoke_direct_CPU_T0_code[] = {
       0x44, 0x89, 0xe6, 0x48, 0x89, 0xef, 0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_T0_code, 11);
    *(uint32_t *)(code_ptr() + 7) = (int32_t)((long)param1 - (long)(code_ptr() + 7 + 4)) + 0;
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_im
{
    static const uint8 helper_op_invoke_direct_CPU_im_code[] = {
       0x48, 0x89, 0xef, 0x8d, 0x35, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00,
       0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_im_code, 14);
    *(uint32_t *)(code_ptr() + 10) = (int32_t)((long)param1 - (long)(code_ptr() + 10 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 5) = (int32_t)((long)param2 - (long)(code_ptr() + 5 + 4)) + 0;
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_im_im,void,(long param1, long param2, long param3))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_im_im
{
    static const uint8 helper_op_invoke_direct_CPU_im_im_code[] = {
       0x48, 0x89, 0xef, 0x8d, 0x15, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x35, 0x00,
       0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_im_im_code, 20);
    *(uint32_t *)(code_ptr() + 16) = (int32_t)((long)param1 - (long)(code_ptr() + 16 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 11) = (int32_t)((long)param2 - (long)(code_ptr() + 11 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 5) = (int32_t)((long)param3 - (long)(code_ptr() + 5 + 4)) + 0;
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_A0_ret_A0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_A0_ret_A0
{
    static const uint8 helper_op_invoke_direct_CPU_A0_ret_A0_code[] = {
       0x4c, 0x89, 0xe6, 0x48, 0x89, 0xef, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x49,
       0x89, 0xc4
    };
    copy_block(helper_op_invoke_direct_CPU_A0_ret_A0_code, 14);
    *(uint32_t *)(code_ptr() + 7) = (int32_t)((long)param1 - (long)(code_ptr() + 7 + 4)) + 0;
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_jmp_fast,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jmp_fast
{
    static const uint8 op_jmp_fast_code[] = {
       0xe9, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_jmp_fast_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (int32_t)((long)param1 - (long)(code_ptr() + 1 + 4)) + 0;
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_jmp_slow,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jmp_slow
{
    static const uint8 op_jmp_slow_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0xff, 0xe0
    };
    copy_block(op_jmp_slow_code, 9);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(9);
}
#endif

DEFINE_GEN(gen_op_neg_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_neg_32_T0
{
    static const uint8 op_neg_32_T0_code[] = {
       0x41, 0xf7, 0xdc
    };
    copy_block(op_neg_32_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_not_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_not_32_T0
{
    static const uint8 op_not_32_T0_code[] = {
       0x45, 0x85, 0xe4, 0x0f, 0x94, 0xc0, 0x44, 0x0f, 0xb6, 0xe0
    };
    copy_block(op_not_32_T0_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_not_32_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_not_32_T1
{
    static const uint8 op_not_32_T1_code[] = {
       0x45, 0x85, 0xed, 0x0f, 0x94, 0xc0, 0x44, 0x0f, 0xb6, 0xe8
    };
    copy_block(op_not_32_T1_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_se_8_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_se_8_32_T0
{
    static const uint8 op_se_8_32_T0_code[] = {
       0x45, 0x0f, 0xbe, 0xe4
    };
    copy_block(op_se_8_32_T0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_ze_8_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ze_8_32_T0
{
    static const uint8 op_ze_8_32_T0_code[] = {
       0x45, 0x0f, 0xb6, 0xe4
    };
    copy_block(op_ze_8_32_T0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_1
{
    static const uint8 op_add_32_T0_1_code[] = {
       0x41, 0xff, 0xc4
    };
    copy_block(op_add_32_T0_1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_2
{
    static const uint8 op_add_32_T0_2_code[] = {
       0x41, 0x83, 0xc4, 0x02
    };
    copy_block(op_add_32_T0_2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_4
{
    static const uint8 op_add_32_T0_4_code[] = {
       0x41, 0x83, 0xc4, 0x04
    };
    copy_block(op_add_32_T0_4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_8
{
    static const uint8 op_add_32_T0_8_code[] = {
       0x41, 0x83, 0xc4, 0x08
    };
    copy_block(op_add_32_T0_8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_1
{
    static const uint8 op_add_32_T1_1_code[] = {
       0x41, 0xff, 0xc5
    };
    copy_block(op_add_32_T1_1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_2
{
    static const uint8 op_add_32_T1_2_code[] = {
       0x41, 0x83, 0xc5, 0x02
    };
    copy_block(op_add_32_T1_2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_4
{
    static const uint8 op_add_32_T1_4_code[] = {
       0x41, 0x83, 0xc5, 0x04
    };
    copy_block(op_add_32_T1_4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_8
{
    static const uint8 op_add_32_T1_8_code[] = {
       0x41, 0x83, 0xc5, 0x08
    };
    copy_block(op_add_32_T1_8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_bswap_16_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_bswap_16_T0
{
    static const uint8 op_bswap_16_T0_code[] = {
       0x44, 0x89, 0xe0, 0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xb7, 0xe0
    };
    copy_block(op_bswap_16_T0_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_bswap_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_bswap_32_T0
{
    static const uint8 op_bswap_32_T0_code[] = {
       0x41, 0x0f, 0xcc
    };
    copy_block(op_bswap_32_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_0
{
    static const uint8 op_mov_32_T0_0_code[] = {
       0x45, 0x31, 0xe4
    };
    copy_block(op_mov_32_T0_0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_0
{
    static const uint8 op_mov_32_T1_0_code[] = {
       0x45, 0x31, 0xed
    };
    copy_block(op_mov_32_T1_0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_0
{
    static const uint8 op_mov_32_T2_0_code[] = {
       0x45, 0x31, 0xf6
    };
    copy_block(op_mov_32_T2_0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_or_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_or_32_T0_T1
{
    static const uint8 op_or_32_T0_T1_code[] = {
       0x45, 0x09, 0xec
    };
    copy_block(op_or_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_or_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_or_32_T0_im
{
    static const uint8 op_or_32_T0_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x09, 0xc4
    };
    copy_block(op_or_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_se_16_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_se_16_32_T0
{
    static const uint8 op_se_16_32_T0_code[] = {
       0x45, 0x0f, 0xbf, 0xe4
    };
    copy_block(op_se_16_32_T0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_se_16_32_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_se_16_32_T1
{
    static const uint8 op_se_16_32_T1_code[] = {
       0x45, 0x0f, 0xbf, 0xed
    };
    copy_block(op_se_16_32_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_1
{
    static const uint8 op_sub_32_T0_1_code[] = {
       0x41, 0xff, 0xcc
    };
    copy_block(op_sub_32_T0_1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_2
{
    static const uint8 op_sub_32_T0_2_code[] = {
       0x41, 0x83, 0xec, 0x02
    };
    copy_block(op_sub_32_T0_2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_4
{
    static const uint8 op_sub_32_T0_4_code[] = {
       0x41, 0x83, 0xec, 0x04
    };
    copy_block(op_sub_32_T0_4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_8
{
    static const uint8 op_sub_32_T0_8_code[] = {
       0x41, 0x83, 0xec, 0x08
    };
    copy_block(op_sub_32_T0_8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_1
{
    static const uint8 op_sub_32_T1_1_code[] = {
       0x41, 0xff, 0xcd
    };
    copy_block(op_sub_32_T1_1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_2
{
    static const uint8 op_sub_32_T1_2_code[] = {
       0x41, 0x83, 0xed, 0x02
    };
    copy_block(op_sub_32_T1_2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_4
{
    static const uint8 op_sub_32_T1_4_code[] = {
       0x41, 0x83, 0xed, 0x04
    };
    copy_block(op_sub_32_T1_4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_8
{
    static const uint8 op_sub_32_T1_8_code[] = {
       0x41, 0x83, 0xed, 0x08
    };
    copy_block(op_sub_32_T1_8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_ze_16_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ze_16_32_T0
{
    static const uint8 op_ze_16_32_T0_code[] = {
       0x45, 0x0f, 0xb7, 0xe4
    };
    copy_block(op_ze_16_32_T0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_T1
{
    static const uint8 op_add_32_T0_T1_code[] = {
       0x47, 0x8d, 0x64, 0x25, 0x00
    };
    copy_block(op_add_32_T0_T1_code, 5);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_T2
{
    static const uint8 op_add_32_T0_T2_code[] = {
       0x47, 0x8d, 0x24, 0x26
    };
    copy_block(op_add_32_T0_T2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_im
{
    static const uint8 op_add_32_T0_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x01, 0xc4
    };
    copy_block(op_add_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_T0
{
    static const uint8 op_add_32_T1_T0_code[] = {
       0x47, 0x8d, 0x2c, 0x2c
    };
    copy_block(op_add_32_T1_T0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_T2
{
    static const uint8 op_add_32_T1_T2_code[] = {
       0x47, 0x8d, 0x2c, 0x2e
    };
    copy_block(op_add_32_T1_T2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_im
{
    static const uint8 op_add_32_T1_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x01, 0xc5
    };
    copy_block(op_add_32_T1_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_and_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_and_32_T0_T1
{
    static const uint8 op_and_32_T0_T1_code[] = {
       0x45, 0x21, 0xec
    };
    copy_block(op_and_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_and_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_and_32_T0_im
{
    static const uint8 op_and_32_T0_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x21, 0xc4
    };
    copy_block(op_and_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_asr_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_asr_32_T0_T1
{
    static const uint8 op_asr_32_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xfc
    };
    copy_block(op_asr_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_asr_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_asr_32_T0_im
{
    static const uint8 op_asr_32_T0_im_code[] = {
       0x48, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x41, 0xd3, 0xfc
    };
    copy_block(op_asr_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_eqv_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_eqv_32_T0_T1
{
    static const uint8 op_eqv_32_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x31, 0xe0, 0x41, 0x89, 0xc4, 0x41, 0xf7, 0xd4
    };
    copy_block(op_eqv_32_T0_T1_code, 12);
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_lsl_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsl_32_T0_T1
{
    static const uint8 op_lsl_32_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xe4
    };
    copy_block(op_lsl_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_lsl_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsl_32_T0_im
{
    static const uint8 op_lsl_32_T0_im_code[] = {
       0x48, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x41, 0xd3, 0xe4
    };
    copy_block(op_lsl_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_lsr_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsr_32_T0_T1
{
    static const uint8 op_lsr_32_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xec
    };
    copy_block(op_lsr_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_lsr_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsr_32_T0_im
{
    static const uint8 op_lsr_32_T0_im_code[] = {
       0x48, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x41, 0xd3, 0xec
    };
    copy_block(op_lsr_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_T1
{
    static const uint8 op_mov_32_T0_T1_code[] = {
       0x45, 0x89, 0xec
    };
    copy_block(op_mov_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_T2
{
    static const uint8 op_mov_32_T0_T2_code[] = {
       0x45, 0x89, 0xf4
    };
    copy_block(op_mov_32_T0_T2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_im
{
    static const uint8 op_mov_32_T0_im_code[] = {
       0x44, 0x8d, 0x25, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_mov_32_T0_im_code, 7);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_T0
{
    static const uint8 op_mov_32_T1_T0_code[] = {
       0x45, 0x89, 0xe5
    };
    copy_block(op_mov_32_T1_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_T2
{
    static const uint8 op_mov_32_T1_T2_code[] = {
       0x45, 0x89, 0xf5
    };
    copy_block(op_mov_32_T1_T2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_im
{
    static const uint8 op_mov_32_T1_im_code[] = {
       0x44, 0x8d, 0x2d, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_mov_32_T1_im_code, 7);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_T0
{
    static const uint8 op_mov_32_T2_T0_code[] = {
       0x45, 0x89, 0xe6
    };
    copy_block(op_mov_32_T2_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_T1
{
    static const uint8 op_mov_32_T2_T1_code[] = {
       0x45, 0x89, 0xee
    };
    copy_block(op_mov_32_T2_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_im
{
    static const uint8 op_mov_32_T2_im_code[] = {
       0x44, 0x8d, 0x35, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_mov_32_T2_im_code, 7);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_mov_ad_A0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_ad_A0_im
{
    static const uint8 op_mov_ad_A0_im_code[] = {
       0x49, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_mov_ad_A0_im_code, 10);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_mov_ad_A1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_ad_A1_im
{
    static const uint8 op_mov_ad_A1_im_code[] = {
       0x49, 0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_mov_ad_A1_im_code, 10);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_mov_ad_A2_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_ad_A2_im
{
    static const uint8 op_mov_ad_A2_im_code[] = {
       0x49, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_mov_ad_A2_im_code, 10);
    *(uint64_t *)(code_ptr() + 2) = (uint64_t)param1 + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_nor_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_nor_32_T0_T1
{
    static const uint8 op_nor_32_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x09, 0xe0, 0x41, 0x89, 0xc4, 0x41, 0xf7, 0xd4
    };
    copy_block(op_nor_32_T0_T1_code, 12);
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_orc_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_orc_32_T0_T1
{
    static const uint8 op_orc_32_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0xf7, 0xd0, 0x41, 0x09, 0xc4
    };
    copy_block(op_orc_32_T0_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_rol_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rol_32_T0_T1
{
    static const uint8 op_rol_32_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xc4
    };
    copy_block(op_rol_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_rol_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rol_32_T0_im
{
    static const uint8 op_rol_32_T0_im_code[] = {
       0x44, 0x89, 0xe0, 0x48, 0x8d, 0x35, 0x00, 0x00, 0x00, 0x00, 0xb9, 0x20,
       0x00, 0x00, 0x00, 0x29, 0xf1, 0x44, 0x89, 0xe2, 0xd3, 0xea, 0x89, 0xf1,
       0xd3, 0xe0, 0x41, 0x89, 0xd4, 0x41, 0x09, 0xc4
    };
    copy_block(op_rol_32_T0_im_code, 32);
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(32);
}
#endif

DEFINE_GEN(gen_op_ror_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ror_32_T0_T1
{
    static const uint8 op_ror_32_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xcc
    };
    copy_block(op_ror_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_ror_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ror_32_T0_im
{
    static const uint8 op_ror_32_T0_im_code[] = {
       0x53, 0x44, 0x89, 0xe0, 0x48, 0x8d, 0x15, 0x00, 0x00, 0x00, 0x00, 0xb9,
       0x20, 0x00, 0x00, 0x00, 0x29, 0xd1, 0x44, 0x89, 0xe6, 0xd3, 0xe6, 0x89,
       0xd1, 0xd3, 0xe8, 0x41, 0x89, 0xf4, 0x41, 0x09, 0xc4, 0x5b
    };
    copy_block(op_ror_32_T0_im_code, 34);
    *(uint32_t *)(code_ptr() + 7) = (int32_t)((long)param1 - (long)(code_ptr() + 7 + 4)) + 0;
    inc_code_ptr(34);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_T1
{
    static const uint8 op_sub_32_T0_T1_code[] = {
       0x45, 0x29, 0xec
    };
    copy_block(op_sub_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_T2
{
    static const uint8 op_sub_32_T0_T2_code[] = {
       0x45, 0x29, 0xf4
    };
    copy_block(op_sub_32_T0_T2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_im
{
    static const uint8 op_sub_32_T0_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x29, 0xc4
    };
    copy_block(op_sub_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_T0
{
    static const uint8 op_sub_32_T1_T0_code[] = {
       0x45, 0x29, 0xe5
    };
    copy_block(op_sub_32_T1_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_T2
{
    static const uint8 op_sub_32_T1_T2_code[] = {
       0x45, 0x29, 0xf5
    };
    copy_block(op_sub_32_T1_T2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_im
{
    static const uint8 op_sub_32_T1_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x29, 0xc5
    };
    copy_block(op_sub_32_T1_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_xor_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_xor_32_T0_T1
{
    static const uint8 op_xor_32_T0_T1_code[] = {
       0x45, 0x31, 0xec
    };
    copy_block(op_xor_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_xor_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_xor_32_T0_im
{
    static const uint8 op_xor_32_T0_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x31, 0xc4
    };
    copy_block(op_xor_32_T0_im_code, 10);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_andc_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_andc_32_T0_T1
{
    static const uint8 op_andc_32_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0xf7, 0xd0, 0x41, 0x21, 0xc4
    };
    copy_block(op_andc_32_T0_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_nand_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_nand_32_T0_T1
{
    static const uint8 op_nand_32_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x21, 0xe0, 0x41, 0x89, 0xc4, 0x41, 0xf7, 0xd4
    };
    copy_block(op_nand_32_T0_T1_code, 12);
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_sdiv_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sdiv_32_T0_T1
{
    static const uint8 op_sdiv_32_T0_T1_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe0, 0xc1, 0xfa, 0x1f, 0x41, 0xf7, 0xfd,
       0x41, 0x89, 0xc4
    };
    copy_block(op_sdiv_32_T0_T1_code, 15);
    inc_code_ptr(15);
}
#endif

DEFINE_GEN(gen_op_smul_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_smul_32_T0_T1
{
    static const uint8 op_smul_32_T0_T1_code[] = {
       0x45, 0x0f, 0xaf, 0xe5
    };
    copy_block(op_smul_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_udiv_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_udiv_32_T0_T1
{
    static const uint8 op_udiv_32_T0_T1_code[] = {
       0x44, 0x89, 0xe0, 0x31, 0xd2, 0x41, 0xf7, 0xf5, 0x41, 0x89, 0xc4
    };
    copy_block(op_udiv_32_T0_T1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_umul_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_umul_32_T0_T1
{
    static const uint8 op_umul_32_T0_T1_code[] = {
       0x45, 0x0f, 0xaf, 0xe5
    };
    copy_block(op_umul_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_xchg_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_xchg_32_T0_T1
{
    static const uint8 op_xchg_32_T0_T1_code[] = {
       0x45, 0x87, 0xe5
    };
    copy_block(op_xchg_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_load_s8_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s8_T0_T1_0
{
    static const uint8 op_load_s8_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x0f, 0xbe, 0x20
    };
    copy_block(op_load_s8_T0_T1_0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_u8_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u8_T0_T1_0
{
    static const uint8 op_load_u8_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x44, 0x0f, 0xb6, 0x20, 
    };
    copy_block(op_load_u8_T0_T1_0_code, 43);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(43);
}
#endif

DEFINE_GEN(gen_op_store_8_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_8_T0_T1_0
{
    static const uint8 op_store_8_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x44, 0x88, 0x20, 
    };
    copy_block(op_store_8_T0_T1_0_code, 42);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(42);
}
#endif

DEFINE_GEN(gen_op_load_s16_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s16_T0_T1_0
{
    static const uint8 op_load_s16_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x0f, 0xb7, 0x00, 
       0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xbf, 0xe0
    };
    copy_block(op_load_s16_T0_T1_0_code, 50);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(50);
}
#endif

DEFINE_GEN(gen_op_load_s32_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s32_T0_T1_0
{
    static const uint8 op_load_s32_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc
    };
    copy_block(op_load_s32_T0_T1_0_code, 47);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(47);
}
#endif

DEFINE_GEN(gen_op_load_s8_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s8_T0_T1_T2
{
    static const uint8 op_load_s8_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 0x44, 0x0f, 0xbe, 0x20
    };
    copy_block(op_load_s8_T0_T1_T2_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_load_s8_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s8_T0_T1_im
{
    static const uint8 op_load_s8_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x44, 0x0f,
       0xbe, 0x24, 0x02
    };
    copy_block(op_load_s8_T0_T1_im_code, 15);
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(15);
}
#endif

DEFINE_GEN(gen_op_load_u16_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u16_T0_T1_0
{
    static const uint8 op_load_u16_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x0f, 0xb7, 0x00, 
       0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xb7, 0xe0
    };
    copy_block(op_load_u16_T0_T1_0_code, 50);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(50);
}
#endif

DEFINE_GEN(gen_op_load_u32_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u32_T0_T1_0
{
    static const uint8 op_load_u32_T0_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc
    };
    copy_block(op_load_u32_T0_T1_0_code, 47);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(47);
}
#endif

DEFINE_GEN(gen_op_load_u8_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u8_T0_T1_T2
{
    static const uint8 op_load_u8_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x44, 0x0f, 0xb6, 0x20, 
    };
    copy_block(op_load_u8_T0_T1_T2_code, 44);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(44);
}
#endif

DEFINE_GEN(gen_op_load_u8_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u8_T0_T1_im
{
    static const uint8 op_load_u8_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x44, 0x0f, 0xb6, 0x20,
    };
    copy_block(op_load_u8_T0_T1_im_code, 52);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(52);
}
#endif

DEFINE_GEN(gen_op_store_16_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_16_T0_T1_0
{
    static const uint8 op_store_16_T0_T1_0_code[] = {
       0x44, 0x89, 0xea, 0x44, 0x89, 0xe0, 0x66, 0xc1, 0xc0, 0x08, 
       TRANS_RDX,
       0x66, 0x89, 0x02, 
    };
    copy_block(op_store_16_T0_T1_0_code, 54);
    *(uint32_t *)(code_ptr() + 38) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 47) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(54);
}
#endif

DEFINE_GEN(gen_op_store_32_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_32_T0_T1_0
{
    static const uint8 op_store_32_T0_T1_0_code[] = {
       0x44, 0x89, 0xe2, 0x0f, 0xca, 0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x89, 0x10, 
    };
    copy_block(op_store_32_T0_T1_0_code, 46);
    *(uint32_t *)(code_ptr() + 32) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 40) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(46);
}
#endif

DEFINE_GEN(gen_op_store_8_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_8_T0_T1_T2
{
    static const uint8 op_store_8_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x44, 0x88, 0x20, 
    };
    copy_block(op_store_8_T0_T1_T2_code, 43);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(43);
}
#endif

DEFINE_GEN(gen_op_store_8_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_8_T0_T1_im
{
    static const uint8 op_store_8_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x44, 0x88, 0x20,
    };
    copy_block(op_store_8_T0_T1_im_code, 51);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(51);
}
#endif

DEFINE_GEN(gen_op_load_s16_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s16_T0_T1_T2
{
    static const uint8 op_load_s16_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x0f, 0xb7, 0x00, 
       0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xbf, 0xe0
    };
    copy_block(op_load_s16_T0_T1_T2_code, 51);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(51);
}
#endif

DEFINE_GEN(gen_op_load_s16_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s16_T0_T1_im
{
    static const uint8 op_load_s16_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x0f, 0xb7, 0x00,
       0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xbf, 0xe0
    };
    copy_block(op_load_s16_T0_T1_im_code, 59);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(59);
}
#endif

DEFINE_GEN(gen_op_load_s32_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s32_T0_T1_T2
{
    static const uint8 op_load_s32_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc
    };
    copy_block(op_load_s32_T0_T1_T2_code, 48);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(48);
}
#endif

DEFINE_GEN(gen_op_load_s32_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s32_T0_T1_im
{
    static const uint8 op_load_s32_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x8b, 0x00,
       0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc
    };
    copy_block(op_load_s32_T0_T1_im_code, 56);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(56);
}
#endif

DEFINE_GEN(gen_op_load_u16_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u16_T0_T1_T2
{
    static const uint8 op_load_u16_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x0f, 0xb7, 0x00, 
       0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xb7, 0xe0
    };
    copy_block(op_load_u16_T0_T1_T2_code, 51);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(51);
}
#endif

DEFINE_GEN(gen_op_load_u16_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u16_T0_T1_im
{
    static const uint8 op_load_u16_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x0f, 0xb7, 0x00,
       0x66, 0xc1, 0xc0, 0x08, 0x44, 0x0f, 0xb7, 0xe0
    };
    copy_block(op_load_u16_T0_T1_im_code, 59);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(59);
}
#endif

DEFINE_GEN(gen_op_load_u32_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u32_T0_T1_T2
{
    static const uint8 op_load_u32_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc
    };
    copy_block(op_load_u32_T0_T1_T2_code, 48);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(48);
}
#endif

DEFINE_GEN(gen_op_load_u32_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u32_T0_T1_im
{
    static const uint8 op_load_u32_T0_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x8b, 0x00,
       0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc
    };
    copy_block(op_load_u32_T0_T1_im_code, 56);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(56);
}
#endif

DEFINE_GEN(gen_op_store_16_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_16_T0_T1_T2
{
    static const uint8 op_store_16_T0_T1_T2_code[] = {
       0x43, 0x8d, 0x14, 0x2e, 0x44, 0x89, 0xe0, 0x66, 0xc1, 0xc0, 0x08, 
       TRANS_RDX,
       0x66, 0x89, 0x02, 
    };
    copy_block(op_store_16_T0_T1_T2_code, 55);
    *(uint32_t *)(code_ptr() + 39) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 48) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(55);
}
#endif

DEFINE_GEN(gen_op_store_16_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_16_T0_T1_im
{
    static const uint8 op_store_16_T0_T1_im_code[] = {
       0x44, 0x89, 0xe9, 0x44, 0x89, 0xe2, 0x66, 0xc1, 0xc2, 0x08, 0x48, 0x8d,
       0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RCX,
       TRANS_RAX,
       0x66, 0x89, 0x10,
    };
    copy_block(op_store_16_T0_T1_im_code, 58);
    *(uint32_t *)(code_ptr() + 43) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 51) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 13) = (int32_t)((long)param1 - (long)(code_ptr() + 13 + 4)) + 0;
    inc_code_ptr(58);
}
#endif

DEFINE_GEN(gen_op_store_32_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_32_T0_T1_T2
{
    static const uint8 op_store_32_T0_T1_T2_code[] = {
       0x44, 0x89, 0xf2, 0x44, 0x89, 0xe1, 0x0f, 0xc9, 0x44, 0x01, 0xea, 
       TRANS_RDX,
       0x89, 0x0a, 
    };
    copy_block(op_store_32_T0_T1_T2_code, 54);
    *(uint32_t *)(code_ptr() + 39) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 48) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(54);
}
#endif

DEFINE_GEN(gen_op_store_32_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_32_T0_T1_im
{
    static const uint8 op_store_32_T0_T1_im_code[] = {
       0x44, 0x89, 0xe1, 0x0f, 0xc9, 0x44, 0x89, 0xe8, 0x48, 0x8d, 0x15, 0x00,
       0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x89, 0x08,
    };
    copy_block(op_store_32_T0_T1_im_code, 55);
    *(uint32_t *)(code_ptr() + 41) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 49) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 11) = (int32_t)((long)param1 - (long)(code_ptr() + 11 + 4)) + 0;
    inc_code_ptr(55);
}
#endif

DEFINE_GEN(gen_op_jmp_A0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jmp_A0
{
    static const uint8 op_jmp_A0_code[] = {
       0x41, 0xff, 0xe4
    };
    copy_block(op_jmp_A0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_CST(op_exec_return_offset,0x32L)

DEFINE_GEN(gen_op_execute,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_execute
{
    static const uint8 op_execute_code[] = {
       0x53, 0x48, 0x81, 0xec, 0xa0, 0x00, 0x00, 0x00, 0x48, 0x89, 0xfb, 0x48,
       0x89, 0xac, 0x24, 0x98, 0x00, 0x00, 0x00, 0x4c, 0x89, 0xa4, 0x24, 0x90,
       0x00, 0x00, 0x00, 0x4c, 0x89, 0xac, 0x24, 0x88, 0x00, 0x00, 0x00, 0x4c,
       0x89, 0xb4, 0x24, 0x80, 0x00, 0x00, 0x00, 0x48, 0x89, 0xf5, 0xff, 0xe7,
       0xff, 0xd3, 0x48, 0x8b, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00, 0x49, 0x89,
       0xc6, 0x48, 0x8b, 0x84, 0x24, 0x88, 0x00, 0x00, 0x00, 0x49, 0x89, 0xc5,
       0x48, 0x8b, 0x84, 0x24, 0x90, 0x00, 0x00, 0x00, 0x49, 0x89, 0xc4, 0x48,
       0x8b, 0x84, 0x24, 0x98, 0x00, 0x00, 0x00, 0x48, 0x89, 0xc5, 0x48, 0x81,
       0xc4, 0xa0, 0x00, 0x00, 0x00, 0x5b, 0xc3
    };
    copy_block(op_execute_code, 103);
    inc_code_ptr(103);
}
#endif

#undef DEFINE_CST
#undef DEFINE_GEN
