// gb-- stupid hack to insert macros in the generated assembly file
static void fpu_emit_macro_definitions()
{
#   define DEFINE_MACRO(name, value) \
    __asm__ __volatile__ (#name " = " #value)
    DEFINE_MACRO(BSUN, 0x00008000);
    DEFINE_MACRO(SNAN, 0x00004000);
    DEFINE_MACRO(OPERR, 0x00002000);
    DEFINE_MACRO(OVFL, 0x00001000);
    DEFINE_MACRO(UNFL, 0x00000800);
    DEFINE_MACRO(DZ, 0x00000400);
    DEFINE_MACRO(INEX2, 0x00000200);
    DEFINE_MACRO(INEX1, 0x00000100);
    DEFINE_MACRO(ACCR_IOP, 0x80);
    DEFINE_MACRO(ACCR_OVFL, 0x40);
    DEFINE_MACRO(ACCR_UNFL, 0x20);
    DEFINE_MACRO(ACCR_DZ, 0x10);
    DEFINE_MACRO(ACCR_INEX, 0x08);
    DEFINE_MACRO(ROUND_CONTROL_MASK, 0x30);
    DEFINE_MACRO(ROUND_TO_NEAREST, 0);
    DEFINE_MACRO(ROUND_TO_ZERO, 0x10);
    DEFINE_MACRO(ROUND_TO_NEGATIVE_INFINITY, 0x20);
    DEFINE_MACRO(ROUND_TO_POSITIVE_INFINITY, 0x30);
    DEFINE_MACRO(PRECISION_CONTROL_MASK, 0xC0);
    DEFINE_MACRO(PRECISION_CONTROL_EXTENDED, 0);
    DEFINE_MACRO(PRECISION_CONTROL_DOUBLE, 0x80);
    DEFINE_MACRO(PRECISION_CONTROL_SINGLE, 0x40);
    DEFINE_MACRO(PRECISION_CONTROL_UNDEFINED, 0xC0);
    DEFINE_MACRO(CW_RESET, 0x0040);
    DEFINE_MACRO(CW_FINIT, 0x037F);
    DEFINE_MACRO(SW_RESET, 0x0000);
    DEFINE_MACRO(SW_FINIT, 0x0000);
    DEFINE_MACRO(TW_RESET, 0x5555);
    DEFINE_MACRO(TW_FINIT, 0x0FFF);
    DEFINE_MACRO(CW_X, 0x1000);
    DEFINE_MACRO(CW_RC_ZERO, 0x0C00);
    DEFINE_MACRO(CW_RC_UP, 0x0800);
    DEFINE_MACRO(CW_RC_DOWN, 0x0400);
    DEFINE_MACRO(CW_RC_NEAR, 0x0000);
    DEFINE_MACRO(CW_PC_EXTENDED, 0x0300);
    DEFINE_MACRO(CW_PC_DOUBLE, 0x0200);
    DEFINE_MACRO(CW_PC_RESERVED, 0x0100);
    DEFINE_MACRO(CW_PC_SINGLE, 0x0000);
    DEFINE_MACRO(CW_PM, 0x0020);
    DEFINE_MACRO(CW_UM, 0x0010);
    DEFINE_MACRO(CW_OM, 0x0008);
    DEFINE_MACRO(CW_ZM, 0x0004);
    DEFINE_MACRO(CW_DM, 0x0002);
    DEFINE_MACRO(CW_IM, 0x0001);
    DEFINE_MACRO(SW_B, 0x8000);
    DEFINE_MACRO(SW_C3, 0x4000);
    DEFINE_MACRO(SW_TOP_7, 0x3800);
    DEFINE_MACRO(SW_TOP_6, 0x3000);
    DEFINE_MACRO(SW_TOP_5, 0x2800);
    DEFINE_MACRO(SW_TOP_4, 0x2000);
    DEFINE_MACRO(SW_TOP_3, 0x1800);
    DEFINE_MACRO(SW_TOP_2, 0x1000);
    DEFINE_MACRO(SW_TOP_1, 0x0800);
    DEFINE_MACRO(SW_TOP_0, 0x0000);
    DEFINE_MACRO(SW_C2, 0x0400);
    DEFINE_MACRO(SW_C1, 0x0200);
    DEFINE_MACRO(SW_C0, 0x0100);
    DEFINE_MACRO(SW_ES, 0x0080);
    DEFINE_MACRO(SW_SF, 0x0040);
    DEFINE_MACRO(SW_PE, 0x0020);
    DEFINE_MACRO(SW_UE, 0x0010);
    DEFINE_MACRO(SW_OE, 0x0008);
    DEFINE_MACRO(SW_ZE, 0x0004);
    DEFINE_MACRO(SW_DE, 0x0002);
    DEFINE_MACRO(SW_IE, 0x0001);
    DEFINE_MACRO(X86_ROUND_CONTROL_MASK, 0x0C00);
    DEFINE_MACRO(X86_PRECISION_CONTROL_MASK, 0x0300);
#   undef DEFINE_MACRO
}
/*
#!/bin/sh

awk=awk
ifile="fpu_x86.h"
ofile="fpu_x86_asm.h"

if [ ! -r "$ifile" ]; then
	echo "Error: could not open input file ($ifile)"
	exit 1
fi

touch $ofile
if [ $? -ne 0 ]; then
	echo "Error: could not open output file ($ofile)"
	exit 1
fi

# header
cat > $ofile << EOF
// gb-- stupid hack to insert macros in the generated assembly file
static void fpu_emit_macro_definitions()
{
#   define DEFINE_MACRO(name, value) \\
    __asm__ __volatile__ (#name " = " #value)
EOF

# processing with awk
$awk '/#define/ { print "    DEFINE_MACRO(" $2 ", " $3 ");"; }' $ifile >> $ofile

# footer
cat >> $ofile << EOF
#   undef DEFINE_MACRO
}
EOF

# insert this script
echo "/""*" >> $ofile
cat $0 >> $ofile
echo "*""/" >> $ofile

exit 0
*/
