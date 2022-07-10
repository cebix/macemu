# Basilisk II and SheepShaver

This repository contains the **Basilisk II** and **SheepShaver** projects.

Releases are made available by the https://emaculation.com/ community.

Note: For a more up-to-date fork, check out https://github.com/kanjitalk755/macemu/.

# What is Basilisk II?

Basilisk II is an Open Source 68k Macintosh emulator. That is, it allows you to run 68k MacOS software on your computer, even if you are using a different operating system. However, you still need a copy of MacOS and a Macintosh ROM image to use Basilisk II. Basilisk II is distributed under the terms of the GNU General Public License (GPL).

For more information, see the README file. If you are interested in learning how Basilisk II works internally, there is a Technical Manual available (knowledge about programming and computer architecture is required).

# Available ports

Basilisk II has been ported to the following systems:
 * Unix with X11 (Linux i386/x86_64, Solaris 2.5, FreeBSD 3.x, IRIX 6.5)
 * Mac OS X (PowerPC, Intel and Apple silicon)
 * Windows NT/2000/XP
 * BeOS R4 (PowerPC and Intel)
 * AmigaOS 3.x
 
# Some features of Basilisk II

 * Emulates either a Mac Classic (which runs MacOS 0.x thru 7.5) or a Mac II series machine (which runs MacOS 7.x, 8.0 and 8.1), depending on the ROM being used
 * Color video display
 * CD quality sound output
 * Floppy disk driver (only 1.44MB disks supported)
 * Driver for HFS partitions and hardfiles
 * CD-ROM driver with basic audio functions
 * Easy file exchange with the host OS via a "Host Directory Tree" icon on the Mac desktop
 * Ethernet driver
 * Serial drivers
 * SCSI Manager (old-style) emulation
 * Emulates extended ADB keyboard and 3-button mouse
 * Uses UAE 68k emulation or (under AmigaOS and NetBSD/m68k) real 68k processor

# What is SheepShaver?

SheepShaver is a Mac OS run-time environment that allows you to run classic PowerPC Mac OS applications on a different operating system, such as Mac OS X, Windows, Linux or BeOS. If you are using a PowerPC-based system, applications will run at native speed (i.e. with no emulation involved). There is also a built-in PowerPC emulator for non-PowerPC systems.

SheepShaver is distributed under the terms of the GNU General Public License (GPL). However, you still need a copy of MacOS and a PowerMac ROM image to use SheepShaver. If you're planning to run SheepShaver on a PowerMac, you probably already have these two items.

# Supported systems

SheepShaver runs with varying degree of functionality on the following systems:

 * Unix with X11 (Linux i386/x86_64/ppc, NetBSD 2.x, FreeBSD 3.x)
 * Mac OS X (PowerPC and Intel)
 * Windows NT/2000/XP
 * BeOS R4/R5 (PowerPC)

# Some of SheepShaver's features
 * Runs MacOS 7.5.2 thru 9.0.4. MacOS X as a guest is not supported.
 * Color video display
 * CD quality sound output
 * Access to floppy disks, CD-ROMs and HFS(+) partitions on hard disks
 * Easy file exchange with the host OS via a "Host Directory Tree" icon on the Mac desktop
 * Internet and LAN networking via Ethernet
 * Serial drivers
SCSI Manager (old-style) emulation
