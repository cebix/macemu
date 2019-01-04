#ifndef DEFINE_CST
#define DEFINE_CST(NAME, VALUE)
#endif
DEFINE_GEN(gen_op_invoke,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke
{
    static const uint8 helper_op_invoke_code[] = {
       0xe8, 0xda, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_code, 5);
    *(uint32_t *)(code_ptr() + 1) = param1 - (long)(code_ptr() + 1) - 4;
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_invoke_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0
{
    static const uint8 helper_op_invoke_T0_code[] = {
       0x89, 0x1c, 0x24, 0xe8, 0xc9, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_T0_code, 8);
    *(uint32_t *)(code_ptr() + 4) = param1 - (long)(code_ptr() + 4) - 4;
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_invoke_T0_T1,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0_T1
{
    static const uint8 helper_op_invoke_T0_T1_code[] = {
       0x89, 0x74, 0x24, 0x04, 0x89, 0x1c, 0x24, 0xe8, 0xb5, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_T0_T1_code, 12);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_T0_T1_T2,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0_T1_T2
{
    static const uint8 helper_op_invoke_T0_T1_T2_code[] = {
       0x89, 0x7c, 0x24, 0x08, 0x89, 0x74, 0x24, 0x04, 0x89, 0x1c, 0x24, 0xe8,
       0x9d, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_T0_T1_T2_code, 16);
    *(uint32_t *)(code_ptr() + 12) = param1 - (long)(code_ptr() + 12) - 4;
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_invoke_T0_ret_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_T0_ret_T0
{
    static const uint8 helper_op_invoke_T0_ret_T0_code[] = {
       0x89, 0x1c, 0x24, 0xe8, 0x8d, 0x02, 0x00, 0x00, 0x89, 0xc3
    };
    copy_block(helper_op_invoke_T0_ret_T0_code, 10);
    *(uint32_t *)(code_ptr() + 4) = param1 - (long)(code_ptr() + 4) - 4;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_invoke_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_im
{
    static const uint8 helper_op_invoke_im_code[] = {
       0xc7, 0x04, 0x24, 0xe0, 0x04, 0x00, 0x00, 0xe8, 0x77, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_im_code, 12);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    *(uint32_t *)(code_ptr() + 3) = (param2 + 0);
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU
{
    static const uint8 helper_op_invoke_CPU_code[] = {
       0x89, 0x2c, 0x24, 0xe8, 0x67, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_CPU_code, 8);
    *(uint32_t *)(code_ptr() + 4) = param1 - (long)(code_ptr() + 4) - 4;
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_T0
{
    static const uint8 helper_op_invoke_CPU_T0_code[] = {
       0x89, 0x5c, 0x24, 0x04, 0x89, 0x2c, 0x24, 0xe8, 0x53, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_CPU_T0_code, 12);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_im
{
    static const uint8 helper_op_invoke_CPU_im_code[] = {
       0xc7, 0x44, 0x24, 0x04, 0xe0, 0x04, 0x00, 0x00, 0x89, 0x2c, 0x24, 0xe8,
       0x3b, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_CPU_im_code, 16);
    *(uint32_t *)(code_ptr() + 12) = param1 - (long)(code_ptr() + 12) - 4;
    *(uint32_t *)(code_ptr() + 4) = (param2 + 0);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_im_im,void,(long param1, long param2, long param3))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_im_im
{
    static const uint8 helper_op_invoke_CPU_im_im_code[] = {
       0xc7, 0x44, 0x24, 0x08, 0xe4, 0x04, 0x00, 0x00, 0xc7, 0x44, 0x24, 0x04,
       0xe0, 0x04, 0x00, 0x00, 0x89, 0x2c, 0x24, 0xe8, 0x1b, 0x02, 0x00, 0x00
    };
    copy_block(helper_op_invoke_CPU_im_im_code, 24);
    *(uint32_t *)(code_ptr() + 20) = param1 - (long)(code_ptr() + 20) - 4;
    *(uint32_t *)(code_ptr() + 12) = (param2 + 0);
    *(uint32_t *)(code_ptr() + 4) = (param3 + 0);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_invoke_CPU_A0_ret_A0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_CPU_A0_ret_A0
{
    static const uint8 helper_op_invoke_CPU_A0_ret_A0_code[] = {
       0x89, 0x5c, 0x24, 0x04, 0x89, 0x2c, 0x24, 0xe8, 0x07, 0x02, 0x00, 0x00,
       0x89, 0xc3
    };
    copy_block(helper_op_invoke_CPU_A0_ret_A0_code, 14);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_invoke_direct,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct
{
    static const uint8 helper_op_invoke_direct_code[] = {
       0xe8, 0xf8, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_code, 5);
    *(uint32_t *)(code_ptr() + 1) = param1 - (long)(code_ptr() + 1) - 4;
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0
{
    static const uint8 helper_op_invoke_direct_T0_code[] = {
       0x89, 0x1c, 0x24, 0xe8, 0xe7, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_T0_code, 8);
    *(uint32_t *)(code_ptr() + 4) = param1 - (long)(code_ptr() + 4) - 4;
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0_T1,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0_T1
{
    static const uint8 helper_op_invoke_direct_T0_T1_code[] = {
       0x89, 0x74, 0x24, 0x04, 0x89, 0x1c, 0x24, 0xe8, 0xd3, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_T0_T1_code, 12);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0_T1_T2,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0_T1_T2
{
    static const uint8 helper_op_invoke_direct_T0_T1_T2_code[] = {
       0x89, 0x7c, 0x24, 0x08, 0x89, 0x74, 0x24, 0x04, 0x89, 0x1c, 0x24, 0xe8,
       0xbb, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_T0_T1_T2_code, 16);
    *(uint32_t *)(code_ptr() + 12) = param1 - (long)(code_ptr() + 12) - 4;
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_T0_ret_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_T0_ret_T0
{
    static const uint8 helper_op_invoke_direct_T0_ret_T0_code[] = {
       0x89, 0x1c, 0x24, 0xe8, 0xab, 0x01, 0x00, 0x00, 0x89, 0xc3
    };
    copy_block(helper_op_invoke_direct_T0_ret_T0_code, 10);
    *(uint32_t *)(code_ptr() + 4) = param1 - (long)(code_ptr() + 4) - 4;
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_im
{
    static const uint8 helper_op_invoke_direct_im_code[] = {
       0xc7, 0x04, 0x24, 0xe0, 0x04, 0x00, 0x00, 0xe8, 0x95, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_im_code, 12);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    *(uint32_t *)(code_ptr() + 3) = (param2 + 0);
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU
{
    static const uint8 helper_op_invoke_direct_CPU_code[] = {
       0x89, 0x2c, 0x24, 0xe8, 0x85, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_code, 8);
    *(uint32_t *)(code_ptr() + 4) = param1 - (long)(code_ptr() + 4) - 4;
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_T0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_T0
{
    static const uint8 helper_op_invoke_direct_CPU_T0_code[] = {
       0x89, 0x5c, 0x24, 0x04, 0x89, 0x2c, 0x24, 0xe8, 0x71, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_T0_code, 12);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_im
{
    static const uint8 helper_op_invoke_direct_CPU_im_code[] = {
       0xc7, 0x44, 0x24, 0x04, 0xe0, 0x04, 0x00, 0x00, 0x89, 0x2c, 0x24, 0xe8,
       0x59, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_im_code, 16);
    *(uint32_t *)(code_ptr() + 12) = param1 - (long)(code_ptr() + 12) - 4;
    *(uint32_t *)(code_ptr() + 4) = (param2 + 0);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_im_im,void,(long param1, long param2, long param3))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_im_im
{
    static const uint8 helper_op_invoke_direct_CPU_im_im_code[] = {
       0xc7, 0x44, 0x24, 0x08, 0xe4, 0x04, 0x00, 0x00, 0xc7, 0x44, 0x24, 0x04,
       0xe0, 0x04, 0x00, 0x00, 0x89, 0x2c, 0x24, 0xe8, 0x39, 0x01, 0x00, 0x00
    };
    copy_block(helper_op_invoke_direct_CPU_im_im_code, 24);
    *(uint32_t *)(code_ptr() + 20) = param1 - (long)(code_ptr() + 20) - 4;
    *(uint32_t *)(code_ptr() + 12) = (param2 + 0);
    *(uint32_t *)(code_ptr() + 4) = (param3 + 0);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_invoke_direct_CPU_A0_ret_A0,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_invoke_direct_CPU_A0_ret_A0
{
    static const uint8 helper_op_invoke_direct_CPU_A0_ret_A0_code[] = {
       0x89, 0x5c, 0x24, 0x04, 0x89, 0x2c, 0x24, 0xe8, 0x25, 0x01, 0x00, 0x00,
       0x89, 0xc3
    };
    copy_block(helper_op_invoke_direct_CPU_A0_ret_A0_code, 14);
    *(uint32_t *)(code_ptr() + 8) = param1 - (long)(code_ptr() + 8) - 4;
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_jmp_fast,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jmp_fast
{
    static const uint8 op_jmp_fast_code[] = {
       0xe9, 0xe7, 0x02, 0x00, 0x00
    };
    copy_block(op_jmp_fast_code, 5);
    *(uint32_t *)(code_ptr() + 1) = param1 - (long)(code_ptr() + 1) - 4;
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_jmp_slow,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jmp_slow
{
    static const uint8 op_jmp_slow_code[] = {
       0xb8, 0xdc, 0x04, 0x00, 0x00, 0xff, 0xe0
    };
    copy_block(op_jmp_slow_code, 7);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_neg_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_neg_32_T0
{
    static const uint8 op_neg_32_T0_code[] = {
       0xf7, 0xdb
    };
    copy_block(op_neg_32_T0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_not_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_not_32_T0
{
    static const uint8 op_not_32_T0_code[] = {
       0x85, 0xdb, 0x0f, 0x94, 0xc0, 0x0f, 0xb6, 0xd8
    };
    copy_block(op_not_32_T0_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_not_32_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_not_32_T1
{
    static const uint8 op_not_32_T1_code[] = {
       0x85, 0xf6, 0x0f, 0x94, 0xc0, 0x0f, 0xb6, 0xf0
    };
    copy_block(op_not_32_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_se_8_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_se_8_32_T0
{
    static const uint8 op_se_8_32_T0_code[] = {
       0x0f, 0xbe, 0xdb
    };
    copy_block(op_se_8_32_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_ze_8_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ze_8_32_T0
{
    static const uint8 op_ze_8_32_T0_code[] = {
       0x0f, 0xb6, 0xdb
    };
    copy_block(op_ze_8_32_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_1
{
    static const uint8 op_add_32_T0_1_code[] = {
       0x43
    };
    copy_block(op_add_32_T0_1_code, 1);
    inc_code_ptr(1);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_2
{
    static const uint8 op_add_32_T0_2_code[] = {
       0x83, 0xc3, 0x02
    };
    copy_block(op_add_32_T0_2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_4
{
    static const uint8 op_add_32_T0_4_code[] = {
       0x83, 0xc3, 0x04
    };
    copy_block(op_add_32_T0_4_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_8
{
    static const uint8 op_add_32_T0_8_code[] = {
       0x83, 0xc3, 0x08
    };
    copy_block(op_add_32_T0_8_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_1
{
    static const uint8 op_add_32_T1_1_code[] = {
       0x46
    };
    copy_block(op_add_32_T1_1_code, 1);
    inc_code_ptr(1);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_2
{
    static const uint8 op_add_32_T1_2_code[] = {
       0x83, 0xc6, 0x02
    };
    copy_block(op_add_32_T1_2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_4
{
    static const uint8 op_add_32_T1_4_code[] = {
       0x83, 0xc6, 0x04
    };
    copy_block(op_add_32_T1_4_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_8
{
    static const uint8 op_add_32_T1_8_code[] = {
       0x83, 0xc6, 0x08
    };
    copy_block(op_add_32_T1_8_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_bswap_16_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_bswap_16_T0
{
    static const uint8 op_bswap_16_T0_code[] = {
       0x89, 0xd8, 0x66, 0xc1, 0xc0, 0x08, 0x0f, 0xb7, 0xd8
    };
    copy_block(op_bswap_16_T0_code, 9);
    inc_code_ptr(9);
}
#endif

DEFINE_GEN(gen_op_bswap_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_bswap_32_T0
{
    static const uint8 op_bswap_32_T0_code[] = {
       0x0f, 0xcb
    };
    copy_block(op_bswap_32_T0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_0
{
    static const uint8 op_mov_32_T0_0_code[] = {
       0x31, 0xdb
    };
    copy_block(op_mov_32_T0_0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_0
{
    static const uint8 op_mov_32_T1_0_code[] = {
       0x31, 0xf6
    };
    copy_block(op_mov_32_T1_0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_0
{
    static const uint8 op_mov_32_T2_0_code[] = {
       0x31, 0xff
    };
    copy_block(op_mov_32_T2_0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_or_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_or_32_T0_T1
{
    static const uint8 op_or_32_T0_T1_code[] = {
       0x09, 0xf3
    };
    copy_block(op_or_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_or_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_or_32_T0_im
{
    static const uint8 op_or_32_T0_im_code[] = {
       0x81, 0xcb, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_or_32_T0_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_se_16_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_se_16_32_T0
{
    static const uint8 op_se_16_32_T0_code[] = {
       0x0f, 0xbf, 0xdb
    };
    copy_block(op_se_16_32_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_se_16_32_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_se_16_32_T1
{
    static const uint8 op_se_16_32_T1_code[] = {
       0x0f, 0xbf, 0xf6
    };
    copy_block(op_se_16_32_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_1
{
    static const uint8 op_sub_32_T0_1_code[] = {
       0x4b
    };
    copy_block(op_sub_32_T0_1_code, 1);
    inc_code_ptr(1);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_2
{
    static const uint8 op_sub_32_T0_2_code[] = {
       0x83, 0xeb, 0x02
    };
    copy_block(op_sub_32_T0_2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_4
{
    static const uint8 op_sub_32_T0_4_code[] = {
       0x83, 0xeb, 0x04
    };
    copy_block(op_sub_32_T0_4_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_8
{
    static const uint8 op_sub_32_T0_8_code[] = {
       0x83, 0xeb, 0x08
    };
    copy_block(op_sub_32_T0_8_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_1
{
    static const uint8 op_sub_32_T1_1_code[] = {
       0x4e
    };
    copy_block(op_sub_32_T1_1_code, 1);
    inc_code_ptr(1);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_2
{
    static const uint8 op_sub_32_T1_2_code[] = {
       0x83, 0xee, 0x02
    };
    copy_block(op_sub_32_T1_2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_4
{
    static const uint8 op_sub_32_T1_4_code[] = {
       0x83, 0xee, 0x04
    };
    copy_block(op_sub_32_T1_4_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_8
{
    static const uint8 op_sub_32_T1_8_code[] = {
       0x83, 0xee, 0x08
    };
    copy_block(op_sub_32_T1_8_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_ze_16_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ze_16_32_T0
{
    static const uint8 op_ze_16_32_T0_code[] = {
       0x0f, 0xb7, 0xdb
    };
    copy_block(op_ze_16_32_T0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_T1
{
    static const uint8 op_add_32_T0_T1_code[] = {
       0x01, 0xf3
    };
    copy_block(op_add_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_T2
{
    static const uint8 op_add_32_T0_T2_code[] = {
       0x01, 0xfb
    };
    copy_block(op_add_32_T0_T2_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_add_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T0_im
{
    static const uint8 op_add_32_T0_im_code[] = {
       0x81, 0xc3, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_add_32_T0_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_T0
{
    static const uint8 op_add_32_T1_T0_code[] = {
       0x01, 0xde
    };
    copy_block(op_add_32_T1_T0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_T2
{
    static const uint8 op_add_32_T1_T2_code[] = {
       0x01, 0xfe
    };
    copy_block(op_add_32_T1_T2_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_add_32_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_add_32_T1_im
{
    static const uint8 op_add_32_T1_im_code[] = {
       0x81, 0xc6, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_add_32_T1_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_and_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_and_32_T0_T1
{
    static const uint8 op_and_32_T0_T1_code[] = {
       0x21, 0xf3
    };
    copy_block(op_and_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_and_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_and_32_T0_im
{
    static const uint8 op_and_32_T0_im_code[] = {
       0x81, 0xe3, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_and_32_T0_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_asr_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_asr_32_T0_T1
{
    static const uint8 op_asr_32_T0_T1_code[] = {
       0x89, 0xf1, 0xd3, 0xfb
    };
    copy_block(op_asr_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_asr_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_asr_32_T0_im
{
    static const uint8 op_asr_32_T0_im_code[] = {
       0xb9, 0xdc, 0x04, 0x00, 0x00, 0xd3, 0xfb
    };
    copy_block(op_asr_32_T0_im_code, 7);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_eqv_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_eqv_32_T0_T1
{
    static const uint8 op_eqv_32_T0_T1_code[] = {
       0x89, 0xd8, 0x31, 0xf0, 0x89, 0xc3, 0xf7, 0xd3
    };
    copy_block(op_eqv_32_T0_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_lsl_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsl_32_T0_T1
{
    static const uint8 op_lsl_32_T0_T1_code[] = {
       0x89, 0xf1, 0xd3, 0xe3
    };
    copy_block(op_lsl_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_lsl_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsl_32_T0_im
{
    static const uint8 op_lsl_32_T0_im_code[] = {
       0xb9, 0xdc, 0x04, 0x00, 0x00, 0xd3, 0xe3
    };
    copy_block(op_lsl_32_T0_im_code, 7);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_lsr_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsr_32_T0_T1
{
    static const uint8 op_lsr_32_T0_T1_code[] = {
       0x89, 0xf1, 0xd3, 0xeb
    };
    copy_block(op_lsr_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_lsr_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lsr_32_T0_im
{
    static const uint8 op_lsr_32_T0_im_code[] = {
       0xb9, 0xdc, 0x04, 0x00, 0x00, 0xd3, 0xeb
    };
    copy_block(op_lsr_32_T0_im_code, 7);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_T1
{
    static const uint8 op_mov_32_T0_T1_code[] = {
       0x89, 0xf3
    };
    copy_block(op_mov_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_T2
{
    static const uint8 op_mov_32_T0_T2_code[] = {
       0x89, 0xfb
    };
    copy_block(op_mov_32_T0_T2_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T0_im
{
    static const uint8 op_mov_32_T0_im_code[] = {
       0xbb, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_mov_32_T0_im_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_T0
{
    static const uint8 op_mov_32_T1_T0_code[] = {
       0x89, 0xde
    };
    copy_block(op_mov_32_T1_T0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_T2
{
    static const uint8 op_mov_32_T1_T2_code[] = {
       0x89, 0xfe
    };
    copy_block(op_mov_32_T1_T2_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T1_im
{
    static const uint8 op_mov_32_T1_im_code[] = {
       0xbe, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_mov_32_T1_im_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_T0
{
    static const uint8 op_mov_32_T2_T0_code[] = {
       0x89, 0xdf
    };
    copy_block(op_mov_32_T2_T0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_T1
{
    static const uint8 op_mov_32_T2_T1_code[] = {
       0x89, 0xf7
    };
    copy_block(op_mov_32_T2_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_mov_32_T2_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_32_T2_im
{
    static const uint8 op_mov_32_T2_im_code[] = {
       0xbf, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_mov_32_T2_im_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_mov_ad_A0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_ad_A0_im
{
    static const uint8 op_mov_ad_A0_im_code[] = {
       0xbb, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_mov_ad_A0_im_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_mov_ad_A1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_ad_A1_im
{
    static const uint8 op_mov_ad_A1_im_code[] = {
       0xbe, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_mov_ad_A1_im_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_mov_ad_A2_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mov_ad_A2_im
{
    static const uint8 op_mov_ad_A2_im_code[] = {
       0xbf, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_mov_ad_A2_im_code, 5);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_nor_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_nor_32_T0_T1
{
    static const uint8 op_nor_32_T0_T1_code[] = {
       0x89, 0xd8, 0x09, 0xf0, 0x89, 0xc3, 0xf7, 0xd3
    };
    copy_block(op_nor_32_T0_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_orc_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_orc_32_T0_T1
{
    static const uint8 op_orc_32_T0_T1_code[] = {
       0x89, 0xf0, 0xf7, 0xd0, 0x09, 0xc3
    };
    copy_block(op_orc_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_rol_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rol_32_T0_T1
{
    static const uint8 op_rol_32_T0_T1_code[] = {
       0x89, 0xf1, 0xd3, 0xc3
    };
    copy_block(op_rol_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_rol_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rol_32_T0_im
{
    static const uint8 op_rol_32_T0_im_code[] = {
       0xb9, 0xdc, 0x04, 0x00, 0x00, 0xd3, 0xc3
    };
    copy_block(op_rol_32_T0_im_code, 7);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_ror_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ror_32_T0_T1
{
    static const uint8 op_ror_32_T0_T1_code[] = {
       0x89, 0xf1, 0xd3, 0xcb
    };
    copy_block(op_ror_32_T0_T1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_ror_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_ror_32_T0_im
{
    static const uint8 op_ror_32_T0_im_code[] = {
       0xb9, 0xdc, 0x04, 0x00, 0x00, 0xd3, 0xcb
    };
    copy_block(op_ror_32_T0_im_code, 7);
    *(uint32_t *)(code_ptr() + 1) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_T1
{
    static const uint8 op_sub_32_T0_T1_code[] = {
       0x29, 0xf3
    };
    copy_block(op_sub_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_T2
{
    static const uint8 op_sub_32_T0_T2_code[] = {
       0x29, 0xfb
    };
    copy_block(op_sub_32_T0_T2_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_sub_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T0_im
{
    static const uint8 op_sub_32_T0_im_code[] = {
       0x81, 0xeb, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_sub_32_T0_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_T0
{
    static const uint8 op_sub_32_T1_T0_code[] = {
       0x29, 0xde
    };
    copy_block(op_sub_32_T1_T0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_T2
{
    static const uint8 op_sub_32_T1_T2_code[] = {
       0x29, 0xfe
    };
    copy_block(op_sub_32_T1_T2_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_sub_32_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sub_32_T1_im
{
    static const uint8 op_sub_32_T1_im_code[] = {
       0x81, 0xee, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_sub_32_T1_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_xor_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_xor_32_T0_T1
{
    static const uint8 op_xor_32_T0_T1_code[] = {
       0x31, 0xf3
    };
    copy_block(op_xor_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_xor_32_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_xor_32_T0_im
{
    static const uint8 op_xor_32_T0_im_code[] = {
       0x81, 0xf3, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_xor_32_T0_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_andc_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_andc_32_T0_T1
{
    static const uint8 op_andc_32_T0_T1_code[] = {
       0x89, 0xf0, 0xf7, 0xd0, 0x21, 0xc3
    };
    copy_block(op_andc_32_T0_T1_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_nand_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_nand_32_T0_T1
{
    static const uint8 op_nand_32_T0_T1_code[] = {
       0x89, 0xd8, 0x21, 0xf0, 0x89, 0xc3, 0xf7, 0xd3
    };
    copy_block(op_nand_32_T0_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_sdiv_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sdiv_32_T0_T1
{
    static const uint8 op_sdiv_32_T0_T1_code[] = {
       0x89, 0xda, 0x89, 0xd8, 0xc1, 0xfa, 0x1f, 0xf7, 0xfe, 0x89, 0xc3
    };
    copy_block(op_sdiv_32_T0_T1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_smul_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_smul_32_T0_T1
{
    static const uint8 op_smul_32_T0_T1_code[] = {
       0x0f, 0xaf, 0xde
    };
    copy_block(op_smul_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_udiv_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_udiv_32_T0_T1
{
    static const uint8 op_udiv_32_T0_T1_code[] = {
       0x89, 0xd8, 0x31, 0xd2, 0xf7, 0xf6, 0x89, 0xc3
    };
    copy_block(op_udiv_32_T0_T1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_umul_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_umul_32_T0_T1
{
    static const uint8 op_umul_32_T0_T1_code[] = {
       0x0f, 0xaf, 0xde
    };
    copy_block(op_umul_32_T0_T1_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_xchg_32_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_xchg_32_T0_T1
{
    static const uint8 op_xchg_32_T0_T1_code[] = {
       0x87, 0xde
    };
    copy_block(op_xchg_32_T0_T1_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_load_s8_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s8_T0_T1_0
{
    static const uint8 op_load_s8_T0_T1_0_code[] = {
       0x0f, 0xbe, 0x1e
    };
    copy_block(op_load_s8_T0_T1_0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_load_u8_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u8_T0_T1_0
{
    static const uint8 op_load_u8_T0_T1_0_code[] = {
       0x0f, 0xb6, 0x1e
    };
    copy_block(op_load_u8_T0_T1_0_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_store_8_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_8_T0_T1_0
{
    static const uint8 op_store_8_T0_T1_0_code[] = {
       0x88, 0x1e
    };
    copy_block(op_store_8_T0_T1_0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_load_s16_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s16_T0_T1_0
{
    static const uint8 op_load_s16_T0_T1_0_code[] = {
       0x0f, 0xb7, 0x06, 0x66, 0xc1, 0xc0, 0x08, 0x0f, 0xbf, 0xd8
    };
    copy_block(op_load_s16_T0_T1_0_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_load_s32_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s32_T0_T1_0
{
    static const uint8 op_load_s32_T0_T1_0_code[] = {
       0x8b, 0x06, 0x89, 0xc3, 0x0f, 0xcb
    };
    copy_block(op_load_s32_T0_T1_0_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_load_s8_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s8_T0_T1_T2
{
    static const uint8 op_load_s8_T0_T1_T2_code[] = {
       0x0f, 0xbe, 0x1c, 0x3e
    };
    copy_block(op_load_s8_T0_T1_T2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_s8_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s8_T0_T1_im
{
    static const uint8 op_load_s8_T0_T1_im_code[] = {
       0x0f, 0xbe, 0x9e, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_load_s8_T0_T1_im_code, 7);
    *(uint32_t *)(code_ptr() + 3) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_u16_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u16_T0_T1_0
{
    static const uint8 op_load_u16_T0_T1_0_code[] = {
       0x0f, 0xb7, 0x06, 0x66, 0xc1, 0xc0, 0x08, 0x0f, 0xb7, 0xd8
    };
    copy_block(op_load_u16_T0_T1_0_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_load_u32_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u32_T0_T1_0
{
    static const uint8 op_load_u32_T0_T1_0_code[] = {
       0x8b, 0x06, 0x89, 0xc3, 0x0f, 0xcb
    };
    copy_block(op_load_u32_T0_T1_0_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_load_u8_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u8_T0_T1_T2
{
    static const uint8 op_load_u8_T0_T1_T2_code[] = {
       0x0f, 0xb6, 0x1c, 0x3e
    };
    copy_block(op_load_u8_T0_T1_T2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_u8_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u8_T0_T1_im
{
    static const uint8 op_load_u8_T0_T1_im_code[] = {
       0x0f, 0xb6, 0x9e, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_load_u8_T0_T1_im_code, 7);
    *(uint32_t *)(code_ptr() + 3) = (param1 + 0);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_16_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_16_T0_T1_0
{
    static const uint8 op_store_16_T0_T1_0_code[] = {
       0x89, 0xd8, 0x66, 0xc1, 0xc0, 0x08, 0x66, 0x89, 0x06
    };
    copy_block(op_store_16_T0_T1_0_code, 9);
    inc_code_ptr(9);
}
#endif

DEFINE_GEN(gen_op_store_32_T0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_32_T0_T1_0
{
    static const uint8 op_store_32_T0_T1_0_code[] = {
       0x89, 0xd8, 0x0f, 0xc8, 0x89, 0x06
    };
    copy_block(op_store_32_T0_T1_0_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_store_8_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_8_T0_T1_T2
{
    static const uint8 op_store_8_T0_T1_T2_code[] = {
       0x88, 0x1c, 0x3e
    };
    copy_block(op_store_8_T0_T1_T2_code, 3);
    inc_code_ptr(3);
}
#endif

DEFINE_GEN(gen_op_store_8_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_8_T0_T1_im
{
    static const uint8 op_store_8_T0_T1_im_code[] = {
       0x88, 0x9e, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_store_8_T0_T1_im_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_load_s16_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s16_T0_T1_T2
{
    static const uint8 op_load_s16_T0_T1_T2_code[] = {
       0x0f, 0xb7, 0x04, 0x3e, 0x66, 0xc1, 0xc0, 0x08, 0x0f, 0xbf, 0xd8
    };
    copy_block(op_load_s16_T0_T1_T2_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_load_s16_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s16_T0_T1_im
{
    static const uint8 op_load_s16_T0_T1_im_code[] = {
       0x0f, 0xb7, 0x86, 0xdc, 0x04, 0x00, 0x00, 0x66, 0xc1, 0xc0, 0x08, 0x0f,
       0xbf, 0xd8
    };
    copy_block(op_load_s16_T0_T1_im_code, 14);
    *(uint32_t *)(code_ptr() + 3) = (param1 + 0);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_s32_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s32_T0_T1_T2
{
    static const uint8 op_load_s32_T0_T1_T2_code[] = {
       0x8b, 0x04, 0x3e, 0x89, 0xc3, 0x0f, 0xcb
    };
    copy_block(op_load_s32_T0_T1_T2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_s32_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_s32_T0_T1_im
{
    static const uint8 op_load_s32_T0_T1_im_code[] = {
       0x8b, 0x86, 0xdc, 0x04, 0x00, 0x00, 0x89, 0xc3, 0x0f, 0xcb
    };
    copy_block(op_load_s32_T0_T1_im_code, 10);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_load_u16_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u16_T0_T1_T2
{
    static const uint8 op_load_u16_T0_T1_T2_code[] = {
       0x0f, 0xb7, 0x04, 0x3e, 0x66, 0xc1, 0xc0, 0x08, 0x0f, 0xb7, 0xd8
    };
    copy_block(op_load_u16_T0_T1_T2_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_load_u16_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u16_T0_T1_im
{
    static const uint8 op_load_u16_T0_T1_im_code[] = {
       0x0f, 0xb7, 0x86, 0xdc, 0x04, 0x00, 0x00, 0x66, 0xc1, 0xc0, 0x08, 0x0f,
       0xb7, 0xd8
    };
    copy_block(op_load_u16_T0_T1_im_code, 14);
    *(uint32_t *)(code_ptr() + 3) = (param1 + 0);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_u32_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u32_T0_T1_T2
{
    static const uint8 op_load_u32_T0_T1_T2_code[] = {
       0x8b, 0x04, 0x3e, 0x89, 0xc3, 0x0f, 0xcb
    };
    copy_block(op_load_u32_T0_T1_T2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_u32_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_u32_T0_T1_im
{
    static const uint8 op_load_u32_T0_T1_im_code[] = {
       0x8b, 0x86, 0xdc, 0x04, 0x00, 0x00, 0x89, 0xc3, 0x0f, 0xcb
    };
    copy_block(op_load_u32_T0_T1_im_code, 10);
    *(uint32_t *)(code_ptr() + 2) = (param1 + 0);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_16_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_16_T0_T1_T2
{
    static const uint8 op_store_16_T0_T1_T2_code[] = {
       0x89, 0xd8, 0x66, 0xc1, 0xc0, 0x08, 0x66, 0x89, 0x04, 0x3e
    };
    copy_block(op_store_16_T0_T1_T2_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_16_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_16_T0_T1_im
{
    static const uint8 op_store_16_T0_T1_im_code[] = {
       0x89, 0xd8, 0x66, 0xc1, 0xc0, 0x08, 0x66, 0x89, 0x86, 0xdc, 0x04, 0x00,
       0x00
    };
    copy_block(op_store_16_T0_T1_im_code, 13);
    *(uint32_t *)(code_ptr() + 9) = (param1 + 0);
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_store_32_T0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_32_T0_T1_T2
{
    static const uint8 op_store_32_T0_T1_T2_code[] = {
       0x89, 0xd8, 0x0f, 0xc8, 0x89, 0x04, 0x3e
    };
    copy_block(op_store_32_T0_T1_T2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_32_T0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_32_T0_T1_im
{
    static const uint8 op_store_32_T0_T1_im_code[] = {
       0x89, 0xd8, 0x0f, 0xc8, 0x89, 0x86, 0xdc, 0x04, 0x00, 0x00
    };
    copy_block(op_store_32_T0_T1_im_code, 10);
    *(uint32_t *)(code_ptr() + 6) = (param1 + 0);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_jmp_A0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jmp_A0
{
    static const uint8 op_jmp_A0_code[] = {
       0xff, 0xe3
    };
    copy_block(op_jmp_A0_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_CST(op_exec_return_offset,0x22L)

DEFINE_GEN(gen_op_execute,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_execute
{
    static const uint8 op_execute_code[] = {
       0x83, 0xec, 0x5c, 0x89, 0x6c, 0x24, 0x4c, 0x8b, 0x6c, 0x24, 0x64, 0x89,
       0x5c, 0x24, 0x48, 0x89, 0x74, 0x24, 0x44, 0x89, 0x7c, 0x24, 0x40, 0xff,
       0x64, 0x24, 0x60, 0xff, 0x54, 0x24, 0x60, 0x0f, 0xa6, 0xf0, 0x8b, 0x44,
       0x24, 0x40, 0x89, 0xc7, 0x8b, 0x44, 0x24, 0x44, 0x89, 0xc6, 0x8b, 0x44,
       0x24, 0x48, 0x89, 0xc3, 0x8b, 0x44, 0x24, 0x4c, 0x83, 0xc4, 0x5c, 0x89,
       0xc5, 0xc3
    };
    copy_block(op_execute_code, 62);
    inc_code_ptr(62);
}
#endif

#undef DEFINE_CST
#undef DEFINE_GEN
