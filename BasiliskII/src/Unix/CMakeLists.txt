cmake_minimum_required(VERSION 3.0.0)
project(BasiliskII)


find_package(SDL2 REQUIRED)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(IOKIT_LIBRARY IOKit)
include_directories(../include . ../CrossPlatform ../uae_cpu ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

add_executable(build68k ../uae_cpu/build68k.c)

add_custom_command(OUTPUT cpudefs.cpp
	COMMAND build68k < ${CMAKE_CURRENT_SOURCE_DIR}/../uae_cpu/table68k > cpudefs.cpp
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../uae_cpu/table68k build68k)

add_executable(gencpu ../uae_cpu/gencpu.c ../uae_cpu/readcpu.cpp cpudefs.cpp)

#add_custom_command(OUTPUT cpuemu.cpp cpustbl.cpp cpufunctbl.cpp COMMAND gencpu DEPENDS gencpu)
add_custom_command(OUTPUT cpuemu.cpp cpuemu_nf.cpp cpustbl.cpp cpustbl_nf.cpp COMMAND gencpu DEPENDS gencpu)

add_executable(gencomp ../uae_cpu/compiler/gencomp.c ../uae_cpu/readcpu.cpp cpudefs.cpp)

add_custom_command(OUTPUT compemu.cpp compstbl.cpp comptbl.h COMMAND gencomp DEPENDS gencomp)

set(BasiliskII_SRCS 
	../main.cpp
	../prefs.cpp
	../prefs_items.cpp
	sys_unix.cpp
	../rom_patches.cpp
	../slot_rom.cpp
	../rsrc_patches.cpp
	../emul_op.cpp
	../macos_util.cpp
	../xpram.cpp
	xpram_unix.cpp
	../timer.cpp
	timer_unix.cpp
	../adb.cpp
	../serial.cpp
	../ether.cpp
	../sony.cpp
	../disk.cpp
	../cdrom.cpp
	../scsi.cpp
	../video.cpp
	../audio.cpp
	../extfs.cpp
	../user_strings.cpp
	user_strings_unix.cpp
	# XPLAT_SRCS
	../CrossPlatform/vm_alloc.cpp
	../CrossPlatform/sigsegv.cpp
	../CrossPlatform/video_blit.cpp
	#SYSSRC
	#SDL USE_SDL USE_SDL_VIDEO USE_SDL_AUDIO
	#video src
	../SDL/video_sdl2.cpp
	#EXTFSSRC
	extfs_unix.cpp
	#Serial src
	../dummy/serial_dummy.cpp
	#ether src
	../dummy/ether_dummy.cpp
	#scsi src
	../dummy/scsi_dummy.cpp
	#audio src
	../SDL/audio_sdl.cpp
	#ui src
	../dummy/prefs_editor_dummy.cpp
	#extra sys 
	Darwin/sys_darwin.cpp
	main_unix.cpp
	prefs_unix.cpp
	../dummy/clip_dummy.cpp

	#CPU srcs
	#add define -DFPU_UAE
	../uae_cpu/basilisk_glue.cpp
	../uae_cpu/newcpu.cpp
	../uae_cpu/readcpu.cpp
	../uae_cpu/fpu/fpu_ieee.cpp
	cpustbl.cpp
	cpudefs.cpp
	cpuemu.cpp
	compemu.cpp
	compstbl.cpp
	../uae_cpu/compiler/compemu_support.cpp 
	../uae_cpu/compiler/compemu_fpp.cpp
	cpustbl_nf.cpp
	cpuemu_nf.cpp
	#addressing mode =direct -DDIRECT_ADDRESSING
	#includes 
)

add_executable(BasiliskII ${BasiliskII_SRCS})

set_source_files_properties(${BasiliskII_SRCS}
	PROPERTIES COMPILE_FLAGS "-DDIRECT_ADDRESSING -DCPU_x86_64 -DFIXED_ADDRESSING -DCPU_64_BIT -DNOFLAGS_SUPPORT -DFPU_IEEE -DUSE_JIT -DJIT -DX86_64_ASSEMBLY -DOPTIMIZED_FLAGS -DWINUAE_ARANYM -DUSE_JIT_FPU -DUSE_INLINING -DDATADIR=\\\".\\\"")

# set_property(SOURCE compemu_support.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -O0 ")

target_link_libraries(BasiliskII ${COREFOUNDATION_LIBRARY} ${IOKIT_LIBRARY} ${SDL2_LIBRARIES})

# set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pagezero_size 0x2000" )

add_definitions(-march=native)


#keycodes -> ../SDL/keycodes
