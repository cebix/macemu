%define name BasiliskII
%define version 0.8
%define release 1

Summary: Free, portable 68k Macintosh emulator
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Applications/Emulators
Source: %{name}_src_05102000.tar.gz
URL: http://www.uni-mainz.de/~bauec002/B2Main.html
BuildRoot: %{_tmppath}/%{name}-root
Prefix: %{_prefix}

%description
Basilisk II is a free, portable, Open Source 68k Macintosh emulator. It
requires a copy of a Mac ROM and a copy of MacOS to run. Basilisk II is
freeware and distributed under the GNU General Public License.

Some features of Basilisk II:
  - Emulates either a Mac Classic (which runs MacOS 0.x thru 7.5)
    or a Mac II series machine (which runs MacOS 7.x, 8.0 and 8.1),
    depending on the ROM being used
  - Color video display
  - CD quality sound output
  - Floppy disk driver (only 1.44MB disks supported)
  - Driver for HFS partitions and hardfiles
  - CD-ROM driver with basic audio functions
  - Easy file exchange with the host OS via a "Host Directory Tree" icon
    on the Mac desktop
  - Ethernet driver
  - Serial drivers
  - SCSI Manager (old-style) emulation
  - Emulates extended ADB keyboard and 3-button mouse
  - Uses UAE 68k emulation or (under AmigaOS) real 68k processor

%prep
%setup -q

%build
cd src/Unix
CFLAGS=${RPM_OPT_FLAGS} CXXFLAGS=${RPM_OPT_FLAGS} ./configure --prefix=%{_prefix} --mandir=%{_mandir}
if [ -x /usr/bin/getconf ] ; then
  NCPU=$(/usr/bin/getconf _NPROCESSORS_ONLN)
  if [ $NCPU -eq 0 ] ; then
    NCPU=1
  fi
else  
  NCPU=1
fi
PARL=$[ $NCPU + 1 ]
make -j $PARL

%install
rm -rf ${RPM_BUILD_ROOT}
cd src/Unix
make DESTDIR=${RPM_BUILD_ROOT} install

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc ChangeLog COPYING INSTALL README TECH TODO
%{_bindir}/BasiliskII
%{_mandir}/man1/BasiliskII.1
%{_datadir}/BasiliskII/keycodes
%{_datadir}/BasiliskII/fbdevices
