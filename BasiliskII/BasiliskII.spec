Summary:   A free, portable Mac II emulator
Name:      BasiliskII
Version:   0.7
Release:   2
URL:       http://www.uni-mainz.de/~bauec002/B2Main.html
Source:    BasiliskII_src_250799.tar.gz
Copyright: GPL
Group:     Applications/Emulators
Vendor: PLD
Packager: Christian Bauer <Christian.Bauer@uni-mainz.de>
BuildRoot: /tmp/%{name}-%{version}-root

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
  - Ethernet driver
  - Serial drivers
  - SCSI Manager (old-style) emulation
  - Emulates extended ADB keyboard and 3-button mouse
  - Uses UAE 68k emulation or (under AmigaOS) real 68k processor

%prep
%setup -q

%build
cd src/Unix
./configure --prefix=/usr

make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT

install -d $RPM_BUILD_ROOT/usr/lib/BasiliskII/Linux
install -d $RPM_BUILD_ROOT/usr/X11R6/bin

install -m755 -s src/Unix/BasiliskII $RPM_BUILD_ROOT/usr/X11R6/bin

cp -R src/Unix/Linux/* $RPM_BUILD_ROOT/usr/lib/BasiliskII/Linux
mkdir docs
cp CHANGES COPYING README TECH TODO docs

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc docs/*
/usr/lib/BasiliskII
/usr/X11R6/bin/*

%changelog
* Fri Jul 23 1999 Roman Niewiarowski <newrom@pasjo.net.pl>
  [0.6-1]
- First rpm release
