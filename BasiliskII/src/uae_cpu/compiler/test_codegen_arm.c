/* Example of using sigaction() to setup a signal handler with 3 arguments
 * including siginfo_t.
 */
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "flags_arm.h"
#include "codegen_arm.h"

#define TEST(c,ex,s) 	{ c; if (opcode != ex) printf("(%s) Invalid opcode %x expected %x\n", s, opcode, ex); }

int opcode;

void emit_long(v) {
	opcode = v;
}

int main (int argc, char *argv[])
{
TEST(MOV_ri(8, 15), 0xe3a0800f, "mov r8,#15");
TEST(MOV_rr(8,9), 0xe1a08009, "mov r8, r9");
TEST(MOV_rrLSLi(8,9,5), 0xe1a08289, "lsl r8, r9, #5");
TEST(MOV_rrLSLr(8,9,7), 0xe1a08719, "lsl r8, r9, r7");
TEST(MOV_rrLSRi(8,9,5), 0xe1a082a9, "lsr r8, r9, #5");
TEST(MOV_rrLSRr(8,9,7), 0xe1a08739, "lsr r8, r9, r7");
TEST(MOV_rrASRi(8,9,5), 0xe1a082c9, "asr r8, r9, #5");
TEST(MOV_rrASRr(8,9,7), 0xe1a08759, "asr r8, r9, r7");
TEST(MOV_rrRORi(8,9,5), 0xe1a082e9, "ror r8, r9, #5");
TEST(MOV_rrRORr(8,9,7), 0xe1a08779, "ror r8, r9, r7");
TEST(MOV_rrRRX(8,9), 0xe1a08069, "rrx r8, r9");

TEST(MOVS_ri(8, 15), 0xe3b0800f, "movs r8,#15");
TEST(MOVS_rr(8,9), 0xe1b08009, "movs r8, r9");
TEST(MOVS_rrLSLi(8,9,5), 0xe1b08289, "lsls r8, r9, #5");
TEST(MOVS_rrLSLr(8,9,7), 0xe1b08719, "lsls r8, r9, r7");
TEST(MOVS_rrLSRi(8,9,5), 0xe1b082a9, "lsrs r8, r9, #5");
TEST(MOVS_rrLSRr(8,9,7), 0xe1b08739, "lsrs r8, r9, r7");
TEST(MOVS_rrASRi(8,9,5), 0xe1b082c9, "asrs r8, r9, #5");
TEST(MOVS_rrASRr(8,9,7), 0xe1b08759, "asrs r8, r9, r7");
TEST(MOVS_rrRORi(8,9,5), 0xe1b082e9, "rors r8, r9, #5");
TEST(MOVS_rrRORr(8,9,7), 0xe1b08779, "rors r8, r9, r7");
TEST(MOVS_rrRRX(8,9), 0xe1b08069, "rrxs r8, r9");

TEST(MVN_ri(8, 15), 0xe3e0800f, "mvn r8,#15");
TEST(MVN_rr(8,9), 0xe1e08009, "mvn r8, r9");
TEST(MVN_rrLSLi(8,9,5), 0xe1e08289, "mvn r8, r9, lsl #5");
TEST(MVN_rrLSLr(8,9,7), 0xe1e08719, "mvn r8, r9, lsl r7");
TEST(MVN_rrLSRi(8,9,5), 0xe1e082a9, "mvn r8, r9, lsr #5");
TEST(MVN_rrLSRr(8,9,7), 0xe1e08739, "mvn r8, r9, lsr r7");
TEST(MVN_rrASRi(8,9,5), 0xe1e082c9, "mvn r8, r9, asr #5");
TEST(MVN_rrASRr(8,9,7), 0xe1e08759, "mvn r8, r9, asr r7");
TEST(MVN_rrRORi(8,9,5), 0xe1e082e9, "mvn r8, r9, ror #5");
TEST(MVN_rrRORr(8,9,7), 0xe1e08779, "mvn r8, r9, ror r7");
TEST(MVN_rrRRX(8,9), 0xe1e08069, "mvn r8, r9, rrx");

TEST(CMP_ri(8, 15), 0xe358000f, "cmp r8,#15");
TEST(CMP_rr(8,9), 0xe1580009, "cmp r8, r9");
TEST(CMP_rrLSLi(8,9,5), 0xe1580289, "cmp r8, r9, #5");
TEST(CMP_rrLSLr(8,9,7), 0xe1580719, "cmp r8, r9, r7");
TEST(CMP_rrLSRi(8,9,5), 0xe15802a9, "cmp r8, r9, #5");
TEST(CMP_rrLSRr(8,9,7), 0xe1580739, "cmp r8, r9, r7");
TEST(CMP_rrASRi(8,9,5), 0xe15802c9, "cmp r8, r9, #5");
TEST(CMP_rrASRr(8,9,7), 0xe1580759, "cmp r8, r9, r7");
TEST(CMP_rrRORi(8,9,5), 0xe15802e9, "cmp r8, r9, #5");
TEST(CMP_rrRORr(8,9,7), 0xe1580779, "cmp r8, r9, r7");
TEST(CMP_rrRRX(8,9), 0xe1580069, "cmp r8, r9");

TEST(CMP_ri(8, 0x81), 0xe3580081, "cmp r8,#0x81");
TEST(CMP_ri(8, 0x204), 0xe3580f81, "cmp r8,#0x204");
TEST(CMP_ri(8, 0x810), 0xe3580e81, "cmp r8,#0x8100");
TEST(CMP_ri(8, 0x2040), 0xe3580d81, "cmp r8,#0x2040");
TEST(CMP_ri(8, 0x8100), 0xe3580c81, "cmp r8,#0x8100");
TEST(CMP_ri(8, 0x20400), 0xe3580b81, "cmp r8,#0x20400");
TEST(CMP_ri(8, 0x81000), 0xe3580a81, "cmp r8,#0x81000");
TEST(CMP_ri(8, 0x204000), 0xe3580981, "cmp r8,#0x204000");
TEST(CMP_ri(8, 0x810000), 0xe3580881, "cmp r8,#0x810000");
TEST(CMP_ri(8, 0x2040000), 0xe3580781, "cmp r8,#0x2040000");
TEST(CMP_ri(8, 0x8100000), 0xe3580681, "cmp r8,#0x8100000");
TEST(CMP_ri(8, 0x20400000), 0xe3580581, "cmp r8,#0x20400000");
TEST(CMP_ri(8, 0x81000000), 0xe3580481, "cmp r8,#0x81000000");
TEST(CMP_ri(8, 0x04000002), 0xe3580381, "cmp r8,#0x04000002");
TEST(CMP_ri(8, 0x10000008), 0xe3580281, "cmp r8,#0x10000008");
TEST(CMP_ri(8, 0x40000020), 0xe3580181, "cmp r8,#0x40000020");

TEST(CMP_ri(8, 0x1200), 0xe3580c12, "cmp r8,#0x1200");
TEST(CMP_ri(8, 0x120000), 0xe3580812, "cmp r8,#0x120000");
TEST(CMP_ri(8, 0x12000000), 0xe3580412, "cmp r8,#0x12000000");

TEST(BEQ_i(5), 0x0a000005, "beq #5");
TEST(BNE_i(5), 0x1a000005, "bne #5");    
TEST(BCS_i(5), 0x2a000005, "bcs #5");
TEST(BCC_i(5), 0x3a000005, "bcc #5");
TEST(BMI_i(5), 0x4a000005, "bmi #5");   
TEST(BPL_i(5), 0x5a000005, "bpl #5");
TEST(BVS_i(5), 0x6a000005, "bvs #5");   
TEST(BVC_i(5), 0x7a000005, "bvc #5");   
TEST(BHI_i(5), 0x8a000005, "bhi #5");   
TEST(BLS_i(5), 0x9a000005, "bls #5");   
TEST(BGE_i(5), 0xaa000005, "bge #5");   
TEST(BLT_i(5), 0xba000005, "blt #5");   
TEST(BGT_i(5), 0xca000005, "bgt #5");   
TEST(BLE_i(5), 0xda000005, "ble #5");   
TEST(B_i(5), 0xea000005, "b #5");     

TEST(BL_i(5), 0xeb000005, "bl #5");
TEST(BLX_r(8), 0xe12fff38, "blx r8");
TEST(BX_r(8), 0xe12fff18, "bx r8");

TEST(EOR_rri(6, 8, 15), 0xe228600f, "eor r6, r8,#15");
TEST(EOR_rrr(6, 8,9), 0xe0286009, "eor r6, r8, r9");
TEST(EOR_rrrLSLi(6,8,9,5), 0xe0286289, "eor r6, r8, r9, lsl #5");
TEST(EOR_rrrLSLr(6,8,9,7), 0xe0286719, "eor r6, r8, r9, lsl r7");
TEST(EOR_rrrLSRi(6,8,9,5), 0xe02862a9, "eor r6, r8, r9, lsr #5");
TEST(EOR_rrrLSRr(6,8,9,7), 0xe0286739, "eor r6, r8, r9, lsr r7");
TEST(EOR_rrrASRi(6,8,9,5), 0xe02862c9, "eor r6, r8, r9, asr #5");
TEST(EOR_rrrASRr(6,8,9,7), 0xe0286759, "eor r6, r8, r9, asr r7");
TEST(EOR_rrrRORi(6,8,9,5), 0xe02862e9, "eor r6, r8, r9, ror #5");
TEST(EOR_rrrRORr(6,8,9,7), 0xe0286779, "eor r6, r8, r9, ror r7");
TEST(EOR_rrrRRX(6,8,9), 0xe0286069, "eor r6, r8, r9, rrx");

TEST(EORS_rri(6, 8, 15), 0xe238600f, "eors  r6, r8,#15");
TEST(EORS_rrr(6, 8,9), 0xe0386009, "eors  r6, r8, r9");
TEST(EORS_rrrLSLi(6,8,9,5), 0xe0386289, "eors  r6, r8, r9, lsl #5");
TEST(EORS_rrrLSLr(6,8,9,7), 0xe0386719, "eors  r6, r8, r9, lsr r7");
TEST(EORS_rrrLSRi(6,8,9,5), 0xe03862a9, "eors  r6, r8, r9, lsr #5");
TEST(EORS_rrrLSRr(6,8,9,7), 0xe0386739, "eors  r6, r8, r9, lsr r7");
TEST(EORS_rrrASRi(6,8,9,5), 0xe03862c9, "eors  r6, r8, r9, asr #5");
TEST(EORS_rrrASRr(6,8,9,7), 0xe0386759, "eors  r6, r8, r9, asr r7");
TEST(EORS_rrrRORi(6,8,9,5), 0xe03862e9, "eors  r6, r8, r9, ror #5");
TEST(EORS_rrrRORr(6,8,9,7), 0xe0386779, "eors  r6, r8, r9, ror r7");
TEST(EORS_rrrRRX(6,8,9), 0xe0386069, "eors  r6, r8, r9, rrx");

TEST(MRS_CPSR(6), 0xe10f6000, "mrs	r6, CPSR");
TEST(MRS_SPSR(6), 0xe14f6000, "mrs	r6, SPSR");

TEST(MSR_CPSR_i(5), 0xe329f005, "msr	CPSR_fc, #5");
TEST(MSR_CPSR_r(5), 0xe129f005, "msr	CPSR_fc, r5");

TEST(MSR_CPSRf_i(5), 0xe328f005, "msr	CPSR_f, #5");
TEST(MSR_CPSRf_r(5), 0xe128f005, "msr	CPSR_f, r5");

TEST(MSR_CPSRc_i(5), 0xe321f005, "msr	CPSR_c, #5");
TEST(MSR_CPSRc_r(5), 0xe121f005, "msr	CPSR_c, r5");

TEST(PUSH(6), 0xe92d0040, "push {r6}");
TEST(POP(6), 0xe8bd0040, "pop	{r6}");

TEST(BIC_rri(0, 0, 0x9f000000), 0xe3c0049f, "bic r0, r0, #0x9f000000");
TEST(BIC_rri(2, 3, 0xff00), 0xe3c32cff, "bic r2, r3, #0xff00");
TEST(BIC_rri(3, 4, 0xff), 0xe3c430ff, "bic r3, r4, #0xff");

TEST(ORR_rrrLSRi(0, 1, 2, 16), 0xe1810822, "orr	r0, r1, r2, lsr #16");
TEST(ORR_rrrLSRi(0, 1, 2, 24), 0xe1810c22, "orr	r0, r1, r2, lsr #24");

TEST(LDR_rR(8, 9), 0xe5998000, "ldr     r8, [r9]");
TEST(LDR_rRI(8, 9, 4), 0xe5998004, "ldr     r8, [r9, #4]");
TEST(LDR_rRi(8, 9, 4), 0xe5198004, "ldr     r8, [r9, #-4]");
TEST(LDR_rRR(8, 9, 7), 0xe7998007, "ldr     r8, [r9, r7]");
TEST(LDR_rRr(8, 9, 7), 0xe7198007, "ldr     r8, [r9, -r7]");
TEST(LDR_rRR_LSLi(8, 9, 7, 5), 0xe7998287, "ldr     r8, [r9, r7, lsl #5]");
TEST(LDR_rRr_LSLi(8, 9, 7, 5), 0xe7198287, "ldr     r8, [r9, -r7, lsl #5]");
TEST(LDR_rRR_LSRi(8, 9, 7, 5), 0xe79982a7, "ldr     r8, [r9, r7, lsr #5]");
TEST(LDR_rRr_LSRi(8, 9, 7, 5), 0xe71982a7, "ldr     r8, [r9, -r7, lsr #5]");
TEST(LDR_rRR_ASRi(8, 9, 7, 5), 0xe79982c7, "ldr     r8, [r9, r7, asr #5]");
TEST(LDR_rRr_ASRi(8, 9, 7, 5), 0xe71982c7, "ldr     r8, [r9, -r7, asr #5]");
TEST(LDR_rRR_RORi(8, 9, 7, 5), 0xe79982e7, "ldr     r8, [r9, r7, ror #5]");
TEST(LDR_rRr_RORi(8, 9, 7, 5), 0xe71982e7, "ldr     r8, [r9, -r7, ror #5]");
TEST(LDR_rRR_RRX(8, 9, 7), 0xe7998067, "ldr     r8, [r9, r7, rrx]");
TEST(LDR_rRr_RRX(8, 9, 7), 0xe7198067, "ldr     r8, [r9, -r7, rrx]");

TEST(LDRB_rR(8, 9), 0xe5d98000, "ldrb r8, [r9]");
TEST(LDRB_rRI(8, 9, 4), 0xe5d98004, "ldrb r8, [r9, #4]");
TEST(LDRB_rRi(8, 9, 4), 0xe5598004, "ldrb r8, [r9, #-4]");
TEST(LDRB_rRR(8, 9, 7), 0xe7d98007, "ldrb r8, [r9, r7]");
TEST(LDRB_rRr(8, 9, 7), 0xe7598007, "ldrb r8, [r9, -r7]");
TEST(LDRB_rRR_LSLi(8, 9, 7, 5), 0xe7d98287, "ldrb r8, [r9, r7, lsl #5]");
TEST(LDRB_rRr_LSLi(8, 9, 7, 5), 0xe7598287, "ldrb r8, [r9, -r7, lsl #5]");
TEST(LDRB_rRR_LSRi(8, 9, 7, 5), 0xe7d982a7, "ldrb r8, [r9, r7, lsr #5]");
TEST(LDRB_rRr_LSRi(8, 9, 7, 5), 0xe75982a7, "ldrb r8, [r9, -r7, lsr #5]");
TEST(LDRB_rRR_ASRi(8, 9, 7, 5), 0xe7d982c7, "ldrb r8, [r9, r7, asr #5]");
TEST(LDRB_rRr_ASRi(8, 9, 7, 5), 0xe75982c7, "ldrb r8, [r9, -r7, asr #5]");
TEST(LDRB_rRR_RORi(8, 9, 7, 5), 0xe7d982e7, "ldrb r8, [r9, r7, ror #5]");
TEST(LDRB_rRr_RORi(8, 9, 7, 5), 0xe75982e7, "ldrb r8, [r9, -r7, ror #5]");
TEST(LDRB_rRR_RRX(8, 9, 7), 0xe7d98067, "ldrb r8, [r9, r7, rrx]");
TEST(LDRB_rRr_RRX(8, 9, 7), 0xe7598067, "ldrb r8, [r9, -r7, rrx]");

TEST(LDRSB_rR(8, 9), 0xe1d980d0, "ldrsb r8, [r9]");
TEST(LDRSB_rRI(8, 9, 4), 0xe1d980d4, "ldrsb r8, [r9, #4]");
TEST(LDRSB_rRi(8, 9, 4), 0xe15980d4, "ldrsb r8, [r9, #-4]");
TEST(LDRSB_rRR(8, 9, 7), 0xe19980d7, "ldrsb r8, [r9, r7]");
TEST(LDRSB_rRr(8, 9, 7), 0xe11980d7, "ldrsb r8, [r9, -r7]");

TEST(LDRSH_rR(8, 9), 0xe1d980f0, "ldrsh r8, [r9]");
TEST(LDRSH_rRI(8, 9, 4), 0xe1d980f4, "ldrsh r8, [r9, #4]");
TEST(LDRSH_rRi(8, 9, 4), 0xe15980f4, "ldrsh r8, [r9, #-4]");
TEST(LDRSH_rRR(8, 9, 7), 0xe19980f7, "ldrsh r8, [r9, r7]");
TEST(LDRSH_rRr(8, 9, 7), 0xe11980f7, "ldrsh r8, [r9, -r7]");

TEST(LDRH_rR(8, 9), 0xe1d980b0, "ldrh r8, [r9]");
TEST(LDRH_rRI(8, 9, 4), 0xe1d980b4, "ldrh r8, [r9, #4]");
TEST(LDRH_rRi(8, 9, 4), 0xe15980b4, "ldrh r8, [r9, #-4]");
TEST(LDRH_rRR(8, 9, 7), 0xe19980b7, "ldrh r8, [r9, r7]");
TEST(LDRH_rRr(8, 9, 7), 0xe11980b7, "ldrh r8, [r9, -r7]");

TEST(STR_rRR(8,9,7), 0xe7898007, "str r8, [r9, r7]");
TEST(STR_rRr(8,9,7), 0xe7098007, "str r8, [r9, -r7]");

TEST(STRB_rR(5, 6), 0xe5c65000, "strb r5,[r6]");

TEST(STRH_rR(8, 9), 0xe1c980b0, "strh r8, [r9]");
TEST(STRH_rRI(8, 9, 4), 0xe1c980b4, "strh r8, [r9, #4]");
TEST(STRH_rRi(8, 9, 4), 0xe14980b4, "strh r8, [r9, #-4]");
TEST(STRH_rRR(8, 9, 7), 0xe18980b7, "strh r8, [r9, r7]");
TEST(STRH_rRr(8, 9, 7), 0xe10980b7, "strh r8, [r9, -r7]");

TEST(CLZ_rr(2, 3), 0xe16f2f13, "clz r2,r3");
TEST(REV_rr(2, 3), 0xe6bf2f33, "rev r2, r3");
TEST(REV16_rr(2, 3), 0xe6bf2fb3, "rev16 r2, r3");
TEST(REVSH_rr(2, 3), 0xe6ff2fb3, "revsh r2, r3");

TEST(SXTB_rr(2,3), 0xe6af2073, "sxtb r2,r3");
TEST(SXTB_rr(3,4), 0xe6af3074, "sxtb r3,r4");

TEST(SXTB_rr_ROR8(2,3), 0xe6af2473, "sxtb	r2, r3, ror #8");
TEST(SXTB_rr_ROR16(2,3), 0xe6af2873, "sxtb	r2, r3, ror #16");
TEST(SXTB_rr_ROR24(2,3), 0xe6af2c73, "sxtb	r2, r3, ror #24");
TEST(SXTH_rr(2,3), 0xe6bf2073, "sxth	r2, r3");
TEST(SXTH_rr_ROR8(2,3), 0xe6bf2473, "sxth	r2, r3, ror #8");
TEST(SXTH_rr_ROR16(2,3), 0xe6bf2873, "sxth	r2, r3, ror #16");
TEST(SXTH_rr_ROR24(2,3), 0xe6bf2c73, "sxth	r2, r3, ror #24");
TEST(UXTB_rr(2,3), 0xe6ef2073, "uxtb	r2, r3");
TEST(UXTB_rr_ROR8(2,3), 0xe6ef2473, "uxtb	r2, r3, ror #8");
TEST(UXTB_rr_ROR16(2,3), 0xe6ef2873, "uxtb	r2, r3, ror #16");
TEST(UXTB_rr_ROR24(2,3), 0xe6ef2c73, "uxtb	r2, r3, ror #24");
TEST(UXTH_rr(2,3), 0xe6ff2073, "uxth	r2, r3");
TEST(UXTH_rr_ROR8(2,3), 0xe6ff2473, "uxth	r2, r3, ror #8");
TEST(UXTH_rr_ROR16(2,3), 0xe6ff2873, "uxth	r2, r3, ror #16");
TEST(UXTH_rr_ROR24(2,3), 0xe6ff2c73, "uxth	r2, r3, ror #24");

TEST(REV_rr(2,3), 0xe6bf2f33, "rev	r2, r3");
TEST(REV16_rr(2,3), 0xe6bf2fb3, "rev16	r2, r3");
TEST(REVSH_rr(2,3), 0xe6ff2fb3, "revsh	r2, r3");

TEST(CC_MOV_ri(NATIVE_CC_CS, 4,1), 0x23a04001, "movcs	r4, #1");
TEST(CC_MOV_ri(NATIVE_CC_CC, 4,1), 0x33a04001, "movcc	r4, #1");

int imm = 0x9f;
TEST(ADDS_rri(0, 0, imm << 24), 0xe290049f, "adds r0, r0, 0x9f000000");

TEST(PKHBT_rrr(1, 2, 3), 0xe6821013, "pkhbt r1,r2,r3");
TEST(MVN_ri8(1,2), 0xe3e01002, "mvn r1,#2");

TEST(ORR_rri8RORi(1,2,0x12,24), 0xe3821c12, "orr	r1, r2, #0x1200");
TEST(PKHTB_rrrASRi(1, 2, 3, 4), 0xe6821253, "pkhtb r1,r2,r3,ASR #4");
TEST(PKHBT_rrrLSLi(1, 2, 3, 4), 0xe6821213, "pkhbt r1,r2,r3,LSL #4");

TEST(MUL_rrr(1,2,3), 0xe0010392, "mul	r1, r2, r3");
TEST(MULS_rrr(1,2,3), 0xe0110392, "muls	r1, r2, r3");


}

