# Microsoft Developer Studio Project File - Name="b2ether" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=b2ether - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "b2ether.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "b2ether.mak" CFG="b2ether - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "b2ether - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "b2ether - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "b2ether - Win32 Release"

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f b2ether.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "b2ether.sys"
# PROP BASE Bsc_Name "b2ether.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "ddkbuild free D:\BasiliskII\src\Windows\b2ether\driver"
# PROP Rebuild_Opt "-cf"
# PROP Target_File "b2ether.sys"
# PROP Bsc_Name "b2ether.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "b2ether - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f b2ether.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "b2ether.sys"
# PROP BASE Bsc_Name "b2ether.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "ddkbuild checked D:\BasiliskII\src\Windows\b2ether\driver"
# PROP Rebuild_Opt "-cf"
# PROP Target_File "b2ether.sys"
# PROP Bsc_Name "b2ether.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "b2ether - Win32 Release"
# Name "b2ether - Win32 Debug"

!IF  "$(CFG)" == "b2ether - Win32 Release"

!ELSEIF  "$(CFG)" == "b2ether - Win32 Debug"

!ENDIF 

# Begin Source File

SOURCE=.\b2ether.c
# End Source File
# Begin Source File

SOURCE=.\b2ether.h
# End Source File
# Begin Source File

SOURCE=.\b2ether_openclose.c
# End Source File
# Begin Source File

SOURCE=.\b2ether_read.c
# End Source File
# Begin Source File

SOURCE=.\b2ether_write.c
# End Source File
# Begin Source File

SOURCE=..\OEMSETUP.INF
# End Source File
# Begin Source File

SOURCE=.\SOURCES
# End Source File
# End Target
# End Project
