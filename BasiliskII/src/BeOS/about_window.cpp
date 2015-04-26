/*
 *  about_window.cpp - "About" window
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#include <InterfaceKit.h>
#include <stdio.h>

#include "sysdeps.h"
#include "version.h"
#include "user_strings.h"

/*
 *  Display "About" window
 */

void ShowAboutWindow(void)
{
	char str[512];
	sprintf(str,
		"Basilisk II\nVersion %d.%d\n\n"
		"Copyright " B_UTF8_COPYRIGHT " 1997-2008 Christian Bauer et al.\n"
		"E-mail: Christian.Bauer@uni-mainz.de\n"
		"http://www.uni-mainz.de/~bauec002/B2Main.html\n\n"
		"Basilisk II comes with ABSOLUTELY NO\n"
		"WARRANTY. This is free software, and\n"
		"you are welcome to redistribute it\n"
		"under the terms of the GNU General\n"
		"Public License.\n",
		VERSION_MAJOR, VERSION_MINOR
	);
	BAlert *about = new BAlert("", str, GetString(STR_OK_BUTTON), NULL, NULL, B_WIDTH_FROM_LABEL);
	BTextView *theText = about->TextView();
	if (theText) {
		theText->SetStylable(true);
		theText->Select(0, 11);
		BFont ourFont;
		theText->SetFontAndColor(be_bold_font);
		theText->GetFontAndColor(2, &ourFont, NULL);
		ourFont.SetSize(24);
		theText->SetFontAndColor(&ourFont);
	}
	about->Go();
}
