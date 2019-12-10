#### BasiliskII
```
macOS 64-bit ---
Linux 32-bit JIT
MinGW 32-bit JIT
```
#### SheepShaver
```
macOS 64-bit JIT
Linux 32-bit JIT
MinGW 32-bit ---
```
### How To Build
These builds need to be installed SDL2.0.10+ framework/library.
#### BasiliskII
##### macOS
1. Open BasiliskII/src/MacOSX/BasiliskII.xcodeproj
1. Set Build Configuration to Release
1. If building with Xcode10+, set File -> Project Settings -> Build System to Legacy Build System.
1. Build

##### Linux(x86)
```
$ cd macemu/BasiliskII/src/Unix
$ ./autogen.sh
$ make
```
##### MinGW32/MSYS
```
$ cd macemu/BasiliskII/src/Windows
$ ../Unix/autogen.sh
$ make
```
#### SheepShaver
##### macOS
1. Open SheepShaver/src/MacOSX/SheepShaver_Xcode8.xcodeproj
1. Set Build Configuration to Release
1. Build

##### Linux(x86)
```
$ cd macemu/SheepShaver
$ make links
$ cd src/Unix
$ ./autogen.sh
$ make
```
##### MinGW32/MSYS
```
$ cd macemu/SheepShaver
$ make links
$ cd src/Windows
$ ../Unix/autogen.sh
$ make
```
