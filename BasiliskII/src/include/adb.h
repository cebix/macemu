/*
 *  adb.h - ADB emulation (mouse/keyboard)
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ADB_H
#define ADB_H

extern void ADBInit(void);
extern void ADBExit(void);

extern void ADBOp(uint8 op, uint8 *data);

extern void ADBMouseMoved(int x, int y);
extern void ADBMouseDown(int button);
extern void ADBMouseUp(int button);

extern void ADBKeyDown(int code);
extern void ADBKeyUp(int code);

extern void ADBInterrupt(void);

extern void ADBSetRelMouseMode(bool relative);

#endif
