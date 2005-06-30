%define name BasiliskII
%define version 1.0rc1
%define release 1

Summary: 68k Macintosh emulator
Name: %{name}
Version: %{version}
Release: %{release}
License: GPL
Group: Applications/Emulators
Source0: %{name}_src_30062005.tar.gz
URL: http://www.uni-mainz.de/~bauec002/B2Main.html
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# While the data file path (/usr/share/BasiliskII) is compiled into the
# executable, the data files are not required for operation and their location
# can be overridden with prefs items, so I consider this package to be
# relocatable.
Prefix: %{_prefix}

%description
Basilisk II is an Open Source 68k Macintosh emulator. That is, it enables
you to run 68k MacOS software on you computer, even if you are using a
different operating system. However, you still need a copy of MacOS and
a Macintosh ROM image to use Basilisk II.

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
  - Uses UAE 68k emulation or (under AmigaOS and NetBSD/m68k) real 68k
    processor

%prep
%setup -q

%build
cd src/Unix
%configure
make

%install
rm -rf ${RPM_BUILD_ROOT}
cd src/Unix
%makeinstall

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc ChangeLog COPYING INSTALL README TECH TODO
%{_bindir}/BasiliskII
%{_mandir}/man1/BasiliskII.1*
%config %{_datadir}/BasiliskII/keycodes
%config %{_datadir}/BasiliskII/fbdevices
