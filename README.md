#### BasiliskII
```
macOS 64-bit JIT
Linux 32-bit JIT
MinGW 32-bit JIT
```
#### SheepShaver
```
macOS 64-bit JIT
Linux 32-bit JIT
MinGW 32-bit JIT
```
### How To Build
These builds need to be installed SDL2.0.10+ framework/library.
#### BasiliskII
##### macOS
1. Open BasiliskII/src/MacOSX/BasiliskII.xcodeproj
1. Set Build Configuration to Release
1. Build

(or same as Linux)

##### Linux(x86)
```
$ cd macemu/BasiliskII/src/Unix
$ ./autogen.sh
$ make
```
##### MinGW32/MSYS2
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

(or same as Linux)

##### Linux(x86)
```
$ cd macemu/SheepShaver/src/Unix
$ ./autogen.sh
$ make
```
##### MinGW32/MSYS2
```
$ cd macemu/SheepShaver
$ make links
$ cd src/Windows
$ ../Unix/autogen.sh
$ make
```
### Recommended key bindings for gnome
https://github.com/kanjitalk755/macemu/blob/master/SheepShaver/doc/Linux/gnome_keybindings.txt

(from https://github.com/kanjitalk755/macemu/issues/59)
