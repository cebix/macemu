/*
 *  SaveROM - Save Mac ROM to file
 *
 *  Copyright (C) 1998-2004 Christian Bauer
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

#include <AppKit.h>
#include <InterfaceKit.h>
#include <StorageKit.h>

#include <stdio.h>
#include <unistd.h>


// Constants
const char APP_SIGNATURE[] = "application/x-vnd.cebix-SaveROM";
const char ROM_FILE_NAME[] = "ROM";

// Global variables
static uint8 buf[0x400000];

// Application object
class SaveROM : public BApplication {
public:
	SaveROM() : BApplication(APP_SIGNATURE)
	{
		// Find application directory and cwd to it
		app_info the_info;
		GetAppInfo(&the_info);
		BEntry the_file(&the_info.ref);
		BEntry the_dir;
		the_file.GetParent(&the_dir);
		BPath the_path;
		the_dir.GetPath(&the_path);
		chdir(the_path.Path());
	}
	virtual void ReadyToRun(void);
};


/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
{	
	SaveROM *the_app = new SaveROM();
	the_app->Run();
	delete the_app;
	return 0;
}


/*
 *  Display error alert
 */

static void ErrorAlert(const char *text)
{
	BAlert *alert = new BAlert("SaveROM Error", text, "Quit", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
}


/*
 *  Display OK alert
 */

static void InfoAlert(const char *text)
{
	BAlert *alert = new BAlert("SaveROM Message", text, "Quit", NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
	alert->Go();
}


/*
 *  Main program
 */

void SaveROM::ReadyToRun(void)
{
	int fd = open("/dev/sheep", 0);
	if (fd < 0) {
		ErrorAlert("Cannot open '/dev/sheep'.");
		goto done;
	}

	if (read(fd, buf, 0x400000) != 0x400000) {
		ErrorAlert("Cannot read ROM.");
		close(fd);
		goto done;
	}

	FILE *f = fopen(ROM_FILE_NAME, "wb");
	if (f == NULL) {
		ErrorAlert("Cannot open ROM file.");
		close(fd);
		goto done;
	}

	if (fwrite(buf, 1, 0x400000, f) != 0x400000) {
		ErrorAlert("Cannot write ROM.");
		fclose(f);
		close(fd);
		goto done;
	}

	InfoAlert("ROM saved.");

	fclose(f);
	close(fd);
done:
	PostMessage(B_QUIT_REQUESTED);
}
