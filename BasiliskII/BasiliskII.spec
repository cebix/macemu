Summary:   A free, portable Mac II emulator
Name:      BasiliskII
Version:   0.7
Release:   1
URL:       http://www.uni-mainz.de/~bauec002/B2Main.html
Source:    BasiliskII_src_03101999.tar.gz
Copyright: GPL
Group:     Applications/Emulators
Packager:  Christian Bauer <Christian.Bauer@uni-mainz.de>

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
./configure --prefix=/usr --exec_prefix=/usr/X11R6
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
cd src/Unix
make install

%files
%doc ChangeLog COPYING README TECH TODO
/usr/X11R6/bin/BasiliskII
/usr/man/man1/BasiliskII.1
/usr/share/BasiliskII/keycodes

%changelog
* Fri Jul 23 1999 Roman Niewiarowski <newrom@pasjo.net.pl>
  [0.6-1]
- First rpm release
