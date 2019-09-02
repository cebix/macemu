#
# Note: this Makefile only contains rules for the source
# generator tools.
#

#
# suppress warnings about overriding LDFLAGS and CPPFLAGS
#
AUTOMAKE_OPTIONS = -Wno-gnu

AM_CPPFLAGS = $(DEFINES) \
	"-I$(srcdir)/../include" \
	"-I$(srcdir)/../Unix" \
	"-I$(builddir)/.." \
	"-I$(builddir)" \
	"-I$(srcdir)"

CC = $(CC_FOR_BUILD)
CXX = $(CXX_FOR_BUILD)

LDFLAGS = $(LDFLAGS_FOR_BUILD)
CPPFLAGS = $(CPPFLAGS_FOR_BUILD)
CFLAGS = $(CFLAGS_FOR_BUILD)
CXXFLAGS = $(CXXFLAGS_FOR_BUILD)
LIBS=-lm

CFLAGS_NOWARN = $(DBGSP)
AM_CFLAGS = $(CFLAGS_NOWARN) $(WFLAGS)
AM_CXXFLAGS = $(CFLAGS_NOWARN) $(WFLAGS)

noinst_PROGRAMS = build68k gencpu
if USE_JIT
noinst_PROGRAMS += gencomp
endif

BUILT_SOURCES = \
	cpudefs.cpp \
	cpuemu.cpp \
	cpustbl.cpp \
	cpufunctbl.cpp \
	cputbl.h \
	$(empty)

build68k_SOURCES = build68k.c
gencpu_SOURCES = gencpu.c m68k.h readcpu.cpp readcpu.h cpudefs.cpp
gencomp_SOURCES = 
if GENCOMP_ARCH_X86
gencomp_SOURCES += compiler/gencomp.c
endif
if GENCOMP_ARCH_ARM
gencomp_SOURCES += compiler/gencomp_arm.c
endif
gencomp_SOURCES += readcpu.cpp cpudefs.cpp

if USE_JIT
BUILT_SOURCES += compemu.cpp compstbl.cpp comptbl.h
endif


cpudefs.cpp: build68k$(EXEEXT) $(srcdir)/table68k
	$(AM_V_GEN)./build68k <$(srcdir)/table68k > $@
cpuemu.cpp: gencpu$(EXEEXT)
	$(AM_V_GEN)./gencpu$(EXEEXT)
cpustbl.cpp cpufunctbl.cpp cputbl.h: cpuemu.cpp
compemu.cpp: gencomp$(EXEEXT)
	$(AM_V_GEN)./gencomp$(EXEEXT)
compstbl.cpp comptbl.h: compemu.cpp

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	table68k \
	compiler/codegen_arm.cpp compiler/codegen_arm.h \
	compiler/compemu_midfunc_arm.cpp compiler/compemu_midfunc_arm.h \
	compiler/compemu_midfunc_arm2.cpp compiler/compemu_midfunc_arm2.h \
	compiler/test_codegen_arm.c \
	compiler/codegen_x86.cpp compiler/codegen_x86.h \
	compiler/compemu_midfunc_x86.cpp compiler/compemu_midfunc_x86.h \
	compiler/test_codegen_x86.cpp \
	$(empty)
