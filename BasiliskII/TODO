Bugs:
- System 7.1 with Quadra900 ModelID (1MB ROM): 0x108 gets strange value
- Apple Personal Diagnostics doesn't work with more than 1023 MB

General:
- Add support for 2MB ROMs (Quadra 840AV)
- Add support for System 6.0.x
- Sony: rdVerify, Tag Buffer
- Disk: rdVerify
- CD-ROM: track lists, positioning type 3, TOC type 4/5, ReadHeader/ReadMCN/
  ReadISRC/ReadAudio/ReadAllSubcodes
- Sound in
- Video: multiple monitor support
- More accurate Time Manager
- Serial driver: XOn/XOff handshaking
- Classic ROM: mouse button/movement is broken with ROM mouse handler
- Classic ROM: sound output
- Write a nice User's Manual with linuxdoc or something similar
- Fix video mode switch to cope with different mac_frame_base
  (CrsrBase is overriden with the previous base after the mode switch)

AmigaOS:
- "Create Hardfile..." button
- Support for ShapeShifter External Video Drivers
- clip_amiga.cpp: clip AmigaOS->Basilisk
- sys_amiga.cpp: MaxTransfer/BufMemType/TransferMask, SysAddCDROMPrefs(),
  SysFormat()
- Patch 512K ROM for 68040/060 caches
- Input handler instead of IDCMP?
- Last sound buffer is not played
- Sound output rate/bits/channels switching

BeOS:
- clip_beos.cpp: clip BeOS->Basilisk
- Last sound buffer is not played
- Sound output rate/bits/channels switching
- Video depth/resolution switching

Unix:
- sys_unix.cpp: SysFormat(), SysIsFixedDisk(), SysIsDiskInserted(),
  prevent/allow for non-floppy/CDROM devices
- ESD is also available on Solaris
- display progress bar during disk file creation in prefs editor

Mac OS X:
- Sound
- Cut and paste
- Lots of other stuff. See src/MacOSX/ToDo.html

Windows
- main_windows.cpp: undo the SDL/DIB driver trick
- video_windows.cpp: implement with DirectX
- audio_windows.cpp, scsi_windows.cpp: merge from original Windows version
