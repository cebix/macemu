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
DEFINE_GEN(gen_op_dcbz_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_dcbz_T0
{
    static const uint8 op_dcbz_T0_code[] = {
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0xe0, 0x41, 0x89, 0xc4, 0x89, 0xc0, 0x48,
       0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0xc7, 0x40, 0x08, 0x00, 0x00,
       0x00, 0x00, 0x48, 0xc7, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x48, 0xc7,
       0x40, 0x18, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_dcbz_T0_code, 42);
    inc_code_ptr(42);
}
#endif

DEFINE_GEN(gen_op_mmx_vor,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vor
{
    static const uint8 op_mmx_vor_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xeb, 0x45, 0x00, 0x41, 0x0f, 0xeb, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vor_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_nego_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_nego_T0
{
    static const uint8 op_nego_T0_code[] = {
       0x31, 0xd2, 0x41, 0x81, 0xfc, 0x00, 0x00, 0x00, 0x80, 0x0f, 0x94, 0xc2,
       0x48, 0x8d, 0x45, 0x10, 0x88, 0x90, 0x85, 0x03, 0x00, 0x00, 0x08, 0x90,
       0x84, 0x03, 0x00, 0x00, 0x41, 0xf7, 0xdc
    };
    copy_block(op_nego_T0_code, 31);
    inc_code_ptr(31);
}
#endif

DEFINE_GEN(gen_op_addme_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addme_T0
{
    static const uint8 op_addme_T0_code[] = {
       0x0f, 0xb6, 0x85, 0x96, 0x03, 0x00, 0x00, 0x0f, 0xba, 0xe0, 0x00, 0x41,
       0x83, 0xd4, 0xff, 0x0f, 0x92, 0xc0, 0x88, 0x85, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_addme_T0_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_addze_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addze_T0
{
    static const uint8 op_addze_T0_code[] = {
       0x0f, 0xb6, 0x85, 0x96, 0x03, 0x00, 0x00, 0x0f, 0xba, 0xe0, 0x00, 0x41,
       0x83, 0xd4, 0x00, 0x0f, 0x92, 0xc0, 0x88, 0x85, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_addze_T0_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_mmx_vand,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vand
{
    static const uint8 op_mmx_vand_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xdb, 0x45, 0x00, 0x41, 0x0f, 0xdb, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vand_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vxor,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vxor
{
    static const uint8 op_mmx_vxor_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xef, 0x45, 0x00, 0x41, 0x0f, 0xef, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vxor_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_addmeo_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addmeo_T0
{
    static const uint8 op_addmeo_T0_code[] = {
       0x48, 0x89, 0xe8, 0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x0f, 0xba,
       0xe2, 0x00, 0x41, 0x83, 0xd4, 0xff, 0x0f, 0x92, 0xc2, 0x0f, 0x90, 0xc1,
       0x88, 0x95, 0x96, 0x03, 0x00, 0x00, 0x48, 0x83, 0xc0, 0x10, 0x88, 0x88,
       0x85, 0x03, 0x00, 0x00, 0x08, 0x88, 0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_addmeo_T0_code, 46);
    inc_code_ptr(46);
}
#endif

DEFINE_GEN(gen_op_addzeo_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addzeo_T0
{
    static const uint8 op_addzeo_T0_code[] = {
       0x48, 0x89, 0xe8, 0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x0f, 0xba,
       0xe2, 0x00, 0x41, 0x83, 0xd4, 0x00, 0x0f, 0x92, 0xc2, 0x0f, 0x90, 0xc1,
       0x88, 0x95, 0x96, 0x03, 0x00, 0x00, 0x48, 0x83, 0xc0, 0x10, 0x88, 0x88,
       0x85, 0x03, 0x00, 0x00, 0x08, 0x88, 0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_addzeo_T0_code, 46);
    inc_code_ptr(46);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_26
{
    static const uint8 op_lmw_T0_26_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x45,
       0x78, 0x8d, 0x42, 0x04, 0x41, 0x89, 0xc4, 0x89, 0xc0, 0x8b, 0x00, 0x0f,
       0xc8, 0x89, 0x45, 0x7c, 0x8d, 0x42, 0x08, 0x41, 0x89, 0xc4, 0x89, 0xc0,
       0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85, 0x80, 0x00, 0x00, 0x00, 0x8d, 0x42,
       0x0c, 0x41, 0x89, 0xc4, 0x89, 0xc0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85,
       0x84, 0x00, 0x00, 0x00, 0x8d, 0x42, 0x10, 0x41, 0x89, 0xc4, 0x89, 0xc0,
       0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85, 0x88, 0x00, 0x00, 0x00, 0x83, 0xc2,
       0x14, 0x41, 0x89, 0xd4, 0x89, 0xd2, 0x8b, 0x02, 0x0f, 0xc8, 0x89, 0x85,
       0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_lmw_T0_26_code, 100);
    inc_code_ptr(100);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_27
{
    static const uint8 op_lmw_T0_27_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x45,
       0x7c, 0x8d, 0x42, 0x04, 0x41, 0x89, 0xc4, 0x89, 0xc0, 0x8b, 0x00, 0x0f,
       0xc8, 0x89, 0x85, 0x80, 0x00, 0x00, 0x00, 0x8d, 0x42, 0x08, 0x41, 0x89,
       0xc4, 0x89, 0xc0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85, 0x84, 0x00, 0x00,
       0x00, 0x8d, 0x42, 0x0c, 0x41, 0x89, 0xc4, 0x89, 0xc0, 0x8b, 0x00, 0x0f,
       0xc8, 0x89, 0x85, 0x88, 0x00, 0x00, 0x00, 0x83, 0xc2, 0x10, 0x41, 0x89,
       0xd4, 0x89, 0xd2, 0x8b, 0x02, 0x0f, 0xc8, 0x89, 0x85, 0x8c, 0x00, 0x00,
       0x00
    };
    copy_block(op_lmw_T0_27_code, 85);
    inc_code_ptr(85);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_28
{
    static const uint8 op_lmw_T0_28_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85,
       0x80, 0x00, 0x00, 0x00, 0x8d, 0x42, 0x04, 0x41, 0x89, 0xc4, 0x89, 0xc0,
       0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85, 0x84, 0x00, 0x00, 0x00, 0x8d, 0x42,
       0x08, 0x41, 0x89, 0xc4, 0x89, 0xc0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85,
       0x88, 0x00, 0x00, 0x00, 0x83, 0xc2, 0x0c, 0x41, 0x89, 0xd4, 0x89, 0xd2,
       0x8b, 0x02, 0x0f, 0xc8, 0x89, 0x85, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_lmw_T0_28_code, 70);
    inc_code_ptr(70);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_29
{
    static const uint8 op_lmw_T0_29_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85,
       0x84, 0x00, 0x00, 0x00, 0x8d, 0x42, 0x04, 0x41, 0x89, 0xc4, 0x89, 0xc0,
       0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85, 0x88, 0x00, 0x00, 0x00, 0x83, 0xc2,
       0x08, 0x41, 0x89, 0xd4, 0x89, 0xd2, 0x8b, 0x02, 0x0f, 0xc8, 0x89, 0x85,
       0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_lmw_T0_29_code, 52);
    inc_code_ptr(52);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_30
{
    static const uint8 op_lmw_T0_30_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85,
       0x88, 0x00, 0x00, 0x00, 0x83, 0xc2, 0x04, 0x41, 0x89, 0xd4, 0x89, 0xd2,
       0x8b, 0x02, 0x0f, 0xc8, 0x89, 0x85, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_lmw_T0_30_code, 34);
    inc_code_ptr(34);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_31
{
    static const uint8 op_lmw_T0_31_code[] = {
       0x44, 0x89, 0xe0, 0x8b, 0x00, 0x0f, 0xc8, 0x89, 0x85, 0x8c, 0x00, 0x00,
       0x00
    };
    copy_block(op_lmw_T0_31_code, 13);
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_lmw_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lmw_T0_im
{
    static const uint8 op_lmw_T0_im_code[] = {
       0x8d, 0x35, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89, 0xe1, 0x48, 0x8d, 0x7d,
       0x10, 0xeb, 0x11, 0x89, 0xc8, 0x8b, 0x00, 0x0f, 0xc8, 0x48, 0x63, 0xd6,
       0x89, 0x04, 0x97, 0xff, 0xc6, 0x83, 0xc1, 0x04, 0x83, 0xfe, 0x1f, 0x76,
       0xea
    };
    copy_block(op_lmw_T0_im_code, 37);
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param1 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(37);
}
#endif

DEFINE_GEN(gen_op_mfvscr_VD,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mfvscr_VD
{
    static const uint8 op_mfvscr_VD_code[] = {
       0x41, 0xc7, 0x07, 0x00, 0x00, 0x00, 0x00, 0x41, 0xc7, 0x47, 0x04, 0x00,
       0x00, 0x00, 0x00, 0x41, 0xc7, 0x47, 0x08, 0x00, 0x00, 0x00, 0x00, 0x8b,
       0x85, 0x98, 0x03, 0x00, 0x00, 0x41, 0x89, 0x47, 0x0c
    };
    copy_block(op_mfvscr_VD_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_mmx_vandc,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vandc
{
    static const uint8 op_mmx_vandc_code[] = {
       0x41, 0x0f, 0x6f, 0x45, 0x00, 0x41, 0x0f, 0x6f, 0x4d, 0x08, 0x41, 0x0f,
       0xdf, 0x04, 0x24, 0x41, 0x0f, 0xdf, 0x4c, 0x24, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vandc_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mtvscr_V0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mtvscr_V0
{
    static const uint8 op_mtvscr_V0_code[] = {
       0x41, 0x8b, 0x44, 0x24, 0x0c, 0x89, 0x85, 0x98, 0x03, 0x00, 0x00
    };
    copy_block(op_mtvscr_V0_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_set_PC_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_set_PC_T0
{
    static const uint8 op_set_PC_T0_code[] = {
       0x44, 0x89, 0xa5, 0xac, 0x03, 0x00, 0x00
    };
    copy_block(op_set_PC_T0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_set_PC_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_set_PC_im
{
    static const uint8 op_set_PC_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0x85, 0xac, 0x03, 0x00,
       0x00
    };
    copy_block(op_set_PC_im_code, 13);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_slw_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_slw_T0_T1
{
    static const uint8 op_slw_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xe4, 0x31, 0xc0, 0x83, 0xe1, 0x20, 0x41,
       0x0f, 0x44, 0xc4, 0x41, 0x89, 0xc4
    };
    copy_block(op_slw_T0_T1_code, 18);
    inc_code_ptr(18);
}
#endif

DEFINE_GEN(gen_op_srw_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_srw_T0_T1
{
    static const uint8 op_srw_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x41, 0xd3, 0xec, 0x31, 0xc0, 0x83, 0xe1, 0x20, 0x41,
       0x0f, 0x44, 0xc4, 0x41, 0x89, 0xc4
    };
    copy_block(op_srw_T0_T1_code, 18);
    inc_code_ptr(18);
}
#endif

DEFINE_GEN(gen_op_subfme_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfme_T0
{
    static const uint8 op_subfme_T0_code[] = {
       0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0xb8, 0xff, 0xff, 0xff, 0xff,
       0x0f, 0xba, 0xe2, 0x00, 0xf5, 0x44, 0x19, 0xe0, 0xf5, 0x0f, 0x92, 0xc2,
       0x41, 0x89, 0xc4, 0x88, 0x95, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_subfme_T0_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_subfze_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfze_T0
{
    static const uint8 op_subfze_T0_code[] = {
       0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x31, 0xc0, 0x0f, 0xba, 0xe2,
       0x00, 0xf5, 0x44, 0x19, 0xe0, 0xf5, 0x0f, 0x92, 0xc2, 0x41, 0x89, 0xc4,
       0x88, 0x95, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_subfze_T0_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_addc_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addc_T0_T1
{
    static const uint8 op_addc_T0_T1_code[] = {
       0x45, 0x01, 0xec, 0x0f, 0x92, 0xc0, 0x88, 0x85, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_addc_T0_T1_code, 12);
    inc_code_ptr(12);
}
#endif

DEFINE_GEN(gen_op_addc_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addc_T0_im
{
    static const uint8 op_addc_T0_im_code[] = {
       0x44, 0x89, 0xe2, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x04,
       0x02, 0x41, 0x39, 0xc4, 0x0f, 0x97, 0x85, 0x96, 0x03, 0x00, 0x00, 0x41,
       0x89, 0xc4
    };
    copy_block(op_addc_T0_im_code, 26);
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_adde_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_adde_T0_T1
{
    static const uint8 op_adde_T0_T1_code[] = {
       0x0f, 0xb6, 0x85, 0x96, 0x03, 0x00, 0x00, 0x0f, 0xba, 0xe0, 0x00, 0x45,
       0x11, 0xec, 0x0f, 0x92, 0xc0, 0x88, 0x85, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_adde_T0_T1_code, 23);
    inc_code_ptr(23);
}
#endif

DEFINE_GEN(gen_op_addo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addo_T0_T1
{
    static const uint8 op_addo_T0_T1_code[] = {
       0x45, 0x01, 0xec, 0x0f, 0x90, 0xc2, 0x48, 0x8d, 0x45, 0x10, 0x88, 0x90,
       0x85, 0x03, 0x00, 0x00, 0x08, 0x90, 0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_addo_T0_T1_code, 22);
    inc_code_ptr(22);
}
#endif

DEFINE_GEN(gen_op_divw_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_divw_T0_T1
{
    static const uint8 op_divw_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x89, 0xe2, 0x45, 0x85, 0xed, 0x74, 0x0f, 0x41,
       0x81, 0xfc, 0x00, 0x00, 0x00, 0x80, 0x75, 0x0d, 0x41, 0x83, 0xfd, 0xff,
       0x75, 0x07, 0x89, 0xd0, 0xc1, 0xf8, 0x1f, 0xeb, 0x09, 0x89, 0xc1, 0x89,
       0xd0, 0xc1, 0xfa, 0x1f, 0xf7, 0xf9, 0x41, 0x89, 0xc4
    };
    copy_block(op_divw_T0_T1_code, 45);
    inc_code_ptr(45);
}
#endif

DEFINE_GEN(gen_op_fabs_FD_F0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fabs_FD_F0
{
    static const uint8 op_fabs_FD_F0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0xba, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
       0xff, 0x7f, 0x48, 0x21, 0xd0, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fabs_FD_F0_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_fmov_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_F0_F1
{
    static const uint8 op_fmov_F0_F1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x49, 0x89, 0x04, 0x24
    };
    copy_block(op_fmov_F0_F1_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_fmov_F0_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_F0_F2
{
    static const uint8 op_fmov_F0_F2_code[] = {
       0x49, 0x8b, 0x06, 0x49, 0x89, 0x04, 0x24
    };
    copy_block(op_fmov_F0_F2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_fmov_F1_F0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_F1_F0
{
    static const uint8 op_fmov_F1_F0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x49, 0x89, 0x45, 0x00
    };
    copy_block(op_fmov_F1_F0_code, 8);
    inc_code_ptr(8);
}
#endif

DEFINE_GEN(gen_op_fmov_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_F1_F2
{
    static const uint8 op_fmov_F1_F2_code[] = {
       0x49, 0x8b, 0x06, 0x49, 0x89, 0x45, 0x00
    };
    copy_block(op_fmov_F1_F2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_fmov_F2_F0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_F2_F0
{
    static const uint8 op_fmov_F2_F0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x49, 0x89, 0x06
    };
    copy_block(op_fmov_F2_F0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_fmov_F2_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_F2_F1
{
    static const uint8 op_fmov_F2_F1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x49, 0x89, 0x06
    };
    copy_block(op_fmov_F2_F1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_fmov_FD_F0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_FD_F0
{
    static const uint8 op_fmov_FD_F0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmov_FD_F0_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_fmov_FD_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_FD_F1
{
    static const uint8 op_fmov_FD_F1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmov_FD_F1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_fmov_FD_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmov_FD_F2
{
    static const uint8 op_fmov_FD_F2_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmov_FD_F2_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_fneg_FD_F0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fneg_FD_F0
{
    static const uint8 op_fneg_FD_F0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x80, 0x48, 0x31, 0xc8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fneg_FD_F0_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_inc_32_mem,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_inc_32_mem
{
    static const uint8 op_inc_32_mem_code[] = {
       0xff, 0x05, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_inc_32_mem_code, 6);
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param1 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_load_T0_CR,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_CR
{
    static const uint8 op_load_T0_CR_code[] = {
       0x44, 0x8b, 0xa5, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_load_T0_CR_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_LR,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_LR
{
    static const uint8 op_load_T0_LR_code[] = {
       0x44, 0x8b, 0xa5, 0xa4, 0x03, 0x00, 0x00
    };
    copy_block(op_load_T0_LR_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_PC,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_PC
{
    static const uint8 op_load_T0_PC_code[] = {
       0x44, 0x8b, 0xa5, 0xac, 0x03, 0x00, 0x00
    };
    copy_block(op_load_T0_PC_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_mmx_vmaxsh,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vmaxsh
{
    static const uint8 op_mmx_vmaxsh_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xee, 0x45, 0x00, 0x41, 0x0f, 0xee, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vmaxsh_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vmaxub,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vmaxub
{
    static const uint8 op_mmx_vmaxub_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xde, 0x45, 0x00, 0x41, 0x0f, 0xde, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vmaxub_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vminsh,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vminsh
{
    static const uint8 op_mmx_vminsh_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xea, 0x45, 0x00, 0x41, 0x0f, 0xea, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vminsh_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vminub,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vminub
{
    static const uint8 op_mmx_vminub_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xda, 0x45, 0x00, 0x41, 0x0f, 0xda, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vminub_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_record_cr1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_record_cr1
{
    static const uint8 op_record_cr1_code[] = {
       0x8b, 0x95, 0xa0, 0x03, 0x00, 0x00, 0xc1, 0xea, 0x04, 0x81, 0xe2, 0x00,
       0x00, 0x00, 0x0f, 0x8b, 0x8d, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe1, 0xff,
       0xff, 0xff, 0xf0, 0x09, 0xca, 0x89, 0x95, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_record_cr1_code, 35);
    inc_code_ptr(35);
}
#endif

DEFINE_GEN(gen_op_sraw_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sraw_T0_T1
{
    static const uint8 op_sraw_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x89, 0xe9, 0x83, 0xe1, 0x3f, 0x41, 0x89, 0xcd,
       0xa8, 0x20, 0x74, 0x14, 0x44, 0x89, 0xe0, 0xc1, 0xe8, 0x1f, 0x88, 0x85,
       0x96, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0xf7, 0xdc, 0xeb, 0x25,
       0x44, 0x89, 0xe2, 0x31, 0xc0, 0x45, 0x85, 0xe4, 0x79, 0x0f, 0xb8, 0xff,
       0xff, 0xff, 0xff, 0xd3, 0xe0, 0xf7, 0xd0, 0x41, 0x85, 0xc4, 0x0f, 0x95,
       0xc0, 0x88, 0x85, 0x96, 0x03, 0x00, 0x00, 0x41, 0x89, 0xd4, 0x41, 0xd3,
       0xfc
    };
    copy_block(op_sraw_T0_T1_code, 73);
    inc_code_ptr(73);
}
#endif

DEFINE_GEN(gen_op_sraw_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_sraw_T0_im
{
    static const uint8 op_sraw_T0_im_code[] = {
       0x44, 0x89, 0xe2, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x31, 0xc0, 0x45,
       0x85, 0xe4, 0x79, 0x0f, 0xb8, 0xff, 0xff, 0xff, 0xff, 0xd3, 0xe0, 0xf7,
       0xd0, 0x41, 0x85, 0xc4, 0x0f, 0x95, 0xc0, 0x88, 0x85, 0x96, 0x03, 0x00,
       0x00, 0x41, 0x89, 0xd4, 0x41, 0xd3, 0xfc
    };
    copy_block(op_sraw_T0_im_code, 43);
    *(uint32_t *)(code_ptr() + 5) = (int32_t)((long)param1 - (long)(code_ptr() + 5 + 4)) + 0;
    inc_code_ptr(43);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_26
{
    static const uint8 op_stmw_T0_26_code[] = {
       0x8b, 0x45, 0x78, 0x0f, 0xc8, 0x44, 0x89, 0xe2, 0x89, 0x02, 0x41, 0x8d,
       0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x45, 0x7c, 0x0f, 0xc8, 0x89,
       0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b,
       0x85, 0x80, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41,
       0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x84, 0x00, 0x00,
       0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54, 0x24, 0x04,
       0x41, 0x89, 0xd4, 0x8b, 0x85, 0x88, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89,
       0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b,
       0x85, 0x8c, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02
    };
    copy_block(op_stmw_T0_26_code, 107);
    inc_code_ptr(107);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_27
{
    static const uint8 op_stmw_T0_27_code[] = {
       0x8b, 0x45, 0x7c, 0x0f, 0xc8, 0x44, 0x89, 0xe2, 0x89, 0x02, 0x41, 0x8d,
       0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x80, 0x00, 0x00, 0x00,
       0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41,
       0x89, 0xd4, 0x8b, 0x85, 0x84, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2,
       0x89, 0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85,
       0x88, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41, 0x8d,
       0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x8c, 0x00, 0x00, 0x00,
       0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02
    };
    copy_block(op_stmw_T0_27_code, 90);
    inc_code_ptr(90);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_28
{
    static const uint8 op_stmw_T0_28_code[] = {
       0x8b, 0x85, 0x80, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x44, 0x89, 0xe2, 0x89,
       0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x84,
       0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54,
       0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x88, 0x00, 0x00, 0x00, 0x0f,
       0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89,
       0xd4, 0x8b, 0x85, 0x8c, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89,
       0x02
    };
    copy_block(op_stmw_T0_28_code, 73);
    inc_code_ptr(73);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_29
{
    static const uint8 op_stmw_T0_29_code[] = {
       0x8b, 0x85, 0x84, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x44, 0x89, 0xe2, 0x89,
       0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x88,
       0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02, 0x41, 0x8d, 0x54,
       0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x8c, 0x00, 0x00, 0x00, 0x0f,
       0xc8, 0x89, 0xd2, 0x89, 0x02
    };
    copy_block(op_stmw_T0_29_code, 53);
    inc_code_ptr(53);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_30
{
    static const uint8 op_stmw_T0_30_code[] = {
       0x8b, 0x85, 0x88, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x44, 0x89, 0xe2, 0x89,
       0x02, 0x41, 0x8d, 0x54, 0x24, 0x04, 0x41, 0x89, 0xd4, 0x8b, 0x85, 0x8c,
       0x00, 0x00, 0x00, 0x0f, 0xc8, 0x89, 0xd2, 0x89, 0x02
    };
    copy_block(op_stmw_T0_30_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_31
{
    static const uint8 op_stmw_T0_31_code[] = {
       0x8b, 0x85, 0x8c, 0x00, 0x00, 0x00, 0x0f, 0xc8, 0x44, 0x89, 0xe2, 0x89,
       0x02
    };
    copy_block(op_stmw_T0_31_code, 13);
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_stmw_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stmw_T0_im
{
    static const uint8 op_stmw_T0_im_code[] = {
       0x8d, 0x35, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89, 0xe1, 0xeb, 0x12, 0x48,
       0x63, 0xc6, 0x8b, 0x44, 0x85, 0x10, 0x0f, 0xc8, 0x89, 0xca, 0x89, 0x02,
       0xff, 0xc6, 0x83, 0xc1, 0x04, 0x83, 0xfe, 0x1f, 0x76, 0xe9
    };
    copy_block(op_stmw_T0_im_code, 34);
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param1 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(34);
}
#endif

DEFINE_GEN(gen_op_subf_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subf_T0_T1
{
    static const uint8 op_subf_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x29, 0xe0, 0x41, 0x89, 0xc4
    };
    copy_block(op_subf_T0_T1_code, 9);
    inc_code_ptr(9);
}
#endif

DEFINE_GEN(gen_op_subfmeo_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfmeo_T0
{
    static const uint8 op_subfmeo_T0_code[] = {
       0x53, 0x48, 0x89, 0xe9, 0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0xb8,
       0xff, 0xff, 0xff, 0xff, 0x89, 0xc3, 0x0f, 0xba, 0xe2, 0x00, 0xf5, 0x44,
       0x19, 0xe3, 0xf5, 0x0f, 0x92, 0xc2, 0x0f, 0x90, 0xc0, 0x41, 0x89, 0xdc,
       0x88, 0x95, 0x96, 0x03, 0x00, 0x00, 0x48, 0x83, 0xc1, 0x10, 0x88, 0x81,
       0x85, 0x03, 0x00, 0x00, 0x08, 0x81, 0x84, 0x03, 0x00, 0x00, 0x5b
    };
    copy_block(op_subfmeo_T0_code, 59);
    inc_code_ptr(59);
}
#endif

DEFINE_GEN(gen_op_subfzeo_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfzeo_T0
{
    static const uint8 op_subfzeo_T0_code[] = {
       0x53, 0x48, 0x89, 0xe9, 0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x31,
       0xc0, 0x89, 0xc3, 0x0f, 0xba, 0xe2, 0x00, 0xf5, 0x44, 0x19, 0xe3, 0xf5,
       0x0f, 0x92, 0xc2, 0x0f, 0x90, 0xc0, 0x41, 0x89, 0xdc, 0x88, 0x95, 0x96,
       0x03, 0x00, 0x00, 0x48, 0x83, 0xc1, 0x10, 0x88, 0x81, 0x85, 0x03, 0x00,
       0x00, 0x08, 0x81, 0x84, 0x03, 0x00, 0x00, 0x5b
    };
    copy_block(op_subfzeo_T0_code, 56);
    inc_code_ptr(56);
}
#endif

DEFINE_GEN(gen_op_addco_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addco_T0_T1
{
    static const uint8 op_addco_T0_T1_code[] = {
       0x45, 0x01, 0xec, 0x0f, 0x92, 0xc2, 0x0f, 0x90, 0xc1, 0x48, 0x89, 0xe8,
       0x88, 0x95, 0x96, 0x03, 0x00, 0x00, 0x48, 0x83, 0xc0, 0x10, 0x88, 0x88,
       0x85, 0x03, 0x00, 0x00, 0x08, 0x88, 0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_addco_T0_T1_code, 34);
    inc_code_ptr(34);
}
#endif

DEFINE_GEN(gen_op_addeo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_addeo_T0_T1
{
    static const uint8 op_addeo_T0_T1_code[] = {
       0x48, 0x89, 0xe8, 0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x0f, 0xba,
       0xe2, 0x00, 0x45, 0x11, 0xec, 0x0f, 0x92, 0xc2, 0x0f, 0x90, 0xc1, 0x88,
       0x95, 0x96, 0x03, 0x00, 0x00, 0x48, 0x83, 0xc0, 0x10, 0x88, 0x88, 0x85,
       0x03, 0x00, 0x00, 0x08, 0x88, 0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_addeo_T0_T1_code, 45);
    inc_code_ptr(45);
}
#endif

DEFINE_GEN(gen_op_branch_1_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_branch_1_T0
{
    static const uint8 op_branch_1_T0_code[] = {
       0x44, 0x89, 0xa5, 0xac, 0x03, 0x00, 0x00
    };
    copy_block(op_branch_1_T0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_branch_1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_branch_1_im
{
    static const uint8 op_branch_1_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0x85, 0xac, 0x03, 0x00,
       0x00
    };
    copy_block(op_branch_1_im_code, 13);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_divwo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_divwo_T0_T1
{
    static const uint8 op_divwo_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x44, 0x89, 0xe2, 0x45, 0x85, 0xed, 0x74, 0x0f, 0x41,
       0x81, 0xfc, 0x00, 0x00, 0x00, 0x80, 0x75, 0x1f, 0x41, 0x83, 0xfd, 0xff,
       0x75, 0x19, 0x89, 0xd1, 0xc1, 0xf9, 0x1f, 0x48, 0x8d, 0x45, 0x10, 0xc6,
       0x80, 0x85, 0x03, 0x00, 0x00, 0x01, 0x80, 0x88, 0x84, 0x03, 0x00, 0x00,
       0x01, 0xeb, 0x10, 0x89, 0xd0, 0xc1, 0xfa, 0x1f, 0xf7, 0xf9, 0x89, 0xc1,
       0xc6, 0x85, 0x95, 0x03, 0x00, 0x00, 0x00, 0x41, 0x89, 0xcc
    };
    copy_block(op_divwo_T0_T1_code, 70);
    inc_code_ptr(70);
}
#endif

DEFINE_GEN(gen_op_divwu_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_divwu_T0_T1
{
    static const uint8 op_divwu_T0_T1_code[] = {
       0x31, 0xc9, 0x45, 0x85, 0xed, 0x74, 0x0a, 0x44, 0x89, 0xe0, 0x31, 0xd2,
       0x41, 0xf7, 0xf5, 0x89, 0xc1, 0x41, 0x89, 0xcc
    };
    copy_block(op_divwu_T0_T1_code, 20);
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_fnabs_FD_F0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fnabs_FD_F0
{
    static const uint8 op_fnabs_FD_F0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
       0xff, 0x7f, 0x48, 0x21, 0xf8, 0x48, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x80, 0x48, 0x31, 0xf0, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10,
       0x00
    };
    copy_block(op_fnabs_FD_F0_code, 37);
    inc_code_ptr(37);
}
#endif

DEFINE_GEN(gen_op_load_T0_CTR,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_CTR
{
    static const uint8 op_load_T0_CTR_code[] = {
       0x44, 0x8b, 0xa5, 0xa8, 0x03, 0x00, 0x00
    };
    copy_block(op_load_T0_CTR_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_XER,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_XER
{
    static const uint8 op_load_T0_XER_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x0f, 0xb6, 0x91, 0x84, 0x03, 0x00, 0x00, 0xc1,
       0xe2, 0x1f, 0x0f, 0xb6, 0x81, 0x85, 0x03, 0x00, 0x00, 0xc1, 0xe0, 0x1e,
       0x09, 0xc2, 0x0f, 0xb6, 0x81, 0x87, 0x03, 0x00, 0x00, 0x09, 0xc2, 0x0f,
       0xb6, 0x81, 0x86, 0x03, 0x00, 0x00, 0xc1, 0xe0, 0x1d, 0x41, 0x89, 0xd4,
       0x41, 0x09, 0xc4
    };
    copy_block(op_load_T0_XER_code, 51);
    inc_code_ptr(51);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr0
{
    static const uint8 op_load_T0_cr0_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0xc1, 0xec,
       0x1c
    };
    copy_block(op_load_T0_cr0_code, 13);
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr1
{
    static const uint8 op_load_T0_cr1_code[] = {
       0x0f, 0xb6, 0x85, 0x93, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0x83,
       0xe4, 0x0f
    };
    copy_block(op_load_T0_cr1_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr2
{
    static const uint8 op_load_T0_cr2_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x14, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x0f
    };
    copy_block(op_load_T0_cr2_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr3
{
    static const uint8 op_load_T0_cr3_code[] = {
       0x0f, 0xb7, 0x85, 0x92, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0x83,
       0xe4, 0x0f
    };
    copy_block(op_load_T0_cr3_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr4
{
    static const uint8 op_load_T0_cr4_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0c, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x0f
    };
    copy_block(op_load_T0_cr4_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr5
{
    static const uint8 op_load_T0_cr5_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x08, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x0f
    };
    copy_block(op_load_T0_cr5_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr6
{
    static const uint8 op_load_T0_cr6_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x04, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x0f
    };
    copy_block(op_load_T0_cr6_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_cr7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_cr7
{
    static const uint8 op_load_T0_cr7_code[] = {
       0x44, 0x8b, 0xa5, 0x90, 0x03, 0x00, 0x00, 0x41, 0x83, 0xe4, 0x0f
    };
    copy_block(op_load_T0_cr7_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_lwarx_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_lwarx_T0_T1
{
    static const uint8 op_lwarx_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x8b, 0x00, 0x41, 0x89, 0xc4, 0x41, 0x0f, 0xcc, 0x48,
       0x89, 0xe8, 0xc7, 0x85, 0xb8, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
       0x44, 0x89, 0xa8, 0xbc, 0x03, 0x00, 0x00
    };
    copy_block(op_lwarx_T0_T1_code, 31);
    inc_code_ptr(31);
}
#endif

DEFINE_GEN(gen_op_mmx_vaddubm,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vaddubm
{
    static const uint8 op_mmx_vaddubm_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xfc, 0x45, 0x00, 0x41, 0x0f, 0xfc, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vaddubm_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vadduhm,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vadduhm
{
    static const uint8 op_mmx_vadduhm_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xfd, 0x45, 0x00, 0x41, 0x0f, 0xfd, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vadduhm_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vadduwm,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vadduwm
{
    static const uint8 op_mmx_vadduwm_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xfe, 0x45, 0x00, 0x41, 0x0f, 0xfe, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vadduwm_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vsububm,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vsububm
{
    static const uint8 op_mmx_vsububm_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xf8, 0x45, 0x00, 0x41, 0x0f, 0xf8, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vsububm_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vsubuhm,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vsubuhm
{
    static const uint8 op_mmx_vsubuhm_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xf9, 0x45, 0x00, 0x41, 0x0f, 0xf9, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vsubuhm_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vsubuwm,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vsubuwm
{
    static const uint8 op_mmx_vsubuwm_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0xfa, 0x45, 0x00, 0x41, 0x0f, 0xfa, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vsubuwm_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mtcrf_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mtcrf_T0_im
{
    static const uint8 op_mtcrf_T0_im_code[] = {
       0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0xc2, 0xf7, 0xd2, 0x23, 0x95,
       0x90, 0x03, 0x00, 0x00, 0x44, 0x21, 0xe0, 0x09, 0xc2, 0x89, 0x95, 0x90,
       0x03, 0x00, 0x00
    };
    copy_block(op_mtcrf_T0_im_code, 27);
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param1 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(27);
}
#endif

DEFINE_GEN(gen_op_mulhw_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mulhw_T0_T1
{
    static const uint8 op_mulhw_T0_T1_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe8, 0xf7, 0xea, 0x41, 0x89, 0xd4
    };
    copy_block(op_mulhw_T0_T1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_mulli_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mulli_T0_im
{
    static const uint8 op_mulli_T0_im_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x44, 0x0f, 0xaf, 0xe0
    };
    copy_block(op_mulli_T0_im_code, 11);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_rlwnm_T0_T1,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rlwnm_T0_T1
{
    static const uint8 op_rlwnm_T0_T1_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe9, 0xd3, 0xc2, 0x48, 0x8d, 0x05, 0x00,
       0x00, 0x00, 0x00, 0x41, 0x89, 0xd4, 0x41, 0x21, 0xc4
    };
    copy_block(op_rlwnm_T0_T1_code, 21);
    *(uint32_t *)(code_ptr() + 11) = (int32_t)((long)param1 - (long)(code_ptr() + 11 + 4)) + 0;
    inc_code_ptr(21);
}
#endif

DEFINE_GEN(gen_op_store_T0_CR,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_CR
{
    static const uint8 op_store_T0_CR_code[] = {
       0x44, 0x89, 0xa5, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_CR_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_LR,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_LR
{
    static const uint8 op_store_T0_LR_code[] = {
       0x44, 0x89, 0xa5, 0xa4, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_LR_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_PC,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_PC
{
    static const uint8 op_store_T0_PC_code[] = {
       0x44, 0x89, 0xa5, 0xac, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_PC_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_im_LR,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_im_LR
{
    static const uint8 op_store_im_LR_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0x85, 0xa4, 0x03, 0x00,
       0x00
    };
    copy_block(op_store_im_LR_code, 13);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_stwcx_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_stwcx_T0_T1
{
    static const uint8 op_stwcx_T0_T1_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0x25, 0xff, 0xff, 0xff, 0x0f, 0x0f,
       0xb6, 0x8d, 0x94, 0x03, 0x00, 0x00, 0xc1, 0xe1, 0x1c, 0x09, 0xc1, 0x44,
       0x8b, 0x85, 0xb8, 0x03, 0x00, 0x00, 0x45, 0x85, 0xc0, 0x74, 0x24, 0xc7,
       0x85, 0xb8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x85, 0xbc,
       0x03, 0x00, 0x00, 0x44, 0x39, 0xe8, 0x75, 0x0f, 0x44, 0x89, 0xe2, 0x0f,
       0xca, 0x89, 0xc0, 0x89, 0x10, 0x81, 0xc9, 0x00, 0x00, 0x00, 0x20, 0x89,
       0x8d, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_stwcx_T0_T1_code, 77);
    inc_code_ptr(77);
}
#endif

DEFINE_GEN(gen_op_subfc_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfc_T0_T1
{
    static const uint8 op_subfc_T0_T1_code[] = {
       0x44, 0x89, 0xea, 0x44, 0x29, 0xe2, 0xf5, 0x0f, 0x92, 0xc0, 0x41, 0x89,
       0xd4, 0x88, 0x85, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_subfc_T0_T1_code, 19);
    inc_code_ptr(19);
}
#endif

DEFINE_GEN(gen_op_subfc_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfc_T0_im
{
    static const uint8 op_subfc_T0_im_code[] = {
       0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0xc2, 0x44, 0x29, 0xe2, 0x39,
       0xd0, 0x0f, 0x93, 0x85, 0x96, 0x03, 0x00, 0x00, 0x41, 0x89, 0xd4
    };
    copy_block(op_subfc_T0_im_code, 23);
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param1 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(23);
}
#endif

DEFINE_GEN(gen_op_subfe_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfe_T0_T1
{
    static const uint8 op_subfe_T0_T1_code[] = {
       0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x44, 0x89, 0xe8, 0x0f, 0xba,
       0xe2, 0x00, 0xf5, 0x44, 0x19, 0xe0, 0xf5, 0x0f, 0x92, 0xc2, 0x41, 0x89,
       0xc4, 0x88, 0x95, 0x96, 0x03, 0x00, 0x00
    };
    copy_block(op_subfe_T0_T1_code, 31);
    inc_code_ptr(31);
}
#endif

DEFINE_GEN(gen_op_subfo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfo_T0_T1
{
    static const uint8 op_subfo_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x29, 0xe0, 0x0f, 0x90, 0xc2, 0x41, 0x89, 0xc4,
       0x48, 0x8d, 0x45, 0x10, 0x88, 0x90, 0x85, 0x03, 0x00, 0x00, 0x08, 0x90,
       0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_subfo_T0_T1_code, 28);
    inc_code_ptr(28);
}
#endif

DEFINE_GEN(gen_op_cntlzw_32_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_cntlzw_32_T0
{
    static const uint8 op_cntlzw_32_T0_code[] = {
       0xb8, 0xff, 0xff, 0xff, 0xff, 0x41, 0x0f, 0xbd, 0xc4, 0xba, 0x1f, 0x00,
       0x00, 0x00, 0x41, 0x89, 0xd4, 0x41, 0x29, 0xc4
    };
    copy_block(op_cntlzw_32_T0_code, 20);
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_compare_T0_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_compare_T0_0
{
    static const uint8 op_compare_T0_0_code[] = {
       0x0f, 0xb6, 0x95, 0x94, 0x03, 0x00, 0x00, 0x41, 0x83, 0xfc, 0x00, 0x7d,
       0x09, 0x41, 0x89, 0xd4, 0x41, 0x83, 0xcc, 0x08, 0xeb, 0x12, 0x89, 0xd0,
       0x83, 0xc8, 0x04, 0x83, 0xca, 0x02, 0x45, 0x85, 0xe4, 0x41, 0x89, 0xc4,
       0x44, 0x0f, 0x44, 0xe2
    };
    copy_block(op_compare_T0_0_code, 40);
    inc_code_ptr(40);
}
#endif

DEFINE_GEN(gen_op_divwuo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_divwuo_T0_T1
{
    static const uint8 op_divwuo_T0_T1_code[] = {
       0x44, 0x89, 0xe9, 0x45, 0x85, 0xed, 0x75, 0x14, 0x48, 0x8d, 0x45, 0x10,
       0xc6, 0x80, 0x85, 0x03, 0x00, 0x00, 0x01, 0x80, 0x88, 0x84, 0x03, 0x00,
       0x00, 0x01, 0xeb, 0x11, 0x44, 0x89, 0xe0, 0x31, 0xd2, 0x41, 0xf7, 0xf5,
       0x89, 0xc1, 0xc6, 0x85, 0x95, 0x03, 0x00, 0x00, 0x00, 0x41, 0x89, 0xcc
    };
    copy_block(op_divwuo_T0_T1_code, 48);
    inc_code_ptr(48);
}
#endif

DEFINE_GEN(gen_op_jump_next_A0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_jump_next_A0
{
    static const uint8 op_jump_next_A0_code[] = {
       0x4c, 0x89, 0xe0, 0x8b, 0x95, 0xac, 0x03, 0x00, 0x00, 0x49, 0x39, 0x14,
       0x24, 0x74, 0x1e, 0x48, 0x89, 0xd0, 0x48, 0xc1, 0xe8, 0x02, 0x25, 0xff,
       0x7f, 0x00, 0x00, 0x48, 0x8b, 0x84, 0xc5, 0x28, 0x08, 0x0c, 0x00, 0x48,
       0x85, 0xc0, 0x74, 0x08, 0x48, 0x3b, 0x10, 0x75, 0x03, 0xff, 0x60, 0x70
    };
    copy_block(op_jump_next_A0_code, 48);
    inc_code_ptr(48);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR0
{
    static const uint8 op_load_F0_FPR0_code[] = {
       0x4c, 0x8d, 0xa5, 0x90, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR1
{
    static const uint8 op_load_F0_FPR1_code[] = {
       0x4c, 0x8d, 0xa5, 0x98, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR2
{
    static const uint8 op_load_F0_FPR2_code[] = {
       0x4c, 0x8d, 0xa5, 0xa0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR3
{
    static const uint8 op_load_F0_FPR3_code[] = {
       0x4c, 0x8d, 0xa5, 0xa8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR4
{
    static const uint8 op_load_F0_FPR4_code[] = {
       0x4c, 0x8d, 0xa5, 0xb0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR5
{
    static const uint8 op_load_F0_FPR5_code[] = {
       0x4c, 0x8d, 0xa5, 0xb8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR6
{
    static const uint8 op_load_F0_FPR6_code[] = {
       0x4c, 0x8d, 0xa5, 0xc0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR7
{
    static const uint8 op_load_F0_FPR7_code[] = {
       0x4c, 0x8d, 0xa5, 0xc8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR8
{
    static const uint8 op_load_F0_FPR8_code[] = {
       0x4c, 0x8d, 0xa5, 0xd0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR9
{
    static const uint8 op_load_F0_FPR9_code[] = {
       0x4c, 0x8d, 0xa5, 0xd8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR0
{
    static const uint8 op_load_F1_FPR0_code[] = {
       0x4c, 0x8d, 0xad, 0x90, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR1
{
    static const uint8 op_load_F1_FPR1_code[] = {
       0x4c, 0x8d, 0xad, 0x98, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR2
{
    static const uint8 op_load_F1_FPR2_code[] = {
       0x4c, 0x8d, 0xad, 0xa0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR3
{
    static const uint8 op_load_F1_FPR3_code[] = {
       0x4c, 0x8d, 0xad, 0xa8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR4
{
    static const uint8 op_load_F1_FPR4_code[] = {
       0x4c, 0x8d, 0xad, 0xb0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR5
{
    static const uint8 op_load_F1_FPR5_code[] = {
       0x4c, 0x8d, 0xad, 0xb8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR6
{
    static const uint8 op_load_F1_FPR6_code[] = {
       0x4c, 0x8d, 0xad, 0xc0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR7
{
    static const uint8 op_load_F1_FPR7_code[] = {
       0x4c, 0x8d, 0xad, 0xc8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR8
{
    static const uint8 op_load_F1_FPR8_code[] = {
       0x4c, 0x8d, 0xad, 0xd0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR9
{
    static const uint8 op_load_F1_FPR9_code[] = {
       0x4c, 0x8d, 0xad, 0xd8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR0
{
    static const uint8 op_load_F2_FPR0_code[] = {
       0x4c, 0x8d, 0xb5, 0x90, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR1
{
    static const uint8 op_load_F2_FPR1_code[] = {
       0x4c, 0x8d, 0xb5, 0x98, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR2
{
    static const uint8 op_load_F2_FPR2_code[] = {
       0x4c, 0x8d, 0xb5, 0xa0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR3
{
    static const uint8 op_load_F2_FPR3_code[] = {
       0x4c, 0x8d, 0xb5, 0xa8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR4
{
    static const uint8 op_load_F2_FPR4_code[] = {
       0x4c, 0x8d, 0xb5, 0xb0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR5
{
    static const uint8 op_load_F2_FPR5_code[] = {
       0x4c, 0x8d, 0xb5, 0xb8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR6
{
    static const uint8 op_load_F2_FPR6_code[] = {
       0x4c, 0x8d, 0xb5, 0xc0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR7
{
    static const uint8 op_load_F2_FPR7_code[] = {
       0x4c, 0x8d, 0xb5, 0xc8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR8
{
    static const uint8 op_load_F2_FPR8_code[] = {
       0x4c, 0x8d, 0xb5, 0xd0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR9
{
    static const uint8 op_load_F2_FPR9_code[] = {
       0x4c, 0x8d, 0xb5, 0xd8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR0
{
    static const uint8 op_load_T0_GPR0_code[] = {
       0x44, 0x8b, 0x65, 0x10
    };
    copy_block(op_load_T0_GPR0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR1
{
    static const uint8 op_load_T0_GPR1_code[] = {
       0x44, 0x8b, 0x65, 0x14
    };
    copy_block(op_load_T0_GPR1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR2
{
    static const uint8 op_load_T0_GPR2_code[] = {
       0x44, 0x8b, 0x65, 0x18
    };
    copy_block(op_load_T0_GPR2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR3
{
    static const uint8 op_load_T0_GPR3_code[] = {
       0x44, 0x8b, 0x65, 0x1c
    };
    copy_block(op_load_T0_GPR3_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR4
{
    static const uint8 op_load_T0_GPR4_code[] = {
       0x44, 0x8b, 0x65, 0x20
    };
    copy_block(op_load_T0_GPR4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR5
{
    static const uint8 op_load_T0_GPR5_code[] = {
       0x44, 0x8b, 0x65, 0x24
    };
    copy_block(op_load_T0_GPR5_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR6
{
    static const uint8 op_load_T0_GPR6_code[] = {
       0x44, 0x8b, 0x65, 0x28
    };
    copy_block(op_load_T0_GPR6_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR7
{
    static const uint8 op_load_T0_GPR7_code[] = {
       0x44, 0x8b, 0x65, 0x2c
    };
    copy_block(op_load_T0_GPR7_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR8
{
    static const uint8 op_load_T0_GPR8_code[] = {
       0x44, 0x8b, 0x65, 0x30
    };
    copy_block(op_load_T0_GPR8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR9
{
    static const uint8 op_load_T0_GPR9_code[] = {
       0x44, 0x8b, 0x65, 0x34
    };
    copy_block(op_load_T0_GPR9_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb0
{
    static const uint8 op_load_T0_crb0_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0xc1, 0xec,
       0x1f
    };
    copy_block(op_load_T0_crb0_code, 13);
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb1
{
    static const uint8 op_load_T0_crb1_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1e, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb1_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb2
{
    static const uint8 op_load_T0_crb2_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1d, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb2_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb3
{
    static const uint8 op_load_T0_crb3_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1c, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb3_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb4
{
    static const uint8 op_load_T0_crb4_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1b, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb4_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb5
{
    static const uint8 op_load_T0_crb5_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1a, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb5_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb6
{
    static const uint8 op_load_T0_crb6_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x19, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb6_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb7
{
    static const uint8 op_load_T0_crb7_code[] = {
       0x0f, 0xb6, 0x85, 0x93, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0x83,
       0xe4, 0x01
    };
    copy_block(op_load_T0_crb7_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb8
{
    static const uint8 op_load_T0_crb8_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x17, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb8_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb9
{
    static const uint8 op_load_T0_crb9_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x16, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb9_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR0
{
    static const uint8 op_load_T1_GPR0_code[] = {
       0x44, 0x8b, 0x6d, 0x10
    };
    copy_block(op_load_T1_GPR0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR1
{
    static const uint8 op_load_T1_GPR1_code[] = {
       0x44, 0x8b, 0x6d, 0x14
    };
    copy_block(op_load_T1_GPR1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR2
{
    static const uint8 op_load_T1_GPR2_code[] = {
       0x44, 0x8b, 0x6d, 0x18
    };
    copy_block(op_load_T1_GPR2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR3
{
    static const uint8 op_load_T1_GPR3_code[] = {
       0x44, 0x8b, 0x6d, 0x1c
    };
    copy_block(op_load_T1_GPR3_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR4
{
    static const uint8 op_load_T1_GPR4_code[] = {
       0x44, 0x8b, 0x6d, 0x20
    };
    copy_block(op_load_T1_GPR4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR5
{
    static const uint8 op_load_T1_GPR5_code[] = {
       0x44, 0x8b, 0x6d, 0x24
    };
    copy_block(op_load_T1_GPR5_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR6
{
    static const uint8 op_load_T1_GPR6_code[] = {
       0x44, 0x8b, 0x6d, 0x28
    };
    copy_block(op_load_T1_GPR6_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR7
{
    static const uint8 op_load_T1_GPR7_code[] = {
       0x44, 0x8b, 0x6d, 0x2c
    };
    copy_block(op_load_T1_GPR7_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR8
{
    static const uint8 op_load_T1_GPR8_code[] = {
       0x44, 0x8b, 0x6d, 0x30
    };
    copy_block(op_load_T1_GPR8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR9
{
    static const uint8 op_load_T1_GPR9_code[] = {
       0x44, 0x8b, 0x6d, 0x34
    };
    copy_block(op_load_T1_GPR9_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb0
{
    static const uint8 op_load_T1_crb0_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc5, 0x41, 0xc1, 0xed,
       0x1f
    };
    copy_block(op_load_T1_crb0_code, 13);
    inc_code_ptr(13);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb1
{
    static const uint8 op_load_T1_crb1_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1e, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb1_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb2
{
    static const uint8 op_load_T1_crb2_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1d, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb2_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb3
{
    static const uint8 op_load_T1_crb3_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1c, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb3_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb4
{
    static const uint8 op_load_T1_crb4_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1b, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb4_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb5
{
    static const uint8 op_load_T1_crb5_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x1a, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb5_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb6
{
    static const uint8 op_load_T1_crb6_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x19, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb6_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb7
{
    static const uint8 op_load_T1_crb7_code[] = {
       0x0f, 0xb6, 0x85, 0x93, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc5, 0x41, 0x83,
       0xe5, 0x01
    };
    copy_block(op_load_T1_crb7_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb8
{
    static const uint8 op_load_T1_crb8_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x17, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb8_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb9
{
    static const uint8 op_load_T1_crb9_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x16, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb9_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR0
{
    static const uint8 op_load_T2_GPR0_code[] = {
       0x44, 0x8b, 0x75, 0x10
    };
    copy_block(op_load_T2_GPR0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR1
{
    static const uint8 op_load_T2_GPR1_code[] = {
       0x44, 0x8b, 0x75, 0x14
    };
    copy_block(op_load_T2_GPR1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR2
{
    static const uint8 op_load_T2_GPR2_code[] = {
       0x44, 0x8b, 0x75, 0x18
    };
    copy_block(op_load_T2_GPR2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR3
{
    static const uint8 op_load_T2_GPR3_code[] = {
       0x44, 0x8b, 0x75, 0x1c
    };
    copy_block(op_load_T2_GPR3_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR4
{
    static const uint8 op_load_T2_GPR4_code[] = {
       0x44, 0x8b, 0x75, 0x20
    };
    copy_block(op_load_T2_GPR4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR5
{
    static const uint8 op_load_T2_GPR5_code[] = {
       0x44, 0x8b, 0x75, 0x24
    };
    copy_block(op_load_T2_GPR5_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR6
{
    static const uint8 op_load_T2_GPR6_code[] = {
       0x44, 0x8b, 0x75, 0x28
    };
    copy_block(op_load_T2_GPR6_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR7
{
    static const uint8 op_load_T2_GPR7_code[] = {
       0x44, 0x8b, 0x75, 0x2c
    };
    copy_block(op_load_T2_GPR7_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR8
{
    static const uint8 op_load_T2_GPR8_code[] = {
       0x44, 0x8b, 0x75, 0x30
    };
    copy_block(op_load_T2_GPR8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR9
{
    static const uint8 op_load_T2_GPR9_code[] = {
       0x44, 0x8b, 0x75, 0x34
    };
    copy_block(op_load_T2_GPR9_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_mmx_vcmpequb,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vcmpequb
{
    static const uint8 op_mmx_vcmpequb_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0x74, 0x45, 0x00, 0x41, 0x0f, 0x74, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vcmpequb_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vcmpequh,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vcmpequh
{
    static const uint8 op_mmx_vcmpequh_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0x75, 0x45, 0x00, 0x41, 0x0f, 0x75, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vcmpequh_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vcmpequw,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vcmpequw
{
    static const uint8 op_mmx_vcmpequw_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0x76, 0x45, 0x00, 0x41, 0x0f, 0x76, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vcmpequw_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vcmpgtsb,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vcmpgtsb
{
    static const uint8 op_mmx_vcmpgtsb_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0x64, 0x45, 0x00, 0x41, 0x0f, 0x64, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vcmpgtsb_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vcmpgtsh,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vcmpgtsh
{
    static const uint8 op_mmx_vcmpgtsh_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0x65, 0x45, 0x00, 0x41, 0x0f, 0x65, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vcmpgtsh_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mmx_vcmpgtsw,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mmx_vcmpgtsw
{
    static const uint8 op_mmx_vcmpgtsw_code[] = {
       0x41, 0x0f, 0x6f, 0x04, 0x24, 0x41, 0x0f, 0x6f, 0x4c, 0x24, 0x08, 0x41,
       0x0f, 0x66, 0x45, 0x00, 0x41, 0x0f, 0x66, 0x4d, 0x08, 0x41, 0x0f, 0x7f,
       0x07, 0x41, 0x0f, 0x7f, 0x4f, 0x08
    };
    copy_block(op_mmx_vcmpgtsw_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_mulhwu_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mulhwu_T0_T1
{
    static const uint8 op_mulhwu_T0_T1_code[] = {
       0x44, 0x89, 0xe2, 0x44, 0x89, 0xe8, 0xf7, 0xe2, 0x41, 0x89, 0xd4
    };
    copy_block(op_mulhwu_T0_T1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_mullwo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_mullwo_T0_T1
{
    static const uint8 op_mullwo_T0_T1_code[] = {
       0x49, 0x63, 0xcd, 0x49, 0x63, 0xc4, 0x48, 0x0f, 0xaf, 0xc8, 0x48, 0x63,
       0xc1, 0x31, 0xd2, 0x48, 0x39, 0xc8, 0x0f, 0x95, 0xc2, 0x48, 0x8d, 0x45,
       0x10, 0x88, 0x90, 0x85, 0x03, 0x00, 0x00, 0x08, 0x90, 0x84, 0x03, 0x00,
       0x00, 0x41, 0x89, 0xcc
    };
    copy_block(op_mullwo_T0_T1_code, 40);
    inc_code_ptr(40);
}
#endif

DEFINE_GEN(gen_op_rlwimi_T0_T1,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rlwimi_T0_T1
{
    static const uint8 op_rlwimi_T0_T1_code[] = {
       0x8d, 0x15, 0x00, 0x00, 0x00, 0x00, 0x89, 0xd6, 0xf7, 0xd6, 0x44, 0x21,
       0xe6, 0x48, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89, 0xe8, 0xd3,
       0xc0, 0x21, 0xc2, 0x41, 0x89, 0xf4, 0x41, 0x09, 0xd4
    };
    copy_block(op_rlwimi_T0_T1_code, 33);
    *(uint32_t *)(code_ptr() + 16) = (int32_t)((long)param1 - (long)(code_ptr() + 16 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param2 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_rlwinm_T0_T1,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_rlwinm_T0_T1
{
    static const uint8 op_rlwinm_T0_T1_code[] = {
       0x48, 0x8d, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x44, 0x89, 0xe2, 0xd3, 0xc2,
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x89, 0xd4, 0x41, 0x21,
       0xc4
    };
    copy_block(op_rlwinm_T0_T1_code, 25);
    *(uint32_t *)(code_ptr() + 15) = (int32_t)((long)param2 - (long)(code_ptr() + 15 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(25);
}
#endif

DEFINE_GEN(gen_op_spcflags_set,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_spcflags_set
{
    static const uint8 op_spcflags_set_code[] = {
       0x48, 0x8d, 0x8d, 0xb0, 0x03, 0x00, 0x00, 0x48, 0x8d, 0x95, 0xb4, 0x03,
       0x00, 0x00, 0xb8, 0x01, 0x00, 0x00, 0x00, 0x87, 0x02, 0x85, 0xc0, 0x75,
       0xf5, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0xc7, 0x41,
       0x04, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_spcflags_set_code, 41);
    *(uint32_t *)(code_ptr() + 28) = (int32_t)((long)param1 - (long)(code_ptr() + 28 + 4)) + 0;
    inc_code_ptr(41);
}
#endif

DEFINE_GEN(gen_op_store_T0_CTR,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_CTR
{
    static const uint8 op_store_T0_CTR_code[] = {
       0x44, 0x89, 0xa5, 0xa8, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_CTR_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_XER,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_XER
{
    static const uint8 op_store_T0_XER_code[] = {
       0x44, 0x89, 0xe2, 0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe0, 0xc1, 0xe8,
       0x1f, 0x88, 0x81, 0x84, 0x03, 0x00, 0x00, 0x44, 0x89, 0xe0, 0xc1, 0xe8,
       0x1e, 0x83, 0xe0, 0x01, 0x88, 0x81, 0x85, 0x03, 0x00, 0x00, 0x44, 0x89,
       0xe0, 0xc1, 0xe8, 0x1d, 0x83, 0xe0, 0x01, 0x88, 0x81, 0x86, 0x03, 0x00,
       0x00, 0x83, 0xe2, 0x7f, 0x88, 0x91, 0x87, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_XER_code, 58);
    inc_code_ptr(58);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr0
{
    static const uint8 op_store_T0_cr0_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x1c, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x25, 0xff, 0xff, 0xff, 0x0f, 0x09, 0xd0, 0x89,
       0x81, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr0_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr1
{
    static const uint8 op_store_T0_cr1_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x18, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x25, 0xff, 0xff, 0xff, 0xf0, 0x09, 0xd0, 0x89,
       0x81, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr1_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr2
{
    static const uint8 op_store_T0_cr2_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x14, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x25, 0xff, 0xff, 0x0f, 0xff, 0x09, 0xd0, 0x89,
       0x81, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr2_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr3
{
    static const uint8 op_store_T0_cr3_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x10, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x25, 0xff, 0xff, 0xf0, 0xff, 0x09, 0xd0, 0x89,
       0x81, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr3_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr4
{
    static const uint8 op_store_T0_cr4_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x0c, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x80, 0xe4, 0x0f, 0x09, 0xd0, 0x89, 0x81, 0x80,
       0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr4_code, 27);
    inc_code_ptr(27);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr5
{
    static const uint8 op_store_T0_cr5_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x08, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x80, 0xe4, 0xf0, 0x09, 0xd0, 0x89, 0x81, 0x80,
       0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr5_code, 27);
    inc_code_ptr(27);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr6
{
    static const uint8 op_store_T0_cr6_code[] = {
       0x48, 0x8d, 0x4d, 0x10, 0x44, 0x89, 0xe2, 0xc1, 0xe2, 0x04, 0x8b, 0x81,
       0x80, 0x03, 0x00, 0x00, 0x24, 0x0f, 0x09, 0xd0, 0x89, 0x81, 0x80, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_cr6_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_cr7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_cr7
{
    static const uint8 op_store_T0_cr7_code[] = {
       0x48, 0x8d, 0x55, 0x10, 0x8b, 0x82, 0x80, 0x03, 0x00, 0x00, 0x83, 0xe0,
       0xf0, 0x44, 0x09, 0xe0, 0x89, 0x82, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_cr7_code, 22);
    inc_code_ptr(22);
}
#endif

DEFINE_GEN(gen_op_subfco_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfco_T0_T1
{
    static const uint8 op_subfco_T0_T1_code[] = {
       0x44, 0x89, 0xe8, 0x44, 0x29, 0xe0, 0xf5, 0x0f, 0x92, 0xc2, 0x0f, 0x90,
       0xc1, 0x41, 0x89, 0xc4, 0x48, 0x89, 0xe8, 0x88, 0x95, 0x96, 0x03, 0x00,
       0x00, 0x48, 0x83, 0xc0, 0x10, 0x88, 0x88, 0x85, 0x03, 0x00, 0x00, 0x08,
       0x88, 0x84, 0x03, 0x00, 0x00
    };
    copy_block(op_subfco_T0_T1_code, 41);
    inc_code_ptr(41);
}
#endif

DEFINE_GEN(gen_op_subfeo_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_subfeo_T0_T1
{
    static const uint8 op_subfeo_T0_T1_code[] = {
       0x53, 0x48, 0x89, 0xe9, 0x0f, 0xb6, 0x95, 0x96, 0x03, 0x00, 0x00, 0x44,
       0x89, 0xeb, 0x0f, 0xba, 0xe2, 0x00, 0xf5, 0x44, 0x19, 0xe3, 0xf5, 0x0f,
       0x92, 0xc2, 0x0f, 0x90, 0xc0, 0x41, 0x89, 0xdc, 0x88, 0x95, 0x96, 0x03,
       0x00, 0x00, 0x48, 0x83, 0xc1, 0x10, 0x88, 0x81, 0x85, 0x03, 0x00, 0x00,
       0x08, 0x81, 0x84, 0x03, 0x00, 0x00, 0x5b
    };
    copy_block(op_subfeo_T0_T1_code, 55);
    inc_code_ptr(55);
}
#endif

DEFINE_GEN(gen_op_vor_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vor_VD_V0_V1
{
    static const uint8 op_vor_VD_V0_V1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x49, 0x0b, 0x04, 0x24, 0x49, 0x89, 0x07, 0x49,
       0x8b, 0x45, 0x08, 0x49, 0x0b, 0x44, 0x24, 0x08, 0x49, 0x89, 0x47, 0x08
    };
    copy_block(op_vor_VD_V0_V1_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_compare_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_compare_T0_T1
{
    static const uint8 op_compare_T0_T1_code[] = {
       0x0f, 0xb6, 0x95, 0x94, 0x03, 0x00, 0x00, 0x45, 0x39, 0xec, 0x7d, 0x09,
       0x41, 0x89, 0xd4, 0x41, 0x83, 0xcc, 0x08, 0xeb, 0x12, 0x89, 0xd0, 0x83,
       0xc8, 0x04, 0x83, 0xca, 0x02, 0x45, 0x39, 0xec, 0x41, 0x89, 0xc4, 0x44,
       0x0f, 0x4e, 0xe2
    };
    copy_block(op_compare_T0_T1_code, 39);
    inc_code_ptr(39);
}
#endif

DEFINE_GEN(gen_op_compare_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_compare_T0_im
{
    static const uint8 op_compare_T0_im_code[] = {
       0x0f, 0xb6, 0x95, 0x94, 0x03, 0x00, 0x00, 0x8d, 0x35, 0x00, 0x00, 0x00,
       0x00, 0x41, 0x39, 0xf4, 0x7d, 0x09, 0x41, 0x89, 0xd4, 0x41, 0x83, 0xcc,
       0x08, 0xeb, 0x12, 0x89, 0xd0, 0x83, 0xc8, 0x04, 0x83, 0xca, 0x02, 0x41,
       0x39, 0xf4, 0x41, 0x89, 0xc4, 0x44, 0x0f, 0x4e, 0xe2
    };
    copy_block(op_compare_T0_im_code, 45);
    *(uint32_t *)(code_ptr() + 9) = (int32_t)((long)param1 - (long)(code_ptr() + 9 + 4)) + 0;
    inc_code_ptr(45);
}
#endif

DEFINE_GEN(gen_op_fadd_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fadd_FD_F0_F1
{
    static const uint8 op_fadd_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x58, 0x45, 0x00,
       0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fadd_FD_F0_F1_code, 20);
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_fdiv_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fdiv_FD_F0_F1
{
    static const uint8 op_fdiv_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x5e, 0x45, 0x00,
       0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fdiv_FD_F0_F1_code, 20);
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_fmul_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmul_FD_F0_F1
{
    static const uint8 op_fmul_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmul_FD_F0_F1_code, 20);
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_fsub_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fsub_FD_F0_F1
{
    static const uint8 op_fsub_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x5c, 0x45, 0x00,
       0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fsub_FD_F0_F1_code, 20);
    inc_code_ptr(20);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR10
{
    static const uint8 op_load_F0_FPR10_code[] = {
       0x4c, 0x8d, 0xa5, 0xe0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR11
{
    static const uint8 op_load_F0_FPR11_code[] = {
       0x4c, 0x8d, 0xa5, 0xe8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR12
{
    static const uint8 op_load_F0_FPR12_code[] = {
       0x4c, 0x8d, 0xa5, 0xf0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR13
{
    static const uint8 op_load_F0_FPR13_code[] = {
       0x4c, 0x8d, 0xa5, 0xf8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR14
{
    static const uint8 op_load_F0_FPR14_code[] = {
       0x4c, 0x8d, 0xa5, 0x00, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR15
{
    static const uint8 op_load_F0_FPR15_code[] = {
       0x4c, 0x8d, 0xa5, 0x08, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR16
{
    static const uint8 op_load_F0_FPR16_code[] = {
       0x4c, 0x8d, 0xa5, 0x10, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR17
{
    static const uint8 op_load_F0_FPR17_code[] = {
       0x4c, 0x8d, 0xa5, 0x18, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR18
{
    static const uint8 op_load_F0_FPR18_code[] = {
       0x4c, 0x8d, 0xa5, 0x20, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR19
{
    static const uint8 op_load_F0_FPR19_code[] = {
       0x4c, 0x8d, 0xa5, 0x28, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR20
{
    static const uint8 op_load_F0_FPR20_code[] = {
       0x4c, 0x8d, 0xa5, 0x30, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR21
{
    static const uint8 op_load_F0_FPR21_code[] = {
       0x4c, 0x8d, 0xa5, 0x38, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR22
{
    static const uint8 op_load_F0_FPR22_code[] = {
       0x4c, 0x8d, 0xa5, 0x40, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR23
{
    static const uint8 op_load_F0_FPR23_code[] = {
       0x4c, 0x8d, 0xa5, 0x48, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR24
{
    static const uint8 op_load_F0_FPR24_code[] = {
       0x4c, 0x8d, 0xa5, 0x50, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR25
{
    static const uint8 op_load_F0_FPR25_code[] = {
       0x4c, 0x8d, 0xa5, 0x58, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR26
{
    static const uint8 op_load_F0_FPR26_code[] = {
       0x4c, 0x8d, 0xa5, 0x60, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR27
{
    static const uint8 op_load_F0_FPR27_code[] = {
       0x4c, 0x8d, 0xa5, 0x68, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR28
{
    static const uint8 op_load_F0_FPR28_code[] = {
       0x4c, 0x8d, 0xa5, 0x70, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR29
{
    static const uint8 op_load_F0_FPR29_code[] = {
       0x4c, 0x8d, 0xa5, 0x78, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR30
{
    static const uint8 op_load_F0_FPR30_code[] = {
       0x4c, 0x8d, 0xa5, 0x80, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F0_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F0_FPR31
{
    static const uint8 op_load_F0_FPR31_code[] = {
       0x4c, 0x8d, 0xa5, 0x88, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F0_FPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR10
{
    static const uint8 op_load_F1_FPR10_code[] = {
       0x4c, 0x8d, 0xad, 0xe0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR11
{
    static const uint8 op_load_F1_FPR11_code[] = {
       0x4c, 0x8d, 0xad, 0xe8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR12
{
    static const uint8 op_load_F1_FPR12_code[] = {
       0x4c, 0x8d, 0xad, 0xf0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR13
{
    static const uint8 op_load_F1_FPR13_code[] = {
       0x4c, 0x8d, 0xad, 0xf8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR14
{
    static const uint8 op_load_F1_FPR14_code[] = {
       0x4c, 0x8d, 0xad, 0x00, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR15
{
    static const uint8 op_load_F1_FPR15_code[] = {
       0x4c, 0x8d, 0xad, 0x08, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR16
{
    static const uint8 op_load_F1_FPR16_code[] = {
       0x4c, 0x8d, 0xad, 0x10, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR17
{
    static const uint8 op_load_F1_FPR17_code[] = {
       0x4c, 0x8d, 0xad, 0x18, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR18
{
    static const uint8 op_load_F1_FPR18_code[] = {
       0x4c, 0x8d, 0xad, 0x20, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR19
{
    static const uint8 op_load_F1_FPR19_code[] = {
       0x4c, 0x8d, 0xad, 0x28, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR20
{
    static const uint8 op_load_F1_FPR20_code[] = {
       0x4c, 0x8d, 0xad, 0x30, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR21
{
    static const uint8 op_load_F1_FPR21_code[] = {
       0x4c, 0x8d, 0xad, 0x38, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR22
{
    static const uint8 op_load_F1_FPR22_code[] = {
       0x4c, 0x8d, 0xad, 0x40, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR23
{
    static const uint8 op_load_F1_FPR23_code[] = {
       0x4c, 0x8d, 0xad, 0x48, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR24
{
    static const uint8 op_load_F1_FPR24_code[] = {
       0x4c, 0x8d, 0xad, 0x50, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR25
{
    static const uint8 op_load_F1_FPR25_code[] = {
       0x4c, 0x8d, 0xad, 0x58, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR26
{
    static const uint8 op_load_F1_FPR26_code[] = {
       0x4c, 0x8d, 0xad, 0x60, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR27
{
    static const uint8 op_load_F1_FPR27_code[] = {
       0x4c, 0x8d, 0xad, 0x68, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR28
{
    static const uint8 op_load_F1_FPR28_code[] = {
       0x4c, 0x8d, 0xad, 0x70, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR29
{
    static const uint8 op_load_F1_FPR29_code[] = {
       0x4c, 0x8d, 0xad, 0x78, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR30
{
    static const uint8 op_load_F1_FPR30_code[] = {
       0x4c, 0x8d, 0xad, 0x80, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F1_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F1_FPR31
{
    static const uint8 op_load_F1_FPR31_code[] = {
       0x4c, 0x8d, 0xad, 0x88, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F1_FPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR10
{
    static const uint8 op_load_F2_FPR10_code[] = {
       0x4c, 0x8d, 0xb5, 0xe0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR11
{
    static const uint8 op_load_F2_FPR11_code[] = {
       0x4c, 0x8d, 0xb5, 0xe8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR12
{
    static const uint8 op_load_F2_FPR12_code[] = {
       0x4c, 0x8d, 0xb5, 0xf0, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR13
{
    static const uint8 op_load_F2_FPR13_code[] = {
       0x4c, 0x8d, 0xb5, 0xf8, 0x00, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR14
{
    static const uint8 op_load_F2_FPR14_code[] = {
       0x4c, 0x8d, 0xb5, 0x00, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR15
{
    static const uint8 op_load_F2_FPR15_code[] = {
       0x4c, 0x8d, 0xb5, 0x08, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR16
{
    static const uint8 op_load_F2_FPR16_code[] = {
       0x4c, 0x8d, 0xb5, 0x10, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR17
{
    static const uint8 op_load_F2_FPR17_code[] = {
       0x4c, 0x8d, 0xb5, 0x18, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR18
{
    static const uint8 op_load_F2_FPR18_code[] = {
       0x4c, 0x8d, 0xb5, 0x20, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR19
{
    static const uint8 op_load_F2_FPR19_code[] = {
       0x4c, 0x8d, 0xb5, 0x28, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR20
{
    static const uint8 op_load_F2_FPR20_code[] = {
       0x4c, 0x8d, 0xb5, 0x30, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR21
{
    static const uint8 op_load_F2_FPR21_code[] = {
       0x4c, 0x8d, 0xb5, 0x38, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR22
{
    static const uint8 op_load_F2_FPR22_code[] = {
       0x4c, 0x8d, 0xb5, 0x40, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR23
{
    static const uint8 op_load_F2_FPR23_code[] = {
       0x4c, 0x8d, 0xb5, 0x48, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR24
{
    static const uint8 op_load_F2_FPR24_code[] = {
       0x4c, 0x8d, 0xb5, 0x50, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR25
{
    static const uint8 op_load_F2_FPR25_code[] = {
       0x4c, 0x8d, 0xb5, 0x58, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR26
{
    static const uint8 op_load_F2_FPR26_code[] = {
       0x4c, 0x8d, 0xb5, 0x60, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR27
{
    static const uint8 op_load_F2_FPR27_code[] = {
       0x4c, 0x8d, 0xb5, 0x68, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR28
{
    static const uint8 op_load_F2_FPR28_code[] = {
       0x4c, 0x8d, 0xb5, 0x70, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR29
{
    static const uint8 op_load_F2_FPR29_code[] = {
       0x4c, 0x8d, 0xb5, 0x78, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR30
{
    static const uint8 op_load_F2_FPR30_code[] = {
       0x4c, 0x8d, 0xb5, 0x80, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_F2_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_F2_FPR31
{
    static const uint8 op_load_F2_FPR31_code[] = {
       0x4c, 0x8d, 0xb5, 0x88, 0x01, 0x00, 0x00
    };
    copy_block(op_load_F2_FPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR10
{
    static const uint8 op_load_T0_GPR10_code[] = {
       0x44, 0x8b, 0x65, 0x38
    };
    copy_block(op_load_T0_GPR10_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR11
{
    static const uint8 op_load_T0_GPR11_code[] = {
       0x44, 0x8b, 0x65, 0x3c
    };
    copy_block(op_load_T0_GPR11_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR12
{
    static const uint8 op_load_T0_GPR12_code[] = {
       0x44, 0x8b, 0x65, 0x40
    };
    copy_block(op_load_T0_GPR12_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR13
{
    static const uint8 op_load_T0_GPR13_code[] = {
       0x44, 0x8b, 0x65, 0x44
    };
    copy_block(op_load_T0_GPR13_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR14
{
    static const uint8 op_load_T0_GPR14_code[] = {
       0x44, 0x8b, 0x65, 0x48
    };
    copy_block(op_load_T0_GPR14_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR15
{
    static const uint8 op_load_T0_GPR15_code[] = {
       0x44, 0x8b, 0x65, 0x4c
    };
    copy_block(op_load_T0_GPR15_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR16
{
    static const uint8 op_load_T0_GPR16_code[] = {
       0x44, 0x8b, 0x65, 0x50
    };
    copy_block(op_load_T0_GPR16_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR17
{
    static const uint8 op_load_T0_GPR17_code[] = {
       0x44, 0x8b, 0x65, 0x54
    };
    copy_block(op_load_T0_GPR17_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR18
{
    static const uint8 op_load_T0_GPR18_code[] = {
       0x44, 0x8b, 0x65, 0x58
    };
    copy_block(op_load_T0_GPR18_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR19
{
    static const uint8 op_load_T0_GPR19_code[] = {
       0x44, 0x8b, 0x65, 0x5c
    };
    copy_block(op_load_T0_GPR19_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR20
{
    static const uint8 op_load_T0_GPR20_code[] = {
       0x44, 0x8b, 0x65, 0x60
    };
    copy_block(op_load_T0_GPR20_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR21
{
    static const uint8 op_load_T0_GPR21_code[] = {
       0x44, 0x8b, 0x65, 0x64
    };
    copy_block(op_load_T0_GPR21_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR22
{
    static const uint8 op_load_T0_GPR22_code[] = {
       0x44, 0x8b, 0x65, 0x68
    };
    copy_block(op_load_T0_GPR22_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR23
{
    static const uint8 op_load_T0_GPR23_code[] = {
       0x44, 0x8b, 0x65, 0x6c
    };
    copy_block(op_load_T0_GPR23_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR24
{
    static const uint8 op_load_T0_GPR24_code[] = {
       0x44, 0x8b, 0x65, 0x70
    };
    copy_block(op_load_T0_GPR24_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR25
{
    static const uint8 op_load_T0_GPR25_code[] = {
       0x44, 0x8b, 0x65, 0x74
    };
    copy_block(op_load_T0_GPR25_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR26
{
    static const uint8 op_load_T0_GPR26_code[] = {
       0x44, 0x8b, 0x65, 0x78
    };
    copy_block(op_load_T0_GPR26_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR27
{
    static const uint8 op_load_T0_GPR27_code[] = {
       0x44, 0x8b, 0x65, 0x7c
    };
    copy_block(op_load_T0_GPR27_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR28
{
    static const uint8 op_load_T0_GPR28_code[] = {
       0x44, 0x8b, 0xa5, 0x80, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T0_GPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR29
{
    static const uint8 op_load_T0_GPR29_code[] = {
       0x44, 0x8b, 0xa5, 0x84, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T0_GPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR30
{
    static const uint8 op_load_T0_GPR30_code[] = {
       0x44, 0x8b, 0xa5, 0x88, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T0_GPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_GPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_GPR31
{
    static const uint8 op_load_T0_GPR31_code[] = {
       0x44, 0x8b, 0xa5, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T0_GPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb10
{
    static const uint8 op_load_T0_crb10_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x15, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb10_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb11
{
    static const uint8 op_load_T0_crb11_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x14, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb11_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb12
{
    static const uint8 op_load_T0_crb12_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x13, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb12_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb13
{
    static const uint8 op_load_T0_crb13_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x12, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb13_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb14
{
    static const uint8 op_load_T0_crb14_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x11, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb14_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb15
{
    static const uint8 op_load_T0_crb15_code[] = {
       0x0f, 0xb7, 0x85, 0x92, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc4, 0x41, 0x83,
       0xe4, 0x01
    };
    copy_block(op_load_T0_crb15_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb16
{
    static const uint8 op_load_T0_crb16_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0f, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb16_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb17
{
    static const uint8 op_load_T0_crb17_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0e, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb17_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb18
{
    static const uint8 op_load_T0_crb18_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0d, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb18_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb19
{
    static const uint8 op_load_T0_crb19_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0c, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb19_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb20
{
    static const uint8 op_load_T0_crb20_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0b, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb20_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb21
{
    static const uint8 op_load_T0_crb21_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0a, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb21_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb22
{
    static const uint8 op_load_T0_crb22_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x09, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb22_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb23
{
    static const uint8 op_load_T0_crb23_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x08, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb23_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb24
{
    static const uint8 op_load_T0_crb24_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x07, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb24_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb25
{
    static const uint8 op_load_T0_crb25_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x06, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb25_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb26
{
    static const uint8 op_load_T0_crb26_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x05, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb26_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb27
{
    static const uint8 op_load_T0_crb27_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x04, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb27_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb28
{
    static const uint8 op_load_T0_crb28_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x03, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb28_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb29
{
    static const uint8 op_load_T0_crb29_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x02, 0x41, 0x89, 0xc4,
       0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb29_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb30
{
    static const uint8 op_load_T0_crb30_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xd1, 0xe8, 0x41, 0x89, 0xc4, 0x41,
       0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb30_code, 15);
    inc_code_ptr(15);
}
#endif

DEFINE_GEN(gen_op_load_T0_crb31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_crb31
{
    static const uint8 op_load_T0_crb31_code[] = {
       0x44, 0x8b, 0xa5, 0x90, 0x03, 0x00, 0x00, 0x41, 0x83, 0xe4, 0x01
    };
    copy_block(op_load_T0_crb31_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR10
{
    static const uint8 op_load_T1_GPR10_code[] = {
       0x44, 0x8b, 0x6d, 0x38
    };
    copy_block(op_load_T1_GPR10_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR11
{
    static const uint8 op_load_T1_GPR11_code[] = {
       0x44, 0x8b, 0x6d, 0x3c
    };
    copy_block(op_load_T1_GPR11_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR12
{
    static const uint8 op_load_T1_GPR12_code[] = {
       0x44, 0x8b, 0x6d, 0x40
    };
    copy_block(op_load_T1_GPR12_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR13
{
    static const uint8 op_load_T1_GPR13_code[] = {
       0x44, 0x8b, 0x6d, 0x44
    };
    copy_block(op_load_T1_GPR13_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR14
{
    static const uint8 op_load_T1_GPR14_code[] = {
       0x44, 0x8b, 0x6d, 0x48
    };
    copy_block(op_load_T1_GPR14_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR15
{
    static const uint8 op_load_T1_GPR15_code[] = {
       0x44, 0x8b, 0x6d, 0x4c
    };
    copy_block(op_load_T1_GPR15_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR16
{
    static const uint8 op_load_T1_GPR16_code[] = {
       0x44, 0x8b, 0x6d, 0x50
    };
    copy_block(op_load_T1_GPR16_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR17
{
    static const uint8 op_load_T1_GPR17_code[] = {
       0x44, 0x8b, 0x6d, 0x54
    };
    copy_block(op_load_T1_GPR17_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR18
{
    static const uint8 op_load_T1_GPR18_code[] = {
       0x44, 0x8b, 0x6d, 0x58
    };
    copy_block(op_load_T1_GPR18_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR19
{
    static const uint8 op_load_T1_GPR19_code[] = {
       0x44, 0x8b, 0x6d, 0x5c
    };
    copy_block(op_load_T1_GPR19_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR20
{
    static const uint8 op_load_T1_GPR20_code[] = {
       0x44, 0x8b, 0x6d, 0x60
    };
    copy_block(op_load_T1_GPR20_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR21
{
    static const uint8 op_load_T1_GPR21_code[] = {
       0x44, 0x8b, 0x6d, 0x64
    };
    copy_block(op_load_T1_GPR21_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR22
{
    static const uint8 op_load_T1_GPR22_code[] = {
       0x44, 0x8b, 0x6d, 0x68
    };
    copy_block(op_load_T1_GPR22_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR23
{
    static const uint8 op_load_T1_GPR23_code[] = {
       0x44, 0x8b, 0x6d, 0x6c
    };
    copy_block(op_load_T1_GPR23_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR24
{
    static const uint8 op_load_T1_GPR24_code[] = {
       0x44, 0x8b, 0x6d, 0x70
    };
    copy_block(op_load_T1_GPR24_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR25
{
    static const uint8 op_load_T1_GPR25_code[] = {
       0x44, 0x8b, 0x6d, 0x74
    };
    copy_block(op_load_T1_GPR25_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR26
{
    static const uint8 op_load_T1_GPR26_code[] = {
       0x44, 0x8b, 0x6d, 0x78
    };
    copy_block(op_load_T1_GPR26_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR27
{
    static const uint8 op_load_T1_GPR27_code[] = {
       0x44, 0x8b, 0x6d, 0x7c
    };
    copy_block(op_load_T1_GPR27_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR28
{
    static const uint8 op_load_T1_GPR28_code[] = {
       0x44, 0x8b, 0xad, 0x80, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T1_GPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR29
{
    static const uint8 op_load_T1_GPR29_code[] = {
       0x44, 0x8b, 0xad, 0x84, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T1_GPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR30
{
    static const uint8 op_load_T1_GPR30_code[] = {
       0x44, 0x8b, 0xad, 0x88, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T1_GPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T1_GPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_GPR31
{
    static const uint8 op_load_T1_GPR31_code[] = {
       0x44, 0x8b, 0xad, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T1_GPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb10
{
    static const uint8 op_load_T1_crb10_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x15, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb10_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb11
{
    static const uint8 op_load_T1_crb11_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x14, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb11_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb12
{
    static const uint8 op_load_T1_crb12_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x13, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb12_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb13
{
    static const uint8 op_load_T1_crb13_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x12, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb13_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb14
{
    static const uint8 op_load_T1_crb14_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x11, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb14_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb15
{
    static const uint8 op_load_T1_crb15_code[] = {
       0x0f, 0xb7, 0x85, 0x92, 0x03, 0x00, 0x00, 0x41, 0x89, 0xc5, 0x41, 0x83,
       0xe5, 0x01
    };
    copy_block(op_load_T1_crb15_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb16
{
    static const uint8 op_load_T1_crb16_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0f, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb16_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb17
{
    static const uint8 op_load_T1_crb17_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0e, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb17_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb18
{
    static const uint8 op_load_T1_crb18_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0d, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb18_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb19
{
    static const uint8 op_load_T1_crb19_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0c, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb19_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb20
{
    static const uint8 op_load_T1_crb20_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0b, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb20_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb21
{
    static const uint8 op_load_T1_crb21_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x0a, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb21_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb22
{
    static const uint8 op_load_T1_crb22_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x09, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb22_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb23
{
    static const uint8 op_load_T1_crb23_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x08, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb23_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb24
{
    static const uint8 op_load_T1_crb24_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x07, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb24_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb25
{
    static const uint8 op_load_T1_crb25_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x06, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb25_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb26
{
    static const uint8 op_load_T1_crb26_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x05, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb26_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb27
{
    static const uint8 op_load_T1_crb27_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x04, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb27_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb28
{
    static const uint8 op_load_T1_crb28_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x03, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb28_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb29
{
    static const uint8 op_load_T1_crb29_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xc1, 0xe8, 0x02, 0x41, 0x89, 0xc5,
       0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb29_code, 16);
    inc_code_ptr(16);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb30
{
    static const uint8 op_load_T1_crb30_code[] = {
       0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0xd1, 0xe8, 0x41, 0x89, 0xc5, 0x41,
       0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb30_code, 15);
    inc_code_ptr(15);
}
#endif

DEFINE_GEN(gen_op_load_T1_crb31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T1_crb31
{
    static const uint8 op_load_T1_crb31_code[] = {
       0x44, 0x8b, 0xad, 0x90, 0x03, 0x00, 0x00, 0x41, 0x83, 0xe5, 0x01
    };
    copy_block(op_load_T1_crb31_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR10
{
    static const uint8 op_load_T2_GPR10_code[] = {
       0x44, 0x8b, 0x75, 0x38
    };
    copy_block(op_load_T2_GPR10_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR11
{
    static const uint8 op_load_T2_GPR11_code[] = {
       0x44, 0x8b, 0x75, 0x3c
    };
    copy_block(op_load_T2_GPR11_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR12
{
    static const uint8 op_load_T2_GPR12_code[] = {
       0x44, 0x8b, 0x75, 0x40
    };
    copy_block(op_load_T2_GPR12_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR13
{
    static const uint8 op_load_T2_GPR13_code[] = {
       0x44, 0x8b, 0x75, 0x44
    };
    copy_block(op_load_T2_GPR13_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR14
{
    static const uint8 op_load_T2_GPR14_code[] = {
       0x44, 0x8b, 0x75, 0x48
    };
    copy_block(op_load_T2_GPR14_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR15
{
    static const uint8 op_load_T2_GPR15_code[] = {
       0x44, 0x8b, 0x75, 0x4c
    };
    copy_block(op_load_T2_GPR15_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR16
{
    static const uint8 op_load_T2_GPR16_code[] = {
       0x44, 0x8b, 0x75, 0x50
    };
    copy_block(op_load_T2_GPR16_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR17
{
    static const uint8 op_load_T2_GPR17_code[] = {
       0x44, 0x8b, 0x75, 0x54
    };
    copy_block(op_load_T2_GPR17_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR18
{
    static const uint8 op_load_T2_GPR18_code[] = {
       0x44, 0x8b, 0x75, 0x58
    };
    copy_block(op_load_T2_GPR18_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR19
{
    static const uint8 op_load_T2_GPR19_code[] = {
       0x44, 0x8b, 0x75, 0x5c
    };
    copy_block(op_load_T2_GPR19_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR20
{
    static const uint8 op_load_T2_GPR20_code[] = {
       0x44, 0x8b, 0x75, 0x60
    };
    copy_block(op_load_T2_GPR20_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR21
{
    static const uint8 op_load_T2_GPR21_code[] = {
       0x44, 0x8b, 0x75, 0x64
    };
    copy_block(op_load_T2_GPR21_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR22
{
    static const uint8 op_load_T2_GPR22_code[] = {
       0x44, 0x8b, 0x75, 0x68
    };
    copy_block(op_load_T2_GPR22_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR23
{
    static const uint8 op_load_T2_GPR23_code[] = {
       0x44, 0x8b, 0x75, 0x6c
    };
    copy_block(op_load_T2_GPR23_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR24
{
    static const uint8 op_load_T2_GPR24_code[] = {
       0x44, 0x8b, 0x75, 0x70
    };
    copy_block(op_load_T2_GPR24_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR25
{
    static const uint8 op_load_T2_GPR25_code[] = {
       0x44, 0x8b, 0x75, 0x74
    };
    copy_block(op_load_T2_GPR25_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR26
{
    static const uint8 op_load_T2_GPR26_code[] = {
       0x44, 0x8b, 0x75, 0x78
    };
    copy_block(op_load_T2_GPR26_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR27
{
    static const uint8 op_load_T2_GPR27_code[] = {
       0x44, 0x8b, 0x75, 0x7c
    };
    copy_block(op_load_T2_GPR27_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR28
{
    static const uint8 op_load_T2_GPR28_code[] = {
       0x44, 0x8b, 0xb5, 0x80, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T2_GPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR29
{
    static const uint8 op_load_T2_GPR29_code[] = {
       0x44, 0x8b, 0xb5, 0x84, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T2_GPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR30
{
    static const uint8 op_load_T2_GPR30_code[] = {
       0x44, 0x8b, 0xb5, 0x88, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T2_GPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_T2_GPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T2_GPR31
{
    static const uint8 op_load_T2_GPR31_code[] = {
       0x44, 0x8b, 0xb5, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_load_T2_GPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_record_cr0_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_record_cr0_T0
{
    static const uint8 op_record_cr0_T0_code[] = {
       0x48, 0x89, 0xe9, 0x8b, 0x85, 0x90, 0x03, 0x00, 0x00, 0x25, 0xff, 0xff,
       0xff, 0x0f, 0x0f, 0xb6, 0x95, 0x94, 0x03, 0x00, 0x00, 0xc1, 0xe2, 0x1c,
       0x09, 0xc2, 0x41, 0x83, 0xfc, 0x00, 0x7d, 0x09, 0x89, 0xd0, 0x0d, 0x00,
       0x00, 0x00, 0x80, 0xeb, 0x13, 0x89, 0xd0, 0x0d, 0x00, 0x00, 0x00, 0x40,
       0x81, 0xca, 0x00, 0x00, 0x00, 0x20, 0x45, 0x85, 0xe4, 0x0f, 0x44, 0xc2,
       0x89, 0x81, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_record_cr0_T0_code, 66);
    inc_code_ptr(66);
}
#endif

DEFINE_GEN(gen_op_record_cr6_VD,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_record_cr6_VD
{
    static const uint8 op_record_cr6_VD_code[] = {
       0x49, 0x8b, 0x37, 0x49, 0x8b, 0x57, 0x08, 0x48, 0x89, 0xd0, 0x48, 0x21,
       0xf0, 0xb9, 0x08, 0x00, 0x00, 0x00, 0x48, 0xff, 0xc0, 0x74, 0x0c, 0x48,
       0x09, 0xf2, 0x48, 0x83, 0xfa, 0x01, 0x19, 0xc9, 0x83, 0xe1, 0x02, 0x48,
       0x8d, 0x55, 0x10, 0xc1, 0xe1, 0x04, 0x8b, 0x82, 0x80, 0x03, 0x00, 0x00,
       0x24, 0x0f, 0x09, 0xc1, 0x89, 0x8a, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_record_cr6_VD_code, 58);
    inc_code_ptr(58);
}
#endif

DEFINE_GEN(gen_op_spcflags_init,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_spcflags_init
{
    static const uint8 op_spcflags_init_code[] = {
       0x48, 0x8d, 0x8d, 0xb0, 0x03, 0x00, 0x00, 0x48, 0x8d, 0x95, 0xb4, 0x03,
       0x00, 0x00, 0xb8, 0x01, 0x00, 0x00, 0x00, 0x87, 0x02, 0x85, 0xc0, 0x75,
       0xf5, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0xc7, 0x41,
       0x04, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_spcflags_init_code, 41);
    *(uint32_t *)(code_ptr() + 28) = (int32_t)((long)param1 - (long)(code_ptr() + 28 + 4)) + 0;
    inc_code_ptr(41);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR0
{
    static const uint8 op_store_F0_FPR0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x90, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR0_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR1
{
    static const uint8 op_store_F0_FPR1_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x98, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR2
{
    static const uint8 op_store_F0_FPR2_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xa0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR2_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR3
{
    static const uint8 op_store_F0_FPR3_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xa8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR3_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR4
{
    static const uint8 op_store_F0_FPR4_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xb0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR4_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR5
{
    static const uint8 op_store_F0_FPR5_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xb8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR5_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR6
{
    static const uint8 op_store_F0_FPR6_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xc0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR6_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR7
{
    static const uint8 op_store_F0_FPR7_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xc8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR7_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR8
{
    static const uint8 op_store_F0_FPR8_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xd0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR8_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR9
{
    static const uint8 op_store_F0_FPR9_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xd8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR9_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR0
{
    static const uint8 op_store_F1_FPR0_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x90, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR0_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR1
{
    static const uint8 op_store_F1_FPR1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x98, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR1_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR2
{
    static const uint8 op_store_F1_FPR2_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xa0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR2_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR3
{
    static const uint8 op_store_F1_FPR3_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xa8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR3_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR4
{
    static const uint8 op_store_F1_FPR4_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xb0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR4_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR5
{
    static const uint8 op_store_F1_FPR5_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xb8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR5_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR6
{
    static const uint8 op_store_F1_FPR6_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xc0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR6_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR7
{
    static const uint8 op_store_F1_FPR7_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xc8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR7_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR8
{
    static const uint8 op_store_F1_FPR8_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xd0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR8_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR9
{
    static const uint8 op_store_F1_FPR9_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xd8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR9_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR0
{
    static const uint8 op_store_F2_FPR0_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x90, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR0_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR1
{
    static const uint8 op_store_F2_FPR1_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x98, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR1_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR2
{
    static const uint8 op_store_F2_FPR2_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xa0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR2_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR3
{
    static const uint8 op_store_F2_FPR3_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xa8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR3_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR4
{
    static const uint8 op_store_F2_FPR4_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xb0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR4_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR5
{
    static const uint8 op_store_F2_FPR5_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xb8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR5_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR6
{
    static const uint8 op_store_F2_FPR6_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xc0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR6_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR7
{
    static const uint8 op_store_F2_FPR7_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xc8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR7_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR8
{
    static const uint8 op_store_F2_FPR8_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xd0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR8_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR9
{
    static const uint8 op_store_F2_FPR9_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xd8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR9_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR0
{
    static const uint8 op_store_FD_FPR0_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x90, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR0_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR1
{
    static const uint8 op_store_FD_FPR1_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x98, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR1_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR2
{
    static const uint8 op_store_FD_FPR2_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xa0, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR2_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR3
{
    static const uint8 op_store_FD_FPR3_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xa8, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR3_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR4
{
    static const uint8 op_store_FD_FPR4_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xb0, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR4_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR5
{
    static const uint8 op_store_FD_FPR5_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xb8, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR5_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR6
{
    static const uint8 op_store_FD_FPR6_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xc0, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR6_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR7
{
    static const uint8 op_store_FD_FPR7_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xc8, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR7_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR8
{
    static const uint8 op_store_FD_FPR8_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xd0, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR8_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR9
{
    static const uint8 op_store_FD_FPR9_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xd8, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR9_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR0
{
    static const uint8 op_store_T0_GPR0_code[] = {
       0x44, 0x89, 0x65, 0x10
    };
    copy_block(op_store_T0_GPR0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR1
{
    static const uint8 op_store_T0_GPR1_code[] = {
       0x44, 0x89, 0x65, 0x14
    };
    copy_block(op_store_T0_GPR1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR2
{
    static const uint8 op_store_T0_GPR2_code[] = {
       0x44, 0x89, 0x65, 0x18
    };
    copy_block(op_store_T0_GPR2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR3
{
    static const uint8 op_store_T0_GPR3_code[] = {
       0x44, 0x89, 0x65, 0x1c
    };
    copy_block(op_store_T0_GPR3_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR4
{
    static const uint8 op_store_T0_GPR4_code[] = {
       0x44, 0x89, 0x65, 0x20
    };
    copy_block(op_store_T0_GPR4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR5
{
    static const uint8 op_store_T0_GPR5_code[] = {
       0x44, 0x89, 0x65, 0x24
    };
    copy_block(op_store_T0_GPR5_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR6
{
    static const uint8 op_store_T0_GPR6_code[] = {
       0x44, 0x89, 0x65, 0x28
    };
    copy_block(op_store_T0_GPR6_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR7
{
    static const uint8 op_store_T0_GPR7_code[] = {
       0x44, 0x89, 0x65, 0x2c
    };
    copy_block(op_store_T0_GPR7_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR8
{
    static const uint8 op_store_T0_GPR8_code[] = {
       0x44, 0x89, 0x65, 0x30
    };
    copy_block(op_store_T0_GPR8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR9
{
    static const uint8 op_store_T0_GPR9_code[] = {
       0x44, 0x89, 0x65, 0x34
    };
    copy_block(op_store_T0_GPR9_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb0
{
    static const uint8 op_store_T0_crb0_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0x7f,
       0x44, 0x89, 0xe1, 0xc1, 0xe1, 0x1f, 0x09, 0xca, 0x89, 0x95, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb0_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb1
{
    static const uint8 op_store_T0_crb1_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xbf,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1e, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb1_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb2
{
    static const uint8 op_store_T0_crb2_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xdf,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1d, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb2_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb3
{
    static const uint8 op_store_T0_crb3_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xef,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1c, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb3_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb4
{
    static const uint8 op_store_T0_crb4_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xf7,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1b, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb4_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb5
{
    static const uint8 op_store_T0_crb5_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xfb,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1a, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb5_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb6
{
    static const uint8 op_store_T0_crb6_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xfd,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x19, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb6_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb7
{
    static const uint8 op_store_T0_crb7_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xfe,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x18, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb7_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb8
{
    static const uint8 op_store_T0_crb8_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0x7f, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x17, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb8_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb9
{
    static const uint8 op_store_T0_crb9_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xbf, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x16, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb9_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR0
{
    static const uint8 op_store_T1_GPR0_code[] = {
       0x44, 0x89, 0x6d, 0x10
    };
    copy_block(op_store_T1_GPR0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR1
{
    static const uint8 op_store_T1_GPR1_code[] = {
       0x44, 0x89, 0x6d, 0x14
    };
    copy_block(op_store_T1_GPR1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR2
{
    static const uint8 op_store_T1_GPR2_code[] = {
       0x44, 0x89, 0x6d, 0x18
    };
    copy_block(op_store_T1_GPR2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR3
{
    static const uint8 op_store_T1_GPR3_code[] = {
       0x44, 0x89, 0x6d, 0x1c
    };
    copy_block(op_store_T1_GPR3_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR4
{
    static const uint8 op_store_T1_GPR4_code[] = {
       0x44, 0x89, 0x6d, 0x20
    };
    copy_block(op_store_T1_GPR4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR5
{
    static const uint8 op_store_T1_GPR5_code[] = {
       0x44, 0x89, 0x6d, 0x24
    };
    copy_block(op_store_T1_GPR5_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR6
{
    static const uint8 op_store_T1_GPR6_code[] = {
       0x44, 0x89, 0x6d, 0x28
    };
    copy_block(op_store_T1_GPR6_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR7
{
    static const uint8 op_store_T1_GPR7_code[] = {
       0x44, 0x89, 0x6d, 0x2c
    };
    copy_block(op_store_T1_GPR7_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR8
{
    static const uint8 op_store_T1_GPR8_code[] = {
       0x44, 0x89, 0x6d, 0x30
    };
    copy_block(op_store_T1_GPR8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR9
{
    static const uint8 op_store_T1_GPR9_code[] = {
       0x44, 0x89, 0x6d, 0x34
    };
    copy_block(op_store_T1_GPR9_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb0
{
    static const uint8 op_store_T1_crb0_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0x7f,
       0x44, 0x89, 0xe9, 0xc1, 0xe1, 0x1f, 0x09, 0xca, 0x89, 0x95, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb0_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb1
{
    static const uint8 op_store_T1_crb1_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xbf,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1e, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb1_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb2
{
    static const uint8 op_store_T1_crb2_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xdf,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1d, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb2_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb3
{
    static const uint8 op_store_T1_crb3_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xef,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1c, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb3_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb4
{
    static const uint8 op_store_T1_crb4_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xf7,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1b, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb4_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb5
{
    static const uint8 op_store_T1_crb5_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xfb,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x1a, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb5_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb6
{
    static const uint8 op_store_T1_crb6_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xfd,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x19, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb6_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb7
{
    static const uint8 op_store_T1_crb7_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xff, 0xfe,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x18, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb7_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb8
{
    static const uint8 op_store_T1_crb8_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0x7f, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x17, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb8_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb9
{
    static const uint8 op_store_T1_crb9_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xbf, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x16, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb9_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR0
{
    static const uint8 op_store_T2_GPR0_code[] = {
       0x44, 0x89, 0x75, 0x10
    };
    copy_block(op_store_T2_GPR0_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR1
{
    static const uint8 op_store_T2_GPR1_code[] = {
       0x44, 0x89, 0x75, 0x14
    };
    copy_block(op_store_T2_GPR1_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR2
{
    static const uint8 op_store_T2_GPR2_code[] = {
       0x44, 0x89, 0x75, 0x18
    };
    copy_block(op_store_T2_GPR2_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR3
{
    static const uint8 op_store_T2_GPR3_code[] = {
       0x44, 0x89, 0x75, 0x1c
    };
    copy_block(op_store_T2_GPR3_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR4
{
    static const uint8 op_store_T2_GPR4_code[] = {
       0x44, 0x89, 0x75, 0x20
    };
    copy_block(op_store_T2_GPR4_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR5
{
    static const uint8 op_store_T2_GPR5_code[] = {
       0x44, 0x89, 0x75, 0x24
    };
    copy_block(op_store_T2_GPR5_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR6
{
    static const uint8 op_store_T2_GPR6_code[] = {
       0x44, 0x89, 0x75, 0x28
    };
    copy_block(op_store_T2_GPR6_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR7
{
    static const uint8 op_store_T2_GPR7_code[] = {
       0x44, 0x89, 0x75, 0x2c
    };
    copy_block(op_store_T2_GPR7_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR8
{
    static const uint8 op_store_T2_GPR8_code[] = {
       0x44, 0x89, 0x75, 0x30
    };
    copy_block(op_store_T2_GPR8_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR9
{
    static const uint8 op_store_T2_GPR9_code[] = {
       0x44, 0x89, 0x75, 0x34
    };
    copy_block(op_store_T2_GPR9_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_vand_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vand_VD_V0_V1
{
    static const uint8 op_vand_VD_V0_V1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x49, 0x23, 0x04, 0x24, 0x49, 0x89, 0x07, 0x49,
       0x8b, 0x45, 0x08, 0x49, 0x23, 0x44, 0x24, 0x08, 0x49, 0x89, 0x47, 0x08
    };
    copy_block(op_vand_VD_V0_V1_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_vnor_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vnor_VD_V0_V1
{
    static const uint8 op_vnor_VD_V0_V1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x49, 0x0b, 0x04, 0x24, 0x48, 0xf7, 0xd0, 0x49,
       0x89, 0x07, 0x49, 0x8b, 0x45, 0x08, 0x49, 0x0b, 0x44, 0x24, 0x08, 0x48,
       0xf7, 0xd0, 0x49, 0x89, 0x47, 0x08
    };
    copy_block(op_vnor_VD_V0_V1_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_vxor_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vxor_VD_V0_V1
{
    static const uint8 op_vxor_VD_V0_V1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x49, 0x33, 0x04, 0x24, 0x49, 0x89, 0x07, 0x49,
       0x8b, 0x45, 0x08, 0x49, 0x33, 0x44, 0x24, 0x08, 0x49, 0x89, 0x47, 0x08
    };
    copy_block(op_vxor_VD_V0_V1_code, 24);
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_branch_2_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_branch_2_T0_im
{
    static const uint8 op_branch_2_T0_im_code[] = {
       0x45, 0x85, 0xed, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x0f, 0x45,
       0xc4, 0x89, 0x85, 0xac, 0x03, 0x00, 0x00
    };
    copy_block(op_branch_2_T0_im_code, 19);
    *(uint32_t *)(code_ptr() + 5) = (int32_t)((long)param1 - (long)(code_ptr() + 5 + 4)) + 0;
    inc_code_ptr(19);
}
#endif

DEFINE_GEN(gen_op_branch_2_im_im,void,(long param1, long param2))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_branch_2_im_im
{
    static const uint8 op_branch_2_im_im_code[] = {
       0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x45, 0x85, 0xed, 0x8d, 0x15, 0x00,
       0x00, 0x00, 0x00, 0x0f, 0x45, 0xc2, 0x89, 0x85, 0xac, 0x03, 0x00, 0x00
    };
    copy_block(op_branch_2_im_im_code, 24);
    *(uint32_t *)(code_ptr() + 11) = (int32_t)((long)param1 - (long)(code_ptr() + 11 + 4)) + 0;
    *(uint32_t *)(code_ptr() + 2) = (int32_t)((long)param2 - (long)(code_ptr() + 2 + 4)) + 0;
    inc_code_ptr(24);
}
#endif

DEFINE_GEN(gen_op_branch_chain_1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_branch_chain_1
{
    static const uint8 op_branch_chain_1_code[] = {
       0xe9, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_branch_chain_1_code, 5);
    jmp_addr[0] = code_ptr() + 1;
    *(uint32_t *)(code_ptr() + 1) = 0;
    inc_code_ptr(5);
}
#endif

DEFINE_GEN(gen_op_branch_chain_2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_branch_chain_2
{
    static const uint8 op_branch_chain_2_code[] = {
       0x45, 0x85, 0xed, 0x74, 0x07, 0xe9, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x05,
       0xe9, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_branch_chain_2_code, 17);
    jmp_addr[1] = code_ptr() + 13;
    *(uint32_t *)(code_ptr() + 13) = 0;
    jmp_addr[0] = code_ptr() + 6;
    *(uint32_t *)(code_ptr() + 6) = 0;
    inc_code_ptr(17);
}
#endif

DEFINE_GEN(gen_op_fadds_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fadds_FD_F0_F1
{
    static const uint8 op_fadds_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x58, 0x45, 0x00,
       0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x85,
       0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fadds_FD_F0_F1_code, 28);
    inc_code_ptr(28);
}
#endif

DEFINE_GEN(gen_op_fdivs_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fdivs_FD_F0_F1
{
    static const uint8 op_fdivs_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x5e, 0x45, 0x00,
       0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x85,
       0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fdivs_FD_F0_F1_code, 28);
    inc_code_ptr(28);
}
#endif

DEFINE_GEN(gen_op_fmuls_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmuls_FD_F0_F1
{
    static const uint8 op_fmuls_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x85,
       0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmuls_FD_F0_F1_code, 28);
    inc_code_ptr(28);
}
#endif

DEFINE_GEN(gen_op_fsubs_FD_F0_F1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fsubs_FD_F0_F1
{
    static const uint8 op_fsubs_FD_F0_F1_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x5c, 0x45, 0x00,
       0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x85,
       0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fsubs_FD_F0_F1_code, 28);
    inc_code_ptr(28);
}
#endif

DEFINE_GEN(gen_op_load_T0_VRSAVE,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_VRSAVE
{
    static const uint8 op_load_T0_VRSAVE_code[] = {
       0x44, 0x8b, 0xa5, 0x9c, 0x03, 0x00, 0x00
    };
    copy_block(op_load_T0_VRSAVE_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR0
{
    static const uint8 op_load_ad_V0_VR0_code[] = {
       0x4c, 0x8d, 0xa5, 0x90, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR1
{
    static const uint8 op_load_ad_V0_VR1_code[] = {
       0x4c, 0x8d, 0xa5, 0xa0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR2
{
    static const uint8 op_load_ad_V0_VR2_code[] = {
       0x4c, 0x8d, 0xa5, 0xb0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR3
{
    static const uint8 op_load_ad_V0_VR3_code[] = {
       0x4c, 0x8d, 0xa5, 0xc0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR4
{
    static const uint8 op_load_ad_V0_VR4_code[] = {
       0x4c, 0x8d, 0xa5, 0xd0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR5
{
    static const uint8 op_load_ad_V0_VR5_code[] = {
       0x4c, 0x8d, 0xa5, 0xe0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR6
{
    static const uint8 op_load_ad_V0_VR6_code[] = {
       0x4c, 0x8d, 0xa5, 0xf0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR7
{
    static const uint8 op_load_ad_V0_VR7_code[] = {
       0x4c, 0x8d, 0xa5, 0x00, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR8
{
    static const uint8 op_load_ad_V0_VR8_code[] = {
       0x4c, 0x8d, 0xa5, 0x10, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR9
{
    static const uint8 op_load_ad_V0_VR9_code[] = {
       0x4c, 0x8d, 0xa5, 0x20, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR0
{
    static const uint8 op_load_ad_V1_VR0_code[] = {
       0x4c, 0x8d, 0xad, 0x90, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR1
{
    static const uint8 op_load_ad_V1_VR1_code[] = {
       0x4c, 0x8d, 0xad, 0xa0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR2
{
    static const uint8 op_load_ad_V1_VR2_code[] = {
       0x4c, 0x8d, 0xad, 0xb0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR3
{
    static const uint8 op_load_ad_V1_VR3_code[] = {
       0x4c, 0x8d, 0xad, 0xc0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR4
{
    static const uint8 op_load_ad_V1_VR4_code[] = {
       0x4c, 0x8d, 0xad, 0xd0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR5
{
    static const uint8 op_load_ad_V1_VR5_code[] = {
       0x4c, 0x8d, 0xad, 0xe0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR6
{
    static const uint8 op_load_ad_V1_VR6_code[] = {
       0x4c, 0x8d, 0xad, 0xf0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR7
{
    static const uint8 op_load_ad_V1_VR7_code[] = {
       0x4c, 0x8d, 0xad, 0x00, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR8
{
    static const uint8 op_load_ad_V1_VR8_code[] = {
       0x4c, 0x8d, 0xad, 0x10, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR9
{
    static const uint8 op_load_ad_V1_VR9_code[] = {
       0x4c, 0x8d, 0xad, 0x20, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR0
{
    static const uint8 op_load_ad_V2_VR0_code[] = {
       0x4c, 0x8d, 0xb5, 0x90, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR1
{
    static const uint8 op_load_ad_V2_VR1_code[] = {
       0x4c, 0x8d, 0xb5, 0xa0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR2
{
    static const uint8 op_load_ad_V2_VR2_code[] = {
       0x4c, 0x8d, 0xb5, 0xb0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR3
{
    static const uint8 op_load_ad_V2_VR3_code[] = {
       0x4c, 0x8d, 0xb5, 0xc0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR4
{
    static const uint8 op_load_ad_V2_VR4_code[] = {
       0x4c, 0x8d, 0xb5, 0xd0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR5
{
    static const uint8 op_load_ad_V2_VR5_code[] = {
       0x4c, 0x8d, 0xb5, 0xe0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR6
{
    static const uint8 op_load_ad_V2_VR6_code[] = {
       0x4c, 0x8d, 0xb5, 0xf0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR7
{
    static const uint8 op_load_ad_V2_VR7_code[] = {
       0x4c, 0x8d, 0xb5, 0x00, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR8
{
    static const uint8 op_load_ad_V2_VR8_code[] = {
       0x4c, 0x8d, 0xb5, 0x10, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR9
{
    static const uint8 op_load_ad_V2_VR9_code[] = {
       0x4c, 0x8d, 0xb5, 0x20, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR0
{
    static const uint8 op_load_ad_VD_VR0_code[] = {
       0x4c, 0x8d, 0xbd, 0x90, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR0_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR1
{
    static const uint8 op_load_ad_VD_VR1_code[] = {
       0x4c, 0x8d, 0xbd, 0xa0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR1_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR2
{
    static const uint8 op_load_ad_VD_VR2_code[] = {
       0x4c, 0x8d, 0xbd, 0xb0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR2_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR3,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR3
{
    static const uint8 op_load_ad_VD_VR3_code[] = {
       0x4c, 0x8d, 0xbd, 0xc0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR3_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR4,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR4
{
    static const uint8 op_load_ad_VD_VR4_code[] = {
       0x4c, 0x8d, 0xbd, 0xd0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR4_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR5,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR5
{
    static const uint8 op_load_ad_VD_VR5_code[] = {
       0x4c, 0x8d, 0xbd, 0xe0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR5_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR6,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR6
{
    static const uint8 op_load_ad_VD_VR6_code[] = {
       0x4c, 0x8d, 0xbd, 0xf0, 0x01, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR6_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR7,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR7
{
    static const uint8 op_load_ad_VD_VR7_code[] = {
       0x4c, 0x8d, 0xbd, 0x00, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR7_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR8,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR8
{
    static const uint8 op_load_ad_VD_VR8_code[] = {
       0x4c, 0x8d, 0xbd, 0x10, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR8_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR9,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR9
{
    static const uint8 op_load_ad_VD_VR9_code[] = {
       0x4c, 0x8d, 0xbd, 0x20, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR9_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_spcflags_check,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_spcflags_check
{
    static const uint8 op_spcflags_check_code[] = {
       0x8b, 0x85, 0xb0, 0x03, 0x00, 0x00, 0x85, 0xc0, 0x0f, 0x84, 0x00, 0x00,
       0x00, 0x00
    };
    copy_block(op_spcflags_check_code, 14);
    jmp_addr[0] = code_ptr() + 10;
    *(uint32_t *)(code_ptr() + 10) = 0;
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_spcflags_clear,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_spcflags_clear
{
    static const uint8 op_spcflags_clear_code[] = {
       0x48, 0x8d, 0x8d, 0xb0, 0x03, 0x00, 0x00, 0x48, 0x8d, 0x95, 0xb4, 0x03,
       0x00, 0x00, 0xb8, 0x01, 0x00, 0x00, 0x00, 0x87, 0x02, 0x85, 0xc0, 0x75,
       0xf5, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0xf7, 0xd0, 0x21, 0x01,
       0xc7, 0x41, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    copy_block(op_spcflags_clear_code, 43);
    *(uint32_t *)(code_ptr() + 28) = (int32_t)((long)param1 - (long)(code_ptr() + 28 + 4)) + 0;
    inc_code_ptr(43);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR10
{
    static const uint8 op_store_F0_FPR10_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xe0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR10_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR11
{
    static const uint8 op_store_F0_FPR11_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xe8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR11_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR12
{
    static const uint8 op_store_F0_FPR12_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xf0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR12_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR13
{
    static const uint8 op_store_F0_FPR13_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0xf8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR13_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR14
{
    static const uint8 op_store_F0_FPR14_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x00, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR14_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR15
{
    static const uint8 op_store_F0_FPR15_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x08, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR15_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR16
{
    static const uint8 op_store_F0_FPR16_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x10, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR16_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR17
{
    static const uint8 op_store_F0_FPR17_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x18, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR17_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR18
{
    static const uint8 op_store_F0_FPR18_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x20, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR18_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR19
{
    static const uint8 op_store_F0_FPR19_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x28, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR19_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR20
{
    static const uint8 op_store_F0_FPR20_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x30, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR20_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR21
{
    static const uint8 op_store_F0_FPR21_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x38, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR21_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR22
{
    static const uint8 op_store_F0_FPR22_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x40, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR22_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR23
{
    static const uint8 op_store_F0_FPR23_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x48, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR23_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR24
{
    static const uint8 op_store_F0_FPR24_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x50, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR24_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR25
{
    static const uint8 op_store_F0_FPR25_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x58, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR25_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR26
{
    static const uint8 op_store_F0_FPR26_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x60, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR26_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR27
{
    static const uint8 op_store_F0_FPR27_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x68, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR27_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR28
{
    static const uint8 op_store_F0_FPR28_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x70, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR28_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR29
{
    static const uint8 op_store_F0_FPR29_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x78, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR29_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR30
{
    static const uint8 op_store_F0_FPR30_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x80, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR30_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F0_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F0_FPR31
{
    static const uint8 op_store_F0_FPR31_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x48, 0x89, 0x85, 0x88, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F0_FPR31_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR10
{
    static const uint8 op_store_F1_FPR10_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xe0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR10_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR11
{
    static const uint8 op_store_F1_FPR11_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xe8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR11_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR12
{
    static const uint8 op_store_F1_FPR12_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xf0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR12_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR13
{
    static const uint8 op_store_F1_FPR13_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0xf8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR13_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR14
{
    static const uint8 op_store_F1_FPR14_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x00, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR14_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR15
{
    static const uint8 op_store_F1_FPR15_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x08, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR15_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR16
{
    static const uint8 op_store_F1_FPR16_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x10, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR16_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR17
{
    static const uint8 op_store_F1_FPR17_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x18, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR17_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR18
{
    static const uint8 op_store_F1_FPR18_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x20, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR18_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR19
{
    static const uint8 op_store_F1_FPR19_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x28, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR19_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR20
{
    static const uint8 op_store_F1_FPR20_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x30, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR20_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR21
{
    static const uint8 op_store_F1_FPR21_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x38, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR21_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR22
{
    static const uint8 op_store_F1_FPR22_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x40, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR22_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR23
{
    static const uint8 op_store_F1_FPR23_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x48, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR23_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR24
{
    static const uint8 op_store_F1_FPR24_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x50, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR24_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR25
{
    static const uint8 op_store_F1_FPR25_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x58, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR25_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR26
{
    static const uint8 op_store_F1_FPR26_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x60, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR26_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR27
{
    static const uint8 op_store_F1_FPR27_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x68, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR27_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR28
{
    static const uint8 op_store_F1_FPR28_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x70, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR28_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR29
{
    static const uint8 op_store_F1_FPR29_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x78, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR29_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR30
{
    static const uint8 op_store_F1_FPR30_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x80, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR30_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F1_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F1_FPR31
{
    static const uint8 op_store_F1_FPR31_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0x89, 0x85, 0x88, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F1_FPR31_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR10
{
    static const uint8 op_store_F2_FPR10_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xe0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR10_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR11
{
    static const uint8 op_store_F2_FPR11_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xe8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR11_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR12
{
    static const uint8 op_store_F2_FPR12_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xf0, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR12_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR13
{
    static const uint8 op_store_F2_FPR13_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0xf8, 0x00, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR13_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR14
{
    static const uint8 op_store_F2_FPR14_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x00, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR14_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR15
{
    static const uint8 op_store_F2_FPR15_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x08, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR15_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR16
{
    static const uint8 op_store_F2_FPR16_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x10, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR16_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR17
{
    static const uint8 op_store_F2_FPR17_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x18, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR17_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR18
{
    static const uint8 op_store_F2_FPR18_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x20, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR18_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR19
{
    static const uint8 op_store_F2_FPR19_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x28, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR19_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR20
{
    static const uint8 op_store_F2_FPR20_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x30, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR20_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR21
{
    static const uint8 op_store_F2_FPR21_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x38, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR21_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR22
{
    static const uint8 op_store_F2_FPR22_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x40, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR22_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR23
{
    static const uint8 op_store_F2_FPR23_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x48, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR23_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR24
{
    static const uint8 op_store_F2_FPR24_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x50, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR24_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR25
{
    static const uint8 op_store_F2_FPR25_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x58, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR25_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR26
{
    static const uint8 op_store_F2_FPR26_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x60, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR26_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR27
{
    static const uint8 op_store_F2_FPR27_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x68, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR27_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR28
{
    static const uint8 op_store_F2_FPR28_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x70, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR28_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR29
{
    static const uint8 op_store_F2_FPR29_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x78, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR29_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR30
{
    static const uint8 op_store_F2_FPR30_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x80, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR30_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_F2_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_F2_FPR31
{
    static const uint8 op_store_F2_FPR31_code[] = {
       0x49, 0x8b, 0x06, 0x48, 0x89, 0x85, 0x88, 0x01, 0x00, 0x00
    };
    copy_block(op_store_F2_FPR31_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR10
{
    static const uint8 op_store_FD_FPR10_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xe0, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR10_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR11
{
    static const uint8 op_store_FD_FPR11_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xe8, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR11_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR12
{
    static const uint8 op_store_FD_FPR12_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xf0, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR12_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR13
{
    static const uint8 op_store_FD_FPR13_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0xf8, 0x00,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR13_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR14
{
    static const uint8 op_store_FD_FPR14_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x00, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR14_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR15
{
    static const uint8 op_store_FD_FPR15_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x08, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR15_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR16
{
    static const uint8 op_store_FD_FPR16_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x10, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR16_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR17
{
    static const uint8 op_store_FD_FPR17_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x18, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR17_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR18
{
    static const uint8 op_store_FD_FPR18_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x20, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR18_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR19
{
    static const uint8 op_store_FD_FPR19_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x28, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR19_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR20
{
    static const uint8 op_store_FD_FPR20_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x30, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR20_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR21
{
    static const uint8 op_store_FD_FPR21_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x38, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR21_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR22
{
    static const uint8 op_store_FD_FPR22_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x40, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR22_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR23
{
    static const uint8 op_store_FD_FPR23_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x48, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR23_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR24
{
    static const uint8 op_store_FD_FPR24_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x50, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR24_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR25
{
    static const uint8 op_store_FD_FPR25_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x58, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR25_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR26
{
    static const uint8 op_store_FD_FPR26_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x60, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR26_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR27
{
    static const uint8 op_store_FD_FPR27_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x68, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR27_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR28
{
    static const uint8 op_store_FD_FPR28_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x70, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR28_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR29
{
    static const uint8 op_store_FD_FPR29_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x78, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR29_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR30
{
    static const uint8 op_store_FD_FPR30_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x80, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR30_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_FD_FPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_FD_FPR31
{
    static const uint8 op_store_FD_FPR31_code[] = {
       0x48, 0x8b, 0x85, 0xa8, 0x08, 0x10, 0x00, 0x48, 0x89, 0x85, 0x88, 0x01,
       0x00, 0x00
    };
    copy_block(op_store_FD_FPR31_code, 14);
    inc_code_ptr(14);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR10
{
    static const uint8 op_store_T0_GPR10_code[] = {
       0x44, 0x89, 0x65, 0x38
    };
    copy_block(op_store_T0_GPR10_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR11
{
    static const uint8 op_store_T0_GPR11_code[] = {
       0x44, 0x89, 0x65, 0x3c
    };
    copy_block(op_store_T0_GPR11_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR12
{
    static const uint8 op_store_T0_GPR12_code[] = {
       0x44, 0x89, 0x65, 0x40
    };
    copy_block(op_store_T0_GPR12_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR13
{
    static const uint8 op_store_T0_GPR13_code[] = {
       0x44, 0x89, 0x65, 0x44
    };
    copy_block(op_store_T0_GPR13_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR14
{
    static const uint8 op_store_T0_GPR14_code[] = {
       0x44, 0x89, 0x65, 0x48
    };
    copy_block(op_store_T0_GPR14_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR15
{
    static const uint8 op_store_T0_GPR15_code[] = {
       0x44, 0x89, 0x65, 0x4c
    };
    copy_block(op_store_T0_GPR15_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR16
{
    static const uint8 op_store_T0_GPR16_code[] = {
       0x44, 0x89, 0x65, 0x50
    };
    copy_block(op_store_T0_GPR16_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR17
{
    static const uint8 op_store_T0_GPR17_code[] = {
       0x44, 0x89, 0x65, 0x54
    };
    copy_block(op_store_T0_GPR17_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR18
{
    static const uint8 op_store_T0_GPR18_code[] = {
       0x44, 0x89, 0x65, 0x58
    };
    copy_block(op_store_T0_GPR18_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR19
{
    static const uint8 op_store_T0_GPR19_code[] = {
       0x44, 0x89, 0x65, 0x5c
    };
    copy_block(op_store_T0_GPR19_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR20
{
    static const uint8 op_store_T0_GPR20_code[] = {
       0x44, 0x89, 0x65, 0x60
    };
    copy_block(op_store_T0_GPR20_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR21
{
    static const uint8 op_store_T0_GPR21_code[] = {
       0x44, 0x89, 0x65, 0x64
    };
    copy_block(op_store_T0_GPR21_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR22
{
    static const uint8 op_store_T0_GPR22_code[] = {
       0x44, 0x89, 0x65, 0x68
    };
    copy_block(op_store_T0_GPR22_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR23
{
    static const uint8 op_store_T0_GPR23_code[] = {
       0x44, 0x89, 0x65, 0x6c
    };
    copy_block(op_store_T0_GPR23_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR24
{
    static const uint8 op_store_T0_GPR24_code[] = {
       0x44, 0x89, 0x65, 0x70
    };
    copy_block(op_store_T0_GPR24_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR25
{
    static const uint8 op_store_T0_GPR25_code[] = {
       0x44, 0x89, 0x65, 0x74
    };
    copy_block(op_store_T0_GPR25_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR26
{
    static const uint8 op_store_T0_GPR26_code[] = {
       0x44, 0x89, 0x65, 0x78
    };
    copy_block(op_store_T0_GPR26_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR27
{
    static const uint8 op_store_T0_GPR27_code[] = {
       0x44, 0x89, 0x65, 0x7c
    };
    copy_block(op_store_T0_GPR27_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR28
{
    static const uint8 op_store_T0_GPR28_code[] = {
       0x44, 0x89, 0xa5, 0x80, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T0_GPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR29
{
    static const uint8 op_store_T0_GPR29_code[] = {
       0x44, 0x89, 0xa5, 0x84, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T0_GPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR30
{
    static const uint8 op_store_T0_GPR30_code[] = {
       0x44, 0x89, 0xa5, 0x88, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T0_GPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_GPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_GPR31
{
    static const uint8 op_store_T0_GPR31_code[] = {
       0x44, 0x89, 0xa5, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T0_GPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb10
{
    static const uint8 op_store_T0_crb10_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xdf, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x15, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb10_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb11
{
    static const uint8 op_store_T0_crb11_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xef, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x14, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb11_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb12
{
    static const uint8 op_store_T0_crb12_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xf7, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x13, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb12_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb13
{
    static const uint8 op_store_T0_crb13_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xfb, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x12, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb13_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb14
{
    static const uint8 op_store_T0_crb14_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xfd, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x11, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb14_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb15
{
    static const uint8 op_store_T0_crb15_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xfe, 0xff,
       0x44, 0x89, 0xe0, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x10, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb15_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb16
{
    static const uint8 op_store_T0_crb16_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0x7f, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0f, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb16_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb17
{
    static const uint8 op_store_T0_crb17_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xbf, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0e, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb17_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb18
{
    static const uint8 op_store_T0_crb18_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xdf, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0d, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb18_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb19
{
    static const uint8 op_store_T0_crb19_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xef, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0c, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb19_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb20
{
    static const uint8 op_store_T0_crb20_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xf7, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0b, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb20_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb21
{
    static const uint8 op_store_T0_crb21_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xfb, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0a, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb21_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb22
{
    static const uint8 op_store_T0_crb22_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xfd, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x09, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb22_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb23
{
    static const uint8 op_store_T0_crb23_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xfe, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x08, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb23_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb24
{
    static const uint8 op_store_T0_crb24_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe2, 0x7f, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x07, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb24_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb25
{
    static const uint8 op_store_T0_crb25_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xbf, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x06, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb25_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb26
{
    static const uint8 op_store_T0_crb26_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xdf, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x05, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb26_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb27
{
    static const uint8 op_store_T0_crb27_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xef, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x04, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb27_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb28
{
    static const uint8 op_store_T0_crb28_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xf7, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x03, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb28_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb29
{
    static const uint8 op_store_T0_crb29_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xfb, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x02, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T0_crb29_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb30
{
    static const uint8 op_store_T0_crb30_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xfd, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0x01, 0xc0, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03, 0x00,
       0x00
    };
    copy_block(op_store_T0_crb30_code, 25);
    inc_code_ptr(25);
}
#endif

DEFINE_GEN(gen_op_store_T0_crb31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_crb31
{
    static const uint8 op_store_T0_crb31_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xfe, 0x44, 0x89, 0xe0,
       0x83, 0xe0, 0x01, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_crb31_code, 23);
    inc_code_ptr(23);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR10
{
    static const uint8 op_store_T1_GPR10_code[] = {
       0x44, 0x89, 0x6d, 0x38
    };
    copy_block(op_store_T1_GPR10_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR11
{
    static const uint8 op_store_T1_GPR11_code[] = {
       0x44, 0x89, 0x6d, 0x3c
    };
    copy_block(op_store_T1_GPR11_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR12
{
    static const uint8 op_store_T1_GPR12_code[] = {
       0x44, 0x89, 0x6d, 0x40
    };
    copy_block(op_store_T1_GPR12_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR13
{
    static const uint8 op_store_T1_GPR13_code[] = {
       0x44, 0x89, 0x6d, 0x44
    };
    copy_block(op_store_T1_GPR13_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR14
{
    static const uint8 op_store_T1_GPR14_code[] = {
       0x44, 0x89, 0x6d, 0x48
    };
    copy_block(op_store_T1_GPR14_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR15
{
    static const uint8 op_store_T1_GPR15_code[] = {
       0x44, 0x89, 0x6d, 0x4c
    };
    copy_block(op_store_T1_GPR15_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR16
{
    static const uint8 op_store_T1_GPR16_code[] = {
       0x44, 0x89, 0x6d, 0x50
    };
    copy_block(op_store_T1_GPR16_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR17
{
    static const uint8 op_store_T1_GPR17_code[] = {
       0x44, 0x89, 0x6d, 0x54
    };
    copy_block(op_store_T1_GPR17_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR18
{
    static const uint8 op_store_T1_GPR18_code[] = {
       0x44, 0x89, 0x6d, 0x58
    };
    copy_block(op_store_T1_GPR18_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR19
{
    static const uint8 op_store_T1_GPR19_code[] = {
       0x44, 0x89, 0x6d, 0x5c
    };
    copy_block(op_store_T1_GPR19_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR20
{
    static const uint8 op_store_T1_GPR20_code[] = {
       0x44, 0x89, 0x6d, 0x60
    };
    copy_block(op_store_T1_GPR20_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR21
{
    static const uint8 op_store_T1_GPR21_code[] = {
       0x44, 0x89, 0x6d, 0x64
    };
    copy_block(op_store_T1_GPR21_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR22
{
    static const uint8 op_store_T1_GPR22_code[] = {
       0x44, 0x89, 0x6d, 0x68
    };
    copy_block(op_store_T1_GPR22_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR23
{
    static const uint8 op_store_T1_GPR23_code[] = {
       0x44, 0x89, 0x6d, 0x6c
    };
    copy_block(op_store_T1_GPR23_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR24
{
    static const uint8 op_store_T1_GPR24_code[] = {
       0x44, 0x89, 0x6d, 0x70
    };
    copy_block(op_store_T1_GPR24_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR25
{
    static const uint8 op_store_T1_GPR25_code[] = {
       0x44, 0x89, 0x6d, 0x74
    };
    copy_block(op_store_T1_GPR25_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR26
{
    static const uint8 op_store_T1_GPR26_code[] = {
       0x44, 0x89, 0x6d, 0x78
    };
    copy_block(op_store_T1_GPR26_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR27
{
    static const uint8 op_store_T1_GPR27_code[] = {
       0x44, 0x89, 0x6d, 0x7c
    };
    copy_block(op_store_T1_GPR27_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR28
{
    static const uint8 op_store_T1_GPR28_code[] = {
       0x44, 0x89, 0xad, 0x80, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T1_GPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR29
{
    static const uint8 op_store_T1_GPR29_code[] = {
       0x44, 0x89, 0xad, 0x84, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T1_GPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR30
{
    static const uint8 op_store_T1_GPR30_code[] = {
       0x44, 0x89, 0xad, 0x88, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T1_GPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T1_GPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_GPR31
{
    static const uint8 op_store_T1_GPR31_code[] = {
       0x44, 0x89, 0xad, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T1_GPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb10
{
    static const uint8 op_store_T1_crb10_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xdf, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x15, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb10_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb11
{
    static const uint8 op_store_T1_crb11_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xef, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x14, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb11_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb12
{
    static const uint8 op_store_T1_crb12_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xf7, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x13, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb12_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb13
{
    static const uint8 op_store_T1_crb13_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xfb, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x12, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb13_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb14
{
    static const uint8 op_store_T1_crb14_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xfd, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x11, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb14_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb15
{
    static const uint8 op_store_T1_crb15_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x81, 0xe2, 0xff, 0xff, 0xfe, 0xff,
       0x44, 0x89, 0xe8, 0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x10, 0x09, 0xd0, 0x89,
       0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb15_code, 29);
    inc_code_ptr(29);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb16
{
    static const uint8 op_store_T1_crb16_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0x7f, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0f, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb16_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb17
{
    static const uint8 op_store_T1_crb17_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xbf, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0e, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb17_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb18
{
    static const uint8 op_store_T1_crb18_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xdf, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0d, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb18_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb19
{
    static const uint8 op_store_T1_crb19_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xef, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0c, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb19_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb20
{
    static const uint8 op_store_T1_crb20_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xf7, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0b, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb20_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb21
{
    static const uint8 op_store_T1_crb21_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xfb, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x0a, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb21_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb22
{
    static const uint8 op_store_T1_crb22_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xfd, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x09, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb22_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb23
{
    static const uint8 op_store_T1_crb23_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe6, 0xfe, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x08, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb23_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb24
{
    static const uint8 op_store_T1_crb24_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x80, 0xe2, 0x7f, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x07, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb24_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb25
{
    static const uint8 op_store_T1_crb25_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xbf, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x06, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb25_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb26
{
    static const uint8 op_store_T1_crb26_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xdf, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x05, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb26_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb27
{
    static const uint8 op_store_T1_crb27_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xef, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x04, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb27_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb28
{
    static const uint8 op_store_T1_crb28_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xf7, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x03, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb28_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb29
{
    static const uint8 op_store_T1_crb29_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xfb, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0xc1, 0xe0, 0x02, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03,
       0x00, 0x00
    };
    copy_block(op_store_T1_crb29_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb30
{
    static const uint8 op_store_T1_crb30_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xfd, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0x01, 0xc0, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03, 0x00,
       0x00
    };
    copy_block(op_store_T1_crb30_code, 25);
    inc_code_ptr(25);
}
#endif

DEFINE_GEN(gen_op_store_T1_crb31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T1_crb31
{
    static const uint8 op_store_T1_crb31_code[] = {
       0x8b, 0x95, 0x90, 0x03, 0x00, 0x00, 0x83, 0xe2, 0xfe, 0x44, 0x89, 0xe8,
       0x83, 0xe0, 0x01, 0x09, 0xd0, 0x89, 0x85, 0x90, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T1_crb31_code, 23);
    inc_code_ptr(23);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR10
{
    static const uint8 op_store_T2_GPR10_code[] = {
       0x44, 0x89, 0x75, 0x38
    };
    copy_block(op_store_T2_GPR10_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR11
{
    static const uint8 op_store_T2_GPR11_code[] = {
       0x44, 0x89, 0x75, 0x3c
    };
    copy_block(op_store_T2_GPR11_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR12
{
    static const uint8 op_store_T2_GPR12_code[] = {
       0x44, 0x89, 0x75, 0x40
    };
    copy_block(op_store_T2_GPR12_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR13
{
    static const uint8 op_store_T2_GPR13_code[] = {
       0x44, 0x89, 0x75, 0x44
    };
    copy_block(op_store_T2_GPR13_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR14
{
    static const uint8 op_store_T2_GPR14_code[] = {
       0x44, 0x89, 0x75, 0x48
    };
    copy_block(op_store_T2_GPR14_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR15
{
    static const uint8 op_store_T2_GPR15_code[] = {
       0x44, 0x89, 0x75, 0x4c
    };
    copy_block(op_store_T2_GPR15_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR16
{
    static const uint8 op_store_T2_GPR16_code[] = {
       0x44, 0x89, 0x75, 0x50
    };
    copy_block(op_store_T2_GPR16_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR17
{
    static const uint8 op_store_T2_GPR17_code[] = {
       0x44, 0x89, 0x75, 0x54
    };
    copy_block(op_store_T2_GPR17_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR18
{
    static const uint8 op_store_T2_GPR18_code[] = {
       0x44, 0x89, 0x75, 0x58
    };
    copy_block(op_store_T2_GPR18_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR19
{
    static const uint8 op_store_T2_GPR19_code[] = {
       0x44, 0x89, 0x75, 0x5c
    };
    copy_block(op_store_T2_GPR19_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR20
{
    static const uint8 op_store_T2_GPR20_code[] = {
       0x44, 0x89, 0x75, 0x60
    };
    copy_block(op_store_T2_GPR20_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR21
{
    static const uint8 op_store_T2_GPR21_code[] = {
       0x44, 0x89, 0x75, 0x64
    };
    copy_block(op_store_T2_GPR21_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR22
{
    static const uint8 op_store_T2_GPR22_code[] = {
       0x44, 0x89, 0x75, 0x68
    };
    copy_block(op_store_T2_GPR22_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR23
{
    static const uint8 op_store_T2_GPR23_code[] = {
       0x44, 0x89, 0x75, 0x6c
    };
    copy_block(op_store_T2_GPR23_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR24
{
    static const uint8 op_store_T2_GPR24_code[] = {
       0x44, 0x89, 0x75, 0x70
    };
    copy_block(op_store_T2_GPR24_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR25
{
    static const uint8 op_store_T2_GPR25_code[] = {
       0x44, 0x89, 0x75, 0x74
    };
    copy_block(op_store_T2_GPR25_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR26
{
    static const uint8 op_store_T2_GPR26_code[] = {
       0x44, 0x89, 0x75, 0x78
    };
    copy_block(op_store_T2_GPR26_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR27
{
    static const uint8 op_store_T2_GPR27_code[] = {
       0x44, 0x89, 0x75, 0x7c
    };
    copy_block(op_store_T2_GPR27_code, 4);
    inc_code_ptr(4);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR28
{
    static const uint8 op_store_T2_GPR28_code[] = {
       0x44, 0x89, 0xb5, 0x80, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T2_GPR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR29
{
    static const uint8 op_store_T2_GPR29_code[] = {
       0x44, 0x89, 0xb5, 0x84, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T2_GPR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR30
{
    static const uint8 op_store_T2_GPR30_code[] = {
       0x44, 0x89, 0xb5, 0x88, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T2_GPR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_store_T2_GPR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T2_GPR31
{
    static const uint8 op_store_T2_GPR31_code[] = {
       0x44, 0x89, 0xb5, 0x8c, 0x00, 0x00, 0x00
    };
    copy_block(op_store_T2_GPR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_vandc_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vandc_VD_V0_V1
{
    static const uint8 op_vandc_VD_V0_V1_code[] = {
       0x49, 0x8b, 0x45, 0x00, 0x48, 0xf7, 0xd0, 0x49, 0x23, 0x04, 0x24, 0x49,
       0x89, 0x07, 0x49, 0x8b, 0x45, 0x08, 0x48, 0xf7, 0xd0, 0x49, 0x23, 0x44,
       0x24, 0x08, 0x49, 0x89, 0x47, 0x08
    };
    copy_block(op_vandc_VD_V0_V1_code, 30);
    inc_code_ptr(30);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR10
{
    static const uint8 op_load_ad_V0_VR10_code[] = {
       0x4c, 0x8d, 0xa5, 0x30, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR11
{
    static const uint8 op_load_ad_V0_VR11_code[] = {
       0x4c, 0x8d, 0xa5, 0x40, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR12
{
    static const uint8 op_load_ad_V0_VR12_code[] = {
       0x4c, 0x8d, 0xa5, 0x50, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR13
{
    static const uint8 op_load_ad_V0_VR13_code[] = {
       0x4c, 0x8d, 0xa5, 0x60, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR14
{
    static const uint8 op_load_ad_V0_VR14_code[] = {
       0x4c, 0x8d, 0xa5, 0x70, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR15
{
    static const uint8 op_load_ad_V0_VR15_code[] = {
       0x4c, 0x8d, 0xa5, 0x80, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR16
{
    static const uint8 op_load_ad_V0_VR16_code[] = {
       0x4c, 0x8d, 0xa5, 0x90, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR17
{
    static const uint8 op_load_ad_V0_VR17_code[] = {
       0x4c, 0x8d, 0xa5, 0xa0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR18
{
    static const uint8 op_load_ad_V0_VR18_code[] = {
       0x4c, 0x8d, 0xa5, 0xb0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR19
{
    static const uint8 op_load_ad_V0_VR19_code[] = {
       0x4c, 0x8d, 0xa5, 0xc0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR20
{
    static const uint8 op_load_ad_V0_VR20_code[] = {
       0x4c, 0x8d, 0xa5, 0xd0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR21
{
    static const uint8 op_load_ad_V0_VR21_code[] = {
       0x4c, 0x8d, 0xa5, 0xe0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR22
{
    static const uint8 op_load_ad_V0_VR22_code[] = {
       0x4c, 0x8d, 0xa5, 0xf0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR23
{
    static const uint8 op_load_ad_V0_VR23_code[] = {
       0x4c, 0x8d, 0xa5, 0x00, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR24
{
    static const uint8 op_load_ad_V0_VR24_code[] = {
       0x4c, 0x8d, 0xa5, 0x10, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR25
{
    static const uint8 op_load_ad_V0_VR25_code[] = {
       0x4c, 0x8d, 0xa5, 0x20, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR26
{
    static const uint8 op_load_ad_V0_VR26_code[] = {
       0x4c, 0x8d, 0xa5, 0x30, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR27
{
    static const uint8 op_load_ad_V0_VR27_code[] = {
       0x4c, 0x8d, 0xa5, 0x40, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR28
{
    static const uint8 op_load_ad_V0_VR28_code[] = {
       0x4c, 0x8d, 0xa5, 0x50, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR29
{
    static const uint8 op_load_ad_V0_VR29_code[] = {
       0x4c, 0x8d, 0xa5, 0x60, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR30
{
    static const uint8 op_load_ad_V0_VR30_code[] = {
       0x4c, 0x8d, 0xa5, 0x70, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V0_VR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V0_VR31
{
    static const uint8 op_load_ad_V0_VR31_code[] = {
       0x4c, 0x8d, 0xa5, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V0_VR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR10
{
    static const uint8 op_load_ad_V1_VR10_code[] = {
       0x4c, 0x8d, 0xad, 0x30, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR11
{
    static const uint8 op_load_ad_V1_VR11_code[] = {
       0x4c, 0x8d, 0xad, 0x40, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR12
{
    static const uint8 op_load_ad_V1_VR12_code[] = {
       0x4c, 0x8d, 0xad, 0x50, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR13
{
    static const uint8 op_load_ad_V1_VR13_code[] = {
       0x4c, 0x8d, 0xad, 0x60, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR14
{
    static const uint8 op_load_ad_V1_VR14_code[] = {
       0x4c, 0x8d, 0xad, 0x70, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR15
{
    static const uint8 op_load_ad_V1_VR15_code[] = {
       0x4c, 0x8d, 0xad, 0x80, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR16
{
    static const uint8 op_load_ad_V1_VR16_code[] = {
       0x4c, 0x8d, 0xad, 0x90, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR17
{
    static const uint8 op_load_ad_V1_VR17_code[] = {
       0x4c, 0x8d, 0xad, 0xa0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR18
{
    static const uint8 op_load_ad_V1_VR18_code[] = {
       0x4c, 0x8d, 0xad, 0xb0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR19
{
    static const uint8 op_load_ad_V1_VR19_code[] = {
       0x4c, 0x8d, 0xad, 0xc0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR20
{
    static const uint8 op_load_ad_V1_VR20_code[] = {
       0x4c, 0x8d, 0xad, 0xd0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR21
{
    static const uint8 op_load_ad_V1_VR21_code[] = {
       0x4c, 0x8d, 0xad, 0xe0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR22
{
    static const uint8 op_load_ad_V1_VR22_code[] = {
       0x4c, 0x8d, 0xad, 0xf0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR23
{
    static const uint8 op_load_ad_V1_VR23_code[] = {
       0x4c, 0x8d, 0xad, 0x00, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR24
{
    static const uint8 op_load_ad_V1_VR24_code[] = {
       0x4c, 0x8d, 0xad, 0x10, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR25
{
    static const uint8 op_load_ad_V1_VR25_code[] = {
       0x4c, 0x8d, 0xad, 0x20, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR26
{
    static const uint8 op_load_ad_V1_VR26_code[] = {
       0x4c, 0x8d, 0xad, 0x30, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR27
{
    static const uint8 op_load_ad_V1_VR27_code[] = {
       0x4c, 0x8d, 0xad, 0x40, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR28
{
    static const uint8 op_load_ad_V1_VR28_code[] = {
       0x4c, 0x8d, 0xad, 0x50, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR29
{
    static const uint8 op_load_ad_V1_VR29_code[] = {
       0x4c, 0x8d, 0xad, 0x60, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR30
{
    static const uint8 op_load_ad_V1_VR30_code[] = {
       0x4c, 0x8d, 0xad, 0x70, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V1_VR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V1_VR31
{
    static const uint8 op_load_ad_V1_VR31_code[] = {
       0x4c, 0x8d, 0xad, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V1_VR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR10
{
    static const uint8 op_load_ad_V2_VR10_code[] = {
       0x4c, 0x8d, 0xb5, 0x30, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR11
{
    static const uint8 op_load_ad_V2_VR11_code[] = {
       0x4c, 0x8d, 0xb5, 0x40, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR12
{
    static const uint8 op_load_ad_V2_VR12_code[] = {
       0x4c, 0x8d, 0xb5, 0x50, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR13
{
    static const uint8 op_load_ad_V2_VR13_code[] = {
       0x4c, 0x8d, 0xb5, 0x60, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR14
{
    static const uint8 op_load_ad_V2_VR14_code[] = {
       0x4c, 0x8d, 0xb5, 0x70, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR15
{
    static const uint8 op_load_ad_V2_VR15_code[] = {
       0x4c, 0x8d, 0xb5, 0x80, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR16
{
    static const uint8 op_load_ad_V2_VR16_code[] = {
       0x4c, 0x8d, 0xb5, 0x90, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR17
{
    static const uint8 op_load_ad_V2_VR17_code[] = {
       0x4c, 0x8d, 0xb5, 0xa0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR18
{
    static const uint8 op_load_ad_V2_VR18_code[] = {
       0x4c, 0x8d, 0xb5, 0xb0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR19
{
    static const uint8 op_load_ad_V2_VR19_code[] = {
       0x4c, 0x8d, 0xb5, 0xc0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR20
{
    static const uint8 op_load_ad_V2_VR20_code[] = {
       0x4c, 0x8d, 0xb5, 0xd0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR21
{
    static const uint8 op_load_ad_V2_VR21_code[] = {
       0x4c, 0x8d, 0xb5, 0xe0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR22
{
    static const uint8 op_load_ad_V2_VR22_code[] = {
       0x4c, 0x8d, 0xb5, 0xf0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR23
{
    static const uint8 op_load_ad_V2_VR23_code[] = {
       0x4c, 0x8d, 0xb5, 0x00, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR24
{
    static const uint8 op_load_ad_V2_VR24_code[] = {
       0x4c, 0x8d, 0xb5, 0x10, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR25
{
    static const uint8 op_load_ad_V2_VR25_code[] = {
       0x4c, 0x8d, 0xb5, 0x20, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR26
{
    static const uint8 op_load_ad_V2_VR26_code[] = {
       0x4c, 0x8d, 0xb5, 0x30, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR27
{
    static const uint8 op_load_ad_V2_VR27_code[] = {
       0x4c, 0x8d, 0xb5, 0x40, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR28
{
    static const uint8 op_load_ad_V2_VR28_code[] = {
       0x4c, 0x8d, 0xb5, 0x50, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR29
{
    static const uint8 op_load_ad_V2_VR29_code[] = {
       0x4c, 0x8d, 0xb5, 0x60, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR30
{
    static const uint8 op_load_ad_V2_VR30_code[] = {
       0x4c, 0x8d, 0xb5, 0x70, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_V2_VR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_V2_VR31
{
    static const uint8 op_load_ad_V2_VR31_code[] = {
       0x4c, 0x8d, 0xb5, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_V2_VR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR10,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR10
{
    static const uint8 op_load_ad_VD_VR10_code[] = {
       0x4c, 0x8d, 0xbd, 0x30, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR10_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR11,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR11
{
    static const uint8 op_load_ad_VD_VR11_code[] = {
       0x4c, 0x8d, 0xbd, 0x40, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR11_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR12,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR12
{
    static const uint8 op_load_ad_VD_VR12_code[] = {
       0x4c, 0x8d, 0xbd, 0x50, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR12_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR13,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR13
{
    static const uint8 op_load_ad_VD_VR13_code[] = {
       0x4c, 0x8d, 0xbd, 0x60, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR13_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR14,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR14
{
    static const uint8 op_load_ad_VD_VR14_code[] = {
       0x4c, 0x8d, 0xbd, 0x70, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR14_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR15,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR15
{
    static const uint8 op_load_ad_VD_VR15_code[] = {
       0x4c, 0x8d, 0xbd, 0x80, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR15_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR16,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR16
{
    static const uint8 op_load_ad_VD_VR16_code[] = {
       0x4c, 0x8d, 0xbd, 0x90, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR16_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR17,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR17
{
    static const uint8 op_load_ad_VD_VR17_code[] = {
       0x4c, 0x8d, 0xbd, 0xa0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR17_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR18,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR18
{
    static const uint8 op_load_ad_VD_VR18_code[] = {
       0x4c, 0x8d, 0xbd, 0xb0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR18_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR19,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR19
{
    static const uint8 op_load_ad_VD_VR19_code[] = {
       0x4c, 0x8d, 0xbd, 0xc0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR19_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR20,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR20
{
    static const uint8 op_load_ad_VD_VR20_code[] = {
       0x4c, 0x8d, 0xbd, 0xd0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR20_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR21,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR21
{
    static const uint8 op_load_ad_VD_VR21_code[] = {
       0x4c, 0x8d, 0xbd, 0xe0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR21_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR22,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR22
{
    static const uint8 op_load_ad_VD_VR22_code[] = {
       0x4c, 0x8d, 0xbd, 0xf0, 0x02, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR22_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR23,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR23
{
    static const uint8 op_load_ad_VD_VR23_code[] = {
       0x4c, 0x8d, 0xbd, 0x00, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR23_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR24,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR24
{
    static const uint8 op_load_ad_VD_VR24_code[] = {
       0x4c, 0x8d, 0xbd, 0x10, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR24_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR25,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR25
{
    static const uint8 op_load_ad_VD_VR25_code[] = {
       0x4c, 0x8d, 0xbd, 0x20, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR25_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR26,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR26
{
    static const uint8 op_load_ad_VD_VR26_code[] = {
       0x4c, 0x8d, 0xbd, 0x30, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR26_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR27,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR27
{
    static const uint8 op_load_ad_VD_VR27_code[] = {
       0x4c, 0x8d, 0xbd, 0x40, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR27_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR28,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR28
{
    static const uint8 op_load_ad_VD_VR28_code[] = {
       0x4c, 0x8d, 0xbd, 0x50, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR28_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR29,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR29
{
    static const uint8 op_load_ad_VD_VR29_code[] = {
       0x4c, 0x8d, 0xbd, 0x60, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR29_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR30,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR30
{
    static const uint8 op_load_ad_VD_VR30_code[] = {
       0x4c, 0x8d, 0xbd, 0x70, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR30_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_ad_VD_VR31,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_ad_VD_VR31
{
    static const uint8 op_load_ad_VD_VR31_code[] = {
       0x4c, 0x8d, 0xbd, 0x80, 0x03, 0x00, 0x00
    };
    copy_block(op_load_ad_VD_VR31_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_load_vect_VD_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_vect_VD_T0
{
    static const uint8 op_load_vect_VD_T0_code[] = {
       0x44, 0x89, 0xe2, 0x83, 0xe2, 0xf0, 0x89, 0xd0, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x0f, 0xc8, 0x41, 0x89, 0x07, 0x8d, 0x42, 0x04, 0x89, 0xc0, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x0f, 0xc8, 0x41, 0x89, 0x47, 0x04, 0x8d, 0x42, 0x08, 0x89, 0xc0, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x0f, 0xc8, 0x41, 0x89, 0x47, 0x08, 0x83, 0xc2, 0x0c, 0x89, 0xd2, 0x8b,
       0x02, 0x0f, 0xc8, 0x41, 0x89, 0x47, 0x0c
    };
    copy_block(op_load_vect_VD_T0_code, 162);
    *(uint32_t *)(code_ptr() + 32) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 80) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 129) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 40) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 88) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 137) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(162);
}
#endif

DEFINE_GEN(gen_op_load_word_VD_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_word_VD_T0
{
    static const uint8 op_load_word_VD_T0_code[] = {
       0x44, 0x89, 0xe2, 0x48, 0x89, 0xd0, 0x83, 0xe0, 0xfc, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x0f, 0xc8, 0xc1, 0xea, 0x02, 0x83, 0xe2, 0x03, 0x41, 0x89, 0x04, 0x97
    };
    copy_block(op_load_word_VD_T0_code, 59);
    *(uint32_t *)(code_ptr() + 33) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 41) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(59);
}
#endif

DEFINE_GEN(gen_op_store_T0_VRSAVE,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_T0_VRSAVE
{
    static const uint8 op_store_T0_VRSAVE_code[] = {
       0x44, 0x89, 0xa5, 0x9c, 0x03, 0x00, 0x00
    };
    copy_block(op_store_T0_VRSAVE_code, 7);
    inc_code_ptr(7);
}
#endif

DEFINE_GEN(gen_op_vaddfp_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vaddfp_VD_V0_V1
{
    static const uint8 op_vaddfp_VD_V0_V1_code[] = {
       0xf3, 0x41, 0x0f, 0x10, 0x45, 0x00, 0xf3, 0x41, 0x0f, 0x58, 0x04, 0x24,
       0xf3, 0x41, 0x0f, 0x11, 0x07, 0xf3, 0x41, 0x0f, 0x10, 0x45, 0x04, 0xf3,
       0x41, 0x0f, 0x58, 0x44, 0x24, 0x04, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x04,
       0xf3, 0x41, 0x0f, 0x10, 0x45, 0x08, 0xf3, 0x41, 0x0f, 0x58, 0x44, 0x24,
       0x08, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x08, 0xf3, 0x41, 0x0f, 0x10, 0x45,
       0x0c, 0xf3, 0x41, 0x0f, 0x58, 0x44, 0x24, 0x0c, 0xf3, 0x41, 0x0f, 0x11,
       0x47, 0x0c
    };
    copy_block(op_vaddfp_VD_V0_V1_code, 74);
    inc_code_ptr(74);
}
#endif

DEFINE_GEN(gen_op_vsubfp_VD_V0_V1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vsubfp_VD_V0_V1
{
    static const uint8 op_vsubfp_VD_V0_V1_code[] = {
       0xf3, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf3, 0x41, 0x0f, 0x5c, 0x45, 0x00,
       0xf3, 0x41, 0x0f, 0x11, 0x07, 0xf3, 0x41, 0x0f, 0x10, 0x44, 0x24, 0x04,
       0xf3, 0x41, 0x0f, 0x5c, 0x45, 0x04, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x04,
       0xf3, 0x41, 0x0f, 0x10, 0x44, 0x24, 0x08, 0xf3, 0x41, 0x0f, 0x5c, 0x45,
       0x08, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x08, 0xf3, 0x41, 0x0f, 0x10, 0x44,
       0x24, 0x0c, 0xf3, 0x41, 0x0f, 0x5c, 0x45, 0x0c, 0xf3, 0x41, 0x0f, 0x11,
       0x47, 0x0c
    };
    copy_block(op_vsubfp_VD_V0_V1_code, 74);
    inc_code_ptr(74);
}
#endif

DEFINE_GEN(gen_op_store_vect_VD_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_vect_VD_T0
{
    static const uint8 op_store_vect_VD_T0_code[] = {
       0x44, 0x89, 0xe1, 0x83, 0xe1, 0xf0, 0x41, 0x8b, 0x07, 0x0f, 0xc8, 0x89,
       0xca, 
       TRANS_RDX,
       0x89, 0x02, 
       0x41, 0x8b, 0x57, 0x04, 0x0f, 0xca, 0x8d, 0x41, 0x04, 0x89, 0xc0, 
       TRANS_RAX,
       0x89, 0x10, 
       0x41, 0x8b, 0x57, 0x08, 0x0f, 0xca, 0x8d, 0x41, 0x08, 0x89, 0xc0, 
       TRANS_RAX,
       0x89, 0x10, 
       0x41, 0x8b, 0x47, 0x0c, 0x0f, 0xc8, 0x83, 0xc1, 0x0c, 0x89, 0xc9, 0x89,
       0x01
    };
    copy_block(op_store_vect_VD_T0_code, 167);
    *(uint32_t *)(code_ptr() + 41) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 91) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 140) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 50) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 99) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 148) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(167);
}
#endif

DEFINE_GEN(gen_op_store_word_VD_T0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_word_VD_T0
{
    static const uint8 op_store_word_VD_T0_code[] = {
       0x44, 0x89, 0xe0, 0x44, 0x89, 0xe2, 0xc1, 0xea, 0x02, 0x83, 0xe2, 0x03,
       0x41, 0x8b, 0x14, 0x97, 0x0f, 0xca, 0x83, 0xe0, 0xfc, 
       TRANS_RAX,
       0x89, 0x10, 
    };
    copy_block(op_store_word_VD_T0_code, 59);
    *(uint32_t *)(code_ptr() + 45) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 53) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(59);
}
#endif

DEFINE_GEN(gen_op_fmadd_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmadd_FD_F0_F1_F2
{
    static const uint8 op_fmadd_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x58, 0x06, 0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10,
       0x00
    };
    copy_block(op_fmadd_FD_F0_F1_F2_code, 25);
    inc_code_ptr(25);
}
#endif

DEFINE_GEN(gen_op_fmsub_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmsub_FD_F0_F1_F2
{
    static const uint8 op_fmsub_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x5c, 0x06, 0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10,
       0x00
    };
    copy_block(op_fmsub_FD_F0_F1_F2_code, 25);
    inc_code_ptr(25);
}
#endif

DEFINE_GEN(gen_op_fmadds_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmadds_FD_F0_F1_F2
{
    static const uint8 op_fmadds_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x58, 0x06, 0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x5a,
       0xc0, 0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmadds_FD_F0_F1_F2_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_fmsubs_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fmsubs_FD_F0_F1_F2
{
    static const uint8 op_fmsubs_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x5c, 0x06, 0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x5a,
       0xc0, 0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fmsubs_FD_F0_F1_F2_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_fnmadd_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fnmadd_FD_F0_F1_F2
{
    static const uint8 op_fnmadd_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x58, 0x06, 0x66, 0x0f, 0x57, 0x05, 0x0b, 0x1b, 0x00,
       0x00, 0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fnmadd_FD_F0_F1_F2_code, 33);
    static const uint8 literal16_1[] = {
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00
    };
    static uint8 *data_p_1 = NULL;
    if (data_p_1 == NULL)
        data_p_1 = copy_data(literal16_1, 16);
    *(uint32_t *)(code_ptr() + 21) = (int32_t)((long)data_p_1 - (long)(code_ptr() + 21 + 4));
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_fnmsub_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fnmsub_FD_F0_F1_F2
{
    static const uint8 op_fnmsub_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x5c, 0x06, 0x66, 0x0f, 0x57, 0x05, 0xe9, 0x1a, 0x00,
       0x00, 0xf2, 0x0f, 0x11, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fnmsub_FD_F0_F1_F2_code, 33);
    static const uint8 literal16_1[] = {
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00
    };
    static uint8 *data_p_1 = NULL;
    if (data_p_1 == NULL)
        data_p_1 = copy_data(literal16_1, 16);
    *(uint32_t *)(code_ptr() + 21) = (int32_t)((long)data_p_1 - (long)(code_ptr() + 21 + 4));
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_load_T0_LR_aligned,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_LR_aligned
{
    static const uint8 op_load_T0_LR_aligned_code[] = {
       0x44, 0x8b, 0xa5, 0xa4, 0x03, 0x00, 0x00, 0x41, 0x83, 0xe4, 0xfc
    };
    copy_block(op_load_T0_LR_aligned_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_fnmadds_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fnmadds_FD_F0_F1_F2
{
    static const uint8 op_fnmadds_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x58, 0x06, 0xf2, 0x0f, 0x5a, 0xc0, 0x0f, 0x57, 0x05,
       0xfc, 0x19, 0x00, 0x00, 0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x85,
       0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fnmadds_FD_F0_F1_F2_code, 40);
    static const uint8 literal16_1[] = {
       0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00
    };
    static uint8 *data_p_1 = NULL;
    if (data_p_1 == NULL)
        data_p_1 = copy_data(literal16_1, 16);
    *(uint32_t *)(code_ptr() + 24) = (int32_t)((long)data_p_1 - (long)(code_ptr() + 24 + 4));
    inc_code_ptr(40);
}
#endif

DEFINE_GEN(gen_op_fnmsubs_FD_F0_F1_F2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_fnmsubs_FD_F0_F1_F2
{
    static const uint8 op_fnmsubs_FD_F0_F1_F2_code[] = {
       0xf2, 0x41, 0x0f, 0x10, 0x04, 0x24, 0xf2, 0x41, 0x0f, 0x59, 0x45, 0x00,
       0xf2, 0x41, 0x0f, 0x5c, 0x06, 0xf2, 0x0f, 0x5a, 0xc0, 0x0f, 0x57, 0x05,
       0x8a, 0x34, 0x00, 0x00, 0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x85,
       0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_fnmsubs_FD_F0_F1_F2_code, 40);
    static const uint8 literal16_1[] = {
       0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00
    };
    static uint8 *data_p_1 = NULL;
    if (data_p_1 == NULL)
        data_p_1 = copy_data(literal16_1, 16);
    *(uint32_t *)(code_ptr() + 24) = (int32_t)((long)data_p_1 - (long)(code_ptr() + 24 + 4));
    inc_code_ptr(40);
}
#endif

DEFINE_GEN(gen_op_load_T0_CTR_aligned,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_T0_CTR_aligned
{
    static const uint8 op_load_T0_CTR_aligned_code[] = {
       0x44, 0x8b, 0xa5, 0xa8, 0x03, 0x00, 0x00, 0x41, 0x83, 0xe4, 0xfc
    };
    copy_block(op_load_T0_CTR_aligned_code, 11);
    inc_code_ptr(11);
}
#endif

DEFINE_GEN(gen_op_load_double_FD_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_double_FD_T1_0
{
    static const uint8 op_load_double_FD_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x48, 0x8b, 0x00, 
       0x48, 0x0f, 0xc8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_load_double_FD_T1_0_code, 52);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(52);
}
#endif

DEFINE_GEN(gen_op_load_single_FD_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_single_FD_T1_0
{
    static const uint8 op_load_single_FD_T1_0_code[] = {
       0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x0f, 0xc8, 0x89, 0x44, 0x24, 0xf4, 0xf3, 0x0f, 0x10, 0x44, 0x24, 0xf4,
       0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x44, 0x24, 0xf8, 0x48, 0x8b,
       0x44, 0x24, 0xf8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_load_single_FD_T1_0_code, 75);
    *(uint32_t *)(code_ptr() + 27) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 35) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(75);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_0000,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_0000
{
    static const uint8 op_prep_branch_bo_0000_code[] = {
       0x8b, 0x85, 0xa8, 0x03, 0x00, 0x00, 0xff, 0xc8, 0x41, 0x89, 0xc6, 0x89,
       0x85, 0xa8, 0x03, 0x00, 0x00, 0x31, 0xd2, 0x85, 0xc0, 0x74, 0x06, 0x45,
       0x85, 0xed, 0x0f, 0x94, 0xc2, 0x44, 0x0f, 0xb6, 0xea
    };
    copy_block(op_prep_branch_bo_0000_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_0001,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_0001
{
    static const uint8 op_prep_branch_bo_0001_code[] = {
       0x8b, 0x85, 0xa8, 0x03, 0x00, 0x00, 0xff, 0xc8, 0x41, 0x89, 0xc6, 0x89,
       0x85, 0xa8, 0x03, 0x00, 0x00, 0x31, 0xd2, 0x85, 0xc0, 0x75, 0x06, 0x45,
       0x85, 0xed, 0x0f, 0x94, 0xc2, 0x44, 0x0f, 0xb6, 0xea
    };
    copy_block(op_prep_branch_bo_0001_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_001x,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_001x
{
    static const uint8 op_prep_branch_bo_001x_code[] = {
       0x45, 0x85, 0xed, 0x0f, 0x94, 0xc0, 0x44, 0x0f, 0xb6, 0xe8
    };
    copy_block(op_prep_branch_bo_001x_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_0100,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_0100
{
    static const uint8 op_prep_branch_bo_0100_code[] = {
       0x8b, 0x85, 0xa8, 0x03, 0x00, 0x00, 0xff, 0xc8, 0x41, 0x89, 0xc6, 0x89,
       0x85, 0xa8, 0x03, 0x00, 0x00, 0x31, 0xd2, 0x85, 0xc0, 0x74, 0x06, 0x45,
       0x85, 0xed, 0x0f, 0x95, 0xc2, 0x44, 0x0f, 0xb6, 0xea
    };
    copy_block(op_prep_branch_bo_0100_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_0101,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_0101
{
    static const uint8 op_prep_branch_bo_0101_code[] = {
       0x8b, 0x85, 0xa8, 0x03, 0x00, 0x00, 0xff, 0xc8, 0x41, 0x89, 0xc6, 0x89,
       0x85, 0xa8, 0x03, 0x00, 0x00, 0x31, 0xd2, 0x85, 0xc0, 0x75, 0x06, 0x45,
       0x85, 0xed, 0x0f, 0x95, 0xc2, 0x44, 0x0f, 0xb6, 0xea
    };
    copy_block(op_prep_branch_bo_0101_code, 33);
    inc_code_ptr(33);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_011x,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_011x
{
    static const uint8 op_prep_branch_bo_011x_code[] = {
       0x45, 0x85, 0xed, 0x0f, 0x95, 0xc0, 0x44, 0x0f, 0xb6, 0xe8
    };
    copy_block(op_prep_branch_bo_011x_code, 10);
    inc_code_ptr(10);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_1x00,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_1x00
{
    static const uint8 op_prep_branch_bo_1x00_code[] = {
       0x8b, 0x95, 0xa8, 0x03, 0x00, 0x00, 0xff, 0xca, 0x41, 0x89, 0xd6, 0x89,
       0x95, 0xa8, 0x03, 0x00, 0x00, 0x45, 0x31, 0xed, 0x85, 0xd2, 0x41, 0x0f,
       0x95, 0xc5
    };
    copy_block(op_prep_branch_bo_1x00_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_1x01,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_1x01
{
    static const uint8 op_prep_branch_bo_1x01_code[] = {
       0x8b, 0x95, 0xa8, 0x03, 0x00, 0x00, 0xff, 0xca, 0x41, 0x89, 0xd6, 0x89,
       0x95, 0xa8, 0x03, 0x00, 0x00, 0x45, 0x31, 0xed, 0x85, 0xd2, 0x41, 0x0f,
       0x94, 0xc5
    };
    copy_block(op_prep_branch_bo_1x01_code, 26);
    inc_code_ptr(26);
}
#endif

DEFINE_GEN(gen_op_prep_branch_bo_1x1x,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_prep_branch_bo_1x1x
{
    static const uint8 op_prep_branch_bo_1x1x_code[] = {
       0x41, 0xbd, 0x01, 0x00, 0x00, 0x00
    };
    copy_block(op_prep_branch_bo_1x1x_code, 6);
    inc_code_ptr(6);
}
#endif

DEFINE_GEN(gen_op_vmaddfp_VD_V0_V1_V2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vmaddfp_VD_V0_V1_V2
{
    static const uint8 op_vmaddfp_VD_V0_V1_V2_code[] = {
       0xf3, 0x41, 0x0f, 0x10, 0x06, 0xf3, 0x41, 0x0f, 0x59, 0x04, 0x24, 0xf3,
       0x41, 0x0f, 0x58, 0x45, 0x00, 0xf3, 0x41, 0x0f, 0x11, 0x07, 0xf3, 0x41,
       0x0f, 0x10, 0x46, 0x04, 0xf3, 0x41, 0x0f, 0x59, 0x44, 0x24, 0x04, 0xf3,
       0x41, 0x0f, 0x58, 0x45, 0x04, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x04, 0xf3,
       0x41, 0x0f, 0x10, 0x46, 0x08, 0xf3, 0x41, 0x0f, 0x59, 0x44, 0x24, 0x08,
       0xf3, 0x41, 0x0f, 0x58, 0x45, 0x08, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x08,
       0xf3, 0x41, 0x0f, 0x10, 0x46, 0x0c, 0xf3, 0x41, 0x0f, 0x59, 0x44, 0x24,
       0x0c, 0xf3, 0x41, 0x0f, 0x58, 0x45, 0x0c, 0xf3, 0x41, 0x0f, 0x11, 0x47,
       0x0c
    };
    copy_block(op_vmaddfp_VD_V0_V1_V2_code, 97);
    inc_code_ptr(97);
}
#endif

DEFINE_GEN(gen_op_compare_logical_T0_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_compare_logical_T0_0
{
    static const uint8 op_compare_logical_T0_0_code[] = {
       0x0f, 0xb6, 0x85, 0x94, 0x03, 0x00, 0x00, 0x89, 0xc2, 0x83, 0xca, 0x04,
       0x83, 0xc8, 0x02, 0x45, 0x85, 0xe4, 0x41, 0x89, 0xd4, 0x44, 0x0f, 0x44,
       0xe0
    };
    copy_block(op_compare_logical_T0_0_code, 25);
    inc_code_ptr(25);
}
#endif

DEFINE_GEN(gen_op_load_double_FD_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_double_FD_T1_T2
{
    static const uint8 op_load_double_FD_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x48, 0x8b, 0x00, 
       0x48, 0x0f, 0xc8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_load_double_FD_T1_T2_code, 53);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(53);
}
#endif

DEFINE_GEN(gen_op_load_double_FD_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_double_FD_T1_im
{
    static const uint8 op_load_double_FD_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x48, 0x8b, 0x00,
       0x48, 0x0f, 0xc8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_load_double_FD_T1_im_code, 61);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(61);
}
#endif

DEFINE_GEN(gen_op_load_single_FD_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_single_FD_T1_T2
{
    static const uint8 op_load_single_FD_T1_T2_code[] = {
       0x43, 0x8d, 0x04, 0x2e, 
       TRANS_RAX,
       0x8b, 0x00, 
       0x0f, 0xc8, 0x89, 0x44, 0x24, 0xf4, 0xf3, 0x0f, 0x10, 0x44, 0x24, 0xf4,
       0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x44, 0x24, 0xf8, 0x48, 0x8b,
       0x44, 0x24, 0xf8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_load_single_FD_T1_T2_code, 76);
    *(uint32_t *)(code_ptr() + 28) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(76);
}
#endif

DEFINE_GEN(gen_op_load_single_FD_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_load_single_FD_T1_im
{
    static const uint8 op_load_single_FD_T1_im_code[] = {
       0x44, 0x89, 0xea, 0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x8b, 0x00,
       0x0f, 0xc8, 0x89, 0x44, 0x24, 0xf4, 0xf3, 0x0f, 0x10, 0x44, 0x24, 0xf4,
       0xf3, 0x0f, 0x5a, 0xc0, 0xf2, 0x0f, 0x11, 0x44, 0x24, 0xf8, 0x48, 0x8b,
       0x44, 0x24, 0xf8, 0x48, 0x89, 0x85, 0xa8, 0x08, 0x10, 0x00
    };
    copy_block(op_load_single_FD_T1_im_code, 84);
    *(uint32_t *)(code_ptr() + 36) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 44) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 6) = (int32_t)((long)param1 - (long)(code_ptr() + 6 + 4)) + 0;
    inc_code_ptr(84);
}
#endif

DEFINE_GEN(gen_op_store_double_F0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_double_F0_T1_0
{
    static const uint8 op_store_double_F0_T1_0_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x44, 0x89, 0xea, 0x48, 0x0f, 0xc8, 
       TRANS_RDX,
       0x48, 0x89, 0x02, 
    };
    copy_block(op_store_double_F0_T1_0_code, 54);
    *(uint32_t *)(code_ptr() + 38) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 47) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(54);
}
#endif

DEFINE_GEN(gen_op_store_single_F0_T1_0,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_single_F0_T1_0
{
    static const uint8 op_store_single_F0_T1_0_code[] = {
       0x49, 0x8b, 0x0c, 0x24, 0x48, 0x89, 0xc8, 0x48, 0xc1, 0xe8, 0x34, 0x25,
       0xff, 0x07, 0x00, 0x00, 0x2d, 0x6a, 0x03, 0x00, 0x00, 0x83, 0xf8, 0x16,
       0x76, 0x1b, 0x48, 0xc1, 0xe9, 0x1d, 0x89, 0xca, 0x81, 0xe2, 0xff, 0xff,
       0xff, 0x3f, 0x48, 0xc1, 0xe9, 0x03, 0x89, 0xc8, 0x25, 0x00, 0x00, 0x00,
       0xc0, 0x09, 0xc2, 0xeb, 0x19, 0x48, 0x89, 0x4c, 0x24, 0xf0, 0xf2, 0x0f,
       0x10, 0x44, 0x24, 0xf0, 0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x11, 0x44,
       0x24, 0xfc, 0x8b, 0x54, 0x24, 0xfc, 0x0f, 0xca, 0x44, 0x89, 0xe8, 
       TRANS_RAX,
       0x89, 0x10, 
    };
    copy_block(op_store_single_F0_T1_0_code, 121);
    *(uint32_t *)(code_ptr() + 107) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 115) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(121);
}
#endif

DEFINE_GEN(gen_op_vnmsubfp_VD_V0_V1_V2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_vnmsubfp_VD_V0_V1_V2
{
    static const uint8 op_vnmsubfp_VD_V0_V1_V2_code[] = {
       0xf3, 0x41, 0x0f, 0x10, 0x06, 0xf3, 0x41, 0x0f, 0x59, 0x04, 0x24, 0xf3,
       0x41, 0x0f, 0x5c, 0x45, 0x00, 0xf3, 0x0f, 0x10, 0x0d, 0xca, 0x29, 0x00,
       0x00, 0x0f, 0x57, 0xc1, 0xf3, 0x41, 0x0f, 0x11, 0x07, 0xf3, 0x41, 0x0f,
       0x10, 0x46, 0x04, 0xf3, 0x41, 0x0f, 0x59, 0x44, 0x24, 0x04, 0xf3, 0x41,
       0x0f, 0x5c, 0x45, 0x04, 0x0f, 0x57, 0xc1, 0xf3, 0x41, 0x0f, 0x11, 0x47,
       0x04, 0xf3, 0x41, 0x0f, 0x10, 0x46, 0x08, 0xf3, 0x41, 0x0f, 0x59, 0x44,
       0x24, 0x08, 0xf3, 0x41, 0x0f, 0x5c, 0x45, 0x08, 0x0f, 0x57, 0xc1, 0xf3,
       0x41, 0x0f, 0x11, 0x47, 0x08, 0xf3, 0x41, 0x0f, 0x10, 0x46, 0x0c, 0xf3,
       0x41, 0x0f, 0x59, 0x44, 0x24, 0x0c, 0xf3, 0x41, 0x0f, 0x5c, 0x45, 0x0c,
       0x0f, 0x57, 0xc1, 0xf3, 0x41, 0x0f, 0x11, 0x47, 0x0c
    };
    copy_block(op_vnmsubfp_VD_V0_V1_V2_code, 117);
    static const uint8 literal16_1[] = {
       0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00
    };
    static uint8 *data_p_1 = NULL;
    if (data_p_1 == NULL)
        data_p_1 = copy_data(literal16_1, 16);
    *(uint32_t *)(code_ptr() + 21) = (int32_t)((long)data_p_1 - (long)(code_ptr() + 21 + 4));
    inc_code_ptr(117);
}
#endif

DEFINE_GEN(gen_op_compare_logical_T0_T1,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_compare_logical_T0_T1
{
    static const uint8 op_compare_logical_T0_T1_code[] = {
       0x0f, 0xb6, 0x95, 0x94, 0x03, 0x00, 0x00, 0x45, 0x39, 0xec, 0x73, 0x09,
       0x41, 0x89, 0xd4, 0x41, 0x83, 0xcc, 0x08, 0xeb, 0x12, 0x89, 0xd0, 0x83,
       0xc8, 0x04, 0x83, 0xca, 0x02, 0x45, 0x39, 0xec, 0x41, 0x89, 0xc4, 0x44,
       0x0f, 0x46, 0xe2
    };
    copy_block(op_compare_logical_T0_T1_code, 39);
    inc_code_ptr(39);
}
#endif

DEFINE_GEN(gen_op_compare_logical_T0_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_compare_logical_T0_im
{
    static const uint8 op_compare_logical_T0_im_code[] = {
       0x0f, 0xb6, 0x95, 0x94, 0x03, 0x00, 0x00, 0x8d, 0x35, 0x00, 0x00, 0x00,
       0x00, 0x41, 0x39, 0xf4, 0x73, 0x09, 0x41, 0x89, 0xd4, 0x41, 0x83, 0xcc,
       0x08, 0xeb, 0x12, 0x89, 0xd0, 0x83, 0xc8, 0x04, 0x83, 0xca, 0x02, 0x41,
       0x39, 0xf4, 0x41, 0x89, 0xc4, 0x44, 0x0f, 0x46, 0xe2
    };
    copy_block(op_compare_logical_T0_im_code, 45);
    *(uint32_t *)(code_ptr() + 9) = (int32_t)((long)param1 - (long)(code_ptr() + 9 + 4)) + 0;
    inc_code_ptr(45);
}
#endif

DEFINE_GEN(gen_op_store_double_F0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_double_F0_T1_T2
{
    static const uint8 op_store_double_F0_T1_T2_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x43, 0x8d, 0x14, 0x2e, 0x48, 0x0f, 0xc8, 
       TRANS_RDX,
       0x48, 0x89, 0x02, 
    };
    copy_block(op_store_double_F0_T1_T2_code, 55);
    *(uint32_t *)(code_ptr() + 39) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 48) = (uint32_t)(uintptr)gZeroPage;
    inc_code_ptr(55);
}
#endif

DEFINE_GEN(gen_op_store_double_F0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_double_F0_T1_im
{
    static const uint8 op_store_double_F0_T1_im_code[] = {
       0x49, 0x8b, 0x04, 0x24, 0x44, 0x89, 0xe9, 0x48, 0x0f, 0xc8, 0x48, 0x8d,
       0x15, 0x00, 0x00, 0x00, 0x00, 
       ADD_RDX_RCX,
       TRANS_RDX,
       0x48, 0x89, 0x02,
    };
    copy_block(op_store_double_F0_T1_im_code, 63);
    *(uint32_t *)(code_ptr() + 47) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 56) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 13) = (int32_t)((long)param1 - (long)(code_ptr() + 13 + 4)) + 0;
    inc_code_ptr(63);
}
#endif

DEFINE_GEN(gen_op_store_single_F0_T1_T2,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_single_F0_T1_T2
{
    static const uint8 op_store_single_F0_T1_T2_code[] = {
       0x49, 0x8b, 0x14, 0x24, 0x48, 0x89, 0xd0, 0x48, 0xc1, 0xe8, 0x34, 0x25,
       0xff, 0x07, 0x00, 0x00, 0x2d, 0x6a, 0x03, 0x00, 0x00, 0x83, 0xf8, 0x16,
       0x76, 0x1b, 0x48, 0xc1, 0xea, 0x1d, 0x89, 0xd1, 0x81, 0xe1, 0xff, 0xff,
       0xff, 0x3f, 0x48, 0xc1, 0xea, 0x03, 0x89, 0xd0, 0x25, 0x00, 0x00, 0x00,
       0xc0, 0x09, 0xc1, 0xeb, 0x19, 0x48, 0x89, 0x54, 0x24, 0xf0, 0xf2, 0x0f,
       0x10, 0x44, 0x24, 0xf0, 0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x11, 0x44,
       0x24, 0xfc, 0x8b, 0x4c, 0x24, 0xfc, 0x44, 0x89, 0xf0, 0x0f, 0xc9, 0x44,
       0x01, 0xe8, 0x89, 0x08
    };
    copy_block(op_store_single_F0_T1_T2_code, 88);
    inc_code_ptr(88);
}
#endif

DEFINE_GEN(gen_op_store_single_F0_T1_im,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_store_single_F0_T1_im
{
    static const uint8 op_store_single_F0_T1_im_code[] = {
       0x49, 0x8b, 0x14, 0x24, 0x48, 0x89, 0xd0, 0x48, 0xc1, 0xe8, 0x34, 0x25,
       0xff, 0x07, 0x00, 0x00, 0x2d, 0x6a, 0x03, 0x00, 0x00, 0x83, 0xf8, 0x16,
       0x76, 0x1b, 0x48, 0xc1, 0xea, 0x1d, 0x89, 0xd1, 0x81, 0xe1, 0xff, 0xff,
       0xff, 0x3f, 0x48, 0xc1, 0xea, 0x03, 0x89, 0xd0, 0x25, 0x00, 0x00, 0x00,
       0xc0, 0x09, 0xc1, 0xeb, 0x19, 0x48, 0x89, 0x54, 0x24, 0xf0, 0xf2, 0x0f,
       0x10, 0x44, 0x24, 0xf0, 0xf2, 0x0f, 0x5a, 0xc0, 0xf3, 0x0f, 0x11, 0x44,
       0x24, 0xfc, 0x8b, 0x4c, 0x24, 0xfc, 0x0f, 0xc9, 0x44, 0x89, 0xe8, 0x48,
       0x8d, 0x15, 0x00, 0x00, 0x00, 0x00, 
       ADD_RAX_RDX,
       TRANS_RAX,
       0x89, 0x08,
    };
    copy_block(op_store_single_F0_T1_im_code, 130);
    *(uint32_t *)(code_ptr() + 116) = (uint32_t)(uintptr)gKernelData;
    *(uint32_t *)(code_ptr() + 124) = (uint32_t)(uintptr)gZeroPage;
    *(uint32_t *)(code_ptr() + 86) = (int32_t)((long)param1 - (long)(code_ptr() + 86 + 4)) + 0;
    inc_code_ptr(130);
}
#endif

DEFINE_GEN(gen_op_emms,void,(void))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_emms
{
    static const uint8 op_emms_code[] = {
       0x0f, 0x77
    };
    copy_block(op_emms_code, 2);
    inc_code_ptr(2);
}
#endif

DEFINE_GEN(gen_op_inc_PC,void,(long param1))
#ifdef DYNGEN_IMPL
#define HAVE_gen_op_inc_PC
{
    static const uint8 op_inc_PC_code[] = {
       0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, 0x85, 0xac, 0x03, 0x00,
       0x00
    };
    copy_block(op_inc_PC_code, 13);
    *(uint32_t *)(code_ptr() + 3) = (int32_t)((long)param1 - (long)(code_ptr() + 3 + 4)) + 0;
    inc_code_ptr(13);
}
#endif

#undef DEFINE_CST
#undef DEFINE_GEN
