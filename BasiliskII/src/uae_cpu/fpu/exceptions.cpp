/*
 *  fpu/exceptions.cpp - system-dependant FPU exceptions management
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  MC68881/68040 fpu emulation
 *  
 *  Original UAE FPU, copyright 1996 Herman ten Brugge
 *  Rewrite for x86, copyright 1999-2000 Lauri Pesonen
 *  New framework, copyright 2000 Gwenole Beauchesne
 *  Adapted for JIT compilation (c) Bernd Meyer, 2000
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#undef	PRIVATE
#define	PRIVATE /**/

#undef	PUBLIC
#define	PUBLIC	/**/

#undef	FFPU
#define	FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* -------------------------------------------------------------------------- */
/* --- Native X86 exceptions                                              --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_X86_EXCEPTIONS
void FFPU fpu_init_native_exceptions(void)
{
	// Mapping for "sw" -> fpsr exception byte
	for (uae_u32 i = 0; i < 0x80; i++) {
		exception_host2mac[i] = 0;
		
		if(i & SW_FAKE_BSUN) {
			exception_host2mac[i] |= FPSR_EXCEPTION_BSUN;
		}
		// precision exception
		if(i & SW_PE) {
			exception_host2mac[i] |= FPSR_EXCEPTION_INEX2;
		}
		// underflow exception
		if(i & SW_UE) {
			exception_host2mac[i] |= FPSR_EXCEPTION_UNFL;
		}
		// overflow exception
		if(i & SW_OE) {
			exception_host2mac[i] |= FPSR_EXCEPTION_OVFL;
		}
		// zero divide exception
		if(i & SW_ZE) {
			exception_host2mac[i] |= FPSR_EXCEPTION_DZ;
		}
		// denormalized operand exception.
		// wrong, but should not get here, normalization is done in elsewhere
		if(i & SW_DE) {
			exception_host2mac[i] |= FPSR_EXCEPTION_SNAN;
		}
		// invalid operation exception
		if(i & SW_IE) {
			exception_host2mac[i] |= FPSR_EXCEPTION_OPERR;
		}
	}
	
	// Mapping for fpsr exception byte -> "sw"
	for (uae_u32 i = 0; i < 0x100; i++) {
		uae_u32 fpsr = (i << 8);
		exception_mac2host[i] = 0;
		
		// BSUN; make sure that you don't generate FPU stack faults.
		if(fpsr & FPSR_EXCEPTION_BSUN) {
			exception_mac2host[i] |= SW_FAKE_BSUN;
		}
		// precision exception
		if(fpsr & FPSR_EXCEPTION_INEX2) {
			exception_mac2host[i] |= SW_PE;
		}
		// underflow exception
		if(fpsr & FPSR_EXCEPTION_UNFL) {
			exception_mac2host[i] |= SW_UE;
		}
		// overflow exception
		if(fpsr & FPSR_EXCEPTION_OVFL) {
			exception_mac2host[i] |= SW_OE;
		}
		// zero divide exception
		if(fpsr & FPSR_EXCEPTION_DZ) {
			exception_mac2host[i] |= SW_ZE;
		}
		// denormalized operand exception
		if(fpsr & FPSR_EXCEPTION_SNAN) {
			exception_mac2host[i] |= SW_DE; //Wrong
		}
		// invalid operation exception
		if(fpsr & FPSR_EXCEPTION_OPERR) {
			exception_mac2host[i] |= SW_IE;
		}
	}
}
#endif

#ifdef FPU_USE_X86_ACCRUED_EXCEPTIONS
void FFPU fpu_init_native_accrued_exceptions(void)
{
	/*
		68881/68040 accrued exceptions accumulate as follows:
			Accrued.IOP		|= (Exception.SNAN | Exception.OPERR)
			Accrued.OVFL	|= (Exception.OVFL)
			Accrued.UNFL	|= (Exception.UNFL | Exception.INEX2)
			Accrued.DZ		|= (Exception.DZ)
			Accrued.INEX	|= (Exception.INEX1 | Exception.INEX2 | Exception.OVFL)
	*/

	// Mapping for "fpsr.accrued_exception" -> fpsr accrued exception byte
	for (uae_u32 i = 0; i < 0x40; i++ ) {
		accrued_exception_host2mac[i] = 0;

		// precision exception
		if(i & SW_PE) {
			accrued_exception_host2mac[i] |= FPSR_ACCR_INEX;
		}
		// underflow exception
		if(i & SW_UE) {
			accrued_exception_host2mac[i] |= FPSR_ACCR_UNFL;
		}
		// overflow exception
		if(i & SW_OE) {
			accrued_exception_host2mac[i] |= FPSR_ACCR_OVFL;
		}
		// zero divide exception
		if(i & SW_ZE) {
			accrued_exception_host2mac[i] |= FPSR_ACCR_DZ;
		}
		// denormalized operand exception
		if(i & SW_DE) {
			accrued_exception_host2mac[i] |= FPSR_ACCR_IOP; //??????
		}
		// invalid operation exception
		if(i & SW_IE) {
			accrued_exception_host2mac[i] |= FPSR_ACCR_IOP;
		}
	}

	// Mapping for fpsr accrued exception byte -> "fpsr.accrued_exception"
	for (uae_u32 i = 0; i < 0x20; i++) {
		int fpsr = (i << 3);
		accrued_exception_mac2host[i] = 0;

		// precision exception
		if(fpsr & FPSR_ACCR_INEX) {
			accrued_exception_mac2host[i] |= SW_PE;
		}
		// underflow exception
		if(fpsr & FPSR_ACCR_UNFL) {
			accrued_exception_mac2host[i] |= SW_UE;
		}
		// overflow exception
		if(fpsr & FPSR_ACCR_OVFL) {
			accrued_exception_mac2host[i] |= SW_OE;
		}
		// zero divide exception
		if(fpsr & FPSR_ACCR_DZ) {
			accrued_exception_mac2host[i] |= SW_ZE;
		}
		// What about SW_DE; //??????
		// invalid operation exception
		if(fpsr & FPSR_ACCR_IOP) {
			accrued_exception_mac2host[i] |= SW_IE;
		}
	}
}
#endif
