# Note that this is NOT a relocatable package
%define ver 0.8
%define rel 1
%define prefix /usr

Summary: A free, portable Mac II emulator
Name: BasiliskII
Version: %ver
Release: %rel
Copyright: GPL
Group: Applications/Emulators
Source: BasiliskII_src_30012000.tar.gz
BuildRoot: /tmp/BasiliskII-%{ver}-root
Packager:  Christian Bauer <Christian.Bauer@uni-mainz.de>
URL: http://www.uni-mainz.de/~bauec002/B2Main.html
Docdir: %{prefix}/doc

%description
Basilisk II is a free, portable, Open Source 68k Mac emulator. It requires
a copy of a Mac ROM and a copy of MacOS to run. Basilisk II is freeware and
distributed under the GNU General Public License.

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
%setup

%build
cd src/Unix
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT
cd src/Unix
make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%doc ChangeLog COPYING INSTALL README TECH TODO
/usr/bin/BasiliskII
/usr/man/man1/BasiliskII.1
/usr/share/BasiliskII/keycodes
/usr/share/BasiliskII/fbdevices
