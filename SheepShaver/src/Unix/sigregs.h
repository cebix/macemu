/*
 *  sigregs.h - Extract machine registers from a signal frame
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

#ifndef SIGREGS_H
#define SIGREGS_H

#ifndef EMULATED_PPC

// Common representation of machine registers
struct sigregs {
	uint32 nip;
	uint32 link;
	uint32 ctr;
	uint32 msr;
	uint32 xer;
	uint32 ccr;
	uint32 gpr[32];
};

// Extract machine registers from Linux signal frame
#if defined(__linux__)
#include <sys/ucontext.h>
#define MACHINE_REGISTERS(scp)	((machine_regs *)(((ucontext_t *)scp)->uc_mcontext.regs))

struct machine_regs : public pt_regs
{
	u_long & cr()				{ return pt_regs::ccr; }
	uint32 cr() const			{ return pt_regs::ccr; }
	uint32 lr() const			{ return pt_regs::link; }
	uint32 ctr() const			{ return pt_regs::ctr; }
	uint32 xer() const			{ return pt_regs::xer; }
	uint32 msr() const			{ return pt_regs::msr; }
	uint32 dar() const			{ return pt_regs::dar; }
	u_long & pc()				{ return pt_regs::nip; }
	uint32 pc() const			{ return pt_regs::nip; }
	u_long & gpr(int i)			{ return pt_regs::gpr[i]; }
	uint32 gpr(int i) const		{ return pt_regs::gpr[i]; }
};
#endif

// Extract machine registers from NetBSD signal frame
#if defined(__NetBSD__)
#include <sys/ucontext.h>
#define MACHINE_REGISTERS(scp)	((machine_regs *)&(((ucontext_t *)scp)->uc_mcontext))

struct machine_regs : public mcontext_t
{
	long & cr()					{ return __gregs[_REG_CR]; }
	uint32 cr() const			{ return __gregs[_REG_CR]; }
	uint32 lr() const			{ return __gregs[_REG_LR]; }
	uint32 ctr() const			{ return __gregs[_REG_CTR]; }
	uint32 xer() const			{ return __gregs[_REG_XER]; }
	uint32 msr() const			{ return __gregs[_REG_MSR]; }
	uint32 dar() const			{ return (uint32)(((siginfo_t *)(((unsigned long)this) - offsetof(ucontext_t, uc_mcontext))) - 1)->si_addr; } /* HACK */
	long & pc()					{ return __gregs[_REG_PC]; }
	uint32 pc() const			{ return __gregs[_REG_PC]; }
	long & gpr(int i)			{ return __gregs[_REG_R0 + i]; }
	uint32 gpr(int i) const		{ return __gregs[_REG_R0 + i]; }
};
#endif

// Extract machine registers from Darwin signal frame
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/signal.h>
extern "C" int sigaltstack(const struct sigaltstack *ss, struct sigaltstack *oss);

#include <sys/ucontext.h>
#define MACHINE_REGISTERS(scp)	((machine_regs *)(((ucontext_t *)scp)->uc_mcontext))

struct machine_regs : public mcontext
{
	uint32 & cr()				{ return ss.cr; }
	uint32 cr() const			{ return ss.cr; }
	uint32 lr() const			{ return ss.lr; }
	uint32 ctr() const			{ return ss.ctr; }
	uint32 xer() const			{ return ss.xer; }
	uint32 msr() const			{ return ss.srr1; }
	uint32 dar() const			{ return es.dar; }
	uint32 & pc()				{ return ss.srr0; }
	uint32 pc() const			{ return ss.srr0; }
	uint32 & gpr(int i)			{ return (&ss.r0)[i]; }
	uint32 gpr(int i) const		{ return (&ss.r0)[i]; }
};
#endif

// Convert system-dependent machine registers to generic sigregs
static void build_sigregs(sigregs *srp, machine_regs *mrp)
{
	srp->nip = mrp->pc();
	srp->link = mrp->lr();
	srp->ctr = mrp->ctr();
	srp->msr = mrp->msr();
	srp->xer = mrp->xer();
	srp->ccr = mrp->cr();
	for (int i = 0; i < 32; i++)
		srp->gpr[i] = mrp->gpr(i);
}

#endif

#endif /* SIGREGS_H */
