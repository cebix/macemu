/*
 *  main_unix.cpp - Startup code for Unix
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

#include "sysdeps.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "cpu_emulation.h"
#include "sys.h"
#include "rom_patches.h"
#include "xpram.h"
#include "timer.h"
#include "video.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "macos_util.h"
#include "user_strings.h"
#include "version.h"
#include "main.h"

#define DEBUG 0
#include "debug.h"


#include <X11/Xlib.h>

#if ENABLE_GTK
#include <gtk/gtk.h>
#endif

#if ENABLE_XF86_DGA
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86dga.h>
#endif

#if ENABLE_MON
#include "mon.h"
#endif


// Constants
const char ROM_FILE_NAME[] = "ROM";


// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;


// Global variables
static char *x_display_name = NULL;					// X11 display name
Display *x_display = NULL;							// X11 display handle

static bool xpram_thread_active = false;			// Flag: XPRAM watchdog installed
static volatile bool xpram_thread_cancel = false;	// Flag: Cancel XPRAM thread
static pthread_t xpram_thread;						// XPRAM watchdog

static bool tick_thread_active = false;				// Flag: 60Hz thread installed
static volatile bool tick_thread_cancel = false;	// Flag: Cancel 60Hz thread
static pthread_t tick_thread;						// 60Hz thread
static pthread_attr_t tick_thread_attr;				// 60Hz thread attributes

static pthread_mutex_t intflag_lock = PTHREAD_MUTEX_INITIALIZER;	// Mutex to protect InterruptFlags

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
#define SIG_TIMER SIGRTMIN
static struct sigaction timer_sa;					// sigaction used for timer
static timer_t timer;								// 60Hz timer
#endif

#if ENABLE_MON
static struct sigaction sigint_sa;					// sigaction for SIGINT handler
static void sigint_handler(...);
#endif


// Prototypes
static void *xpram_func(void *arg);
static void *tick_func(void *arg);
static void one_tick(...);


/*
 *  Ersatz functions
 */

extern "C" {

#ifndef HAVE_STRDUP
char *strdup(const char *s)
{
	char *n = (char *)malloc(strlen(s) + 1);
	strcpy(n, s);
	return n;
}
#endif

}


/*
 *  Main program
 */

int main(int argc, char **argv)
{
	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	srand(time(NULL));
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Parse arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && ++i < argc)
			x_display_name = argv[i];
		else if (strcmp(argv[i], "-break") == 0 && ++i < argc)
			ROMBreakpoint = strtol(argv[i], NULL, 0);
		else if (strcmp(argv[i], "-rominfo") == 0)
			PrintROMInfo = true;
	}

	// Open display
	x_display = XOpenDisplay(x_display_name);
	if (x_display == NULL) {
		char str[256];
		sprintf(str, GetString(STR_NO_XSERVER_ERR), XDisplayName(x_display_name));
		ErrorAlert(str);
		QuitEmulator();
	}

#if ENABLE_XF86_DGA && !ENABLE_MON
	// Fork out, so we can return from fullscreen mode when things get ugly
	XF86DGAForkApp(DefaultScreen(x_display));
#endif

#if ENABLE_GTK
	// Init GTK
	gtk_set_locale();
	gtk_init(&argc, &argv);
#endif

	// Read preferences
	PrefsInit();

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!PrefsFindBool("nogui"))
		if (!PrefsEditor())
			QuitEmulator();

	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize") & 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 1024*1024;
	}
	RAMBaseHost = new uint8[RAMSize];

	// Create area for Mac ROM
	ROMBaseHost = new uint8[0x100000];

	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Load Mac ROM
	int rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
		QuitEmulator();
	}
	printf(GetString(STR_READING_ROM_FILE));
	ROMSize = lseek(rom_fd, 0, SEEK_END);
	if (ROMSize != 64*1024 && ROMSize != 128*1024 && ROMSize != 256*1024 && ROMSize != 512*1024 && ROMSize != 1024*1024) {
		ErrorAlert(GetString(STR_ROM_SIZE_ERR));
		close(rom_fd);
		QuitEmulator();
	}
	lseek(rom_fd, 0, SEEK_SET);
	if (read(rom_fd, ROMBaseHost, ROMSize) != (ssize_t)ROMSize) {
		ErrorAlert(GetString(STR_ROM_FILE_READ_ERR));
		close(rom_fd);
		QuitEmulator();
	}

	// Initialize everything
	if (!InitAll())
		QuitEmulator();

	// Start XPRAM watchdog thread
	xpram_thread_active = (pthread_create(&xpram_thread, NULL, xpram_func, NULL) == 0);

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
	// Start 60Hz timer
	sigemptyset(&timer_sa.sa_mask);
	timer_sa.sa_flags = SA_SIGINFO | SA_RESTART;
	timer_sa.sa_sigaction = one_tick;
	if (sigaction(SIG_TIMER, &timer_sa, NULL) < 0) {
		printf("FATAL: cannot set up timer signal handler\n");
		QuitEmulator();
	}
	struct sigevent timer_event;
	timer_event.sigev_notify = SIGEV_SIGNAL;
	timer_event.sigev_signo = SIG_TIMER;
	if (timer_create(CLOCK_REALTIME, &timer_event, &timer) < 0) {
		printf("FATAL: cannot create timer\n");
		QuitEmulator();
	}
	struct itimerspec req;
	req.it_value.tv_sec = 0;
	req.it_value.tv_nsec = 16625000;
	req.it_interval.tv_sec = 0;
	req.it_interval.tv_nsec = 16625000;
	if (timer_settime(timer, TIMER_RELTIME, &req, NULL) < 0) {
		printf("FATAL: cannot start timer\n");
		QuitEmulator();
	}

#else

	// Start 60Hz thread
	pthread_attr_init(&tick_thread_attr);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
	if (geteuid() == 0) {
		pthread_attr_setinheritsched(&tick_thread_attr, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&tick_thread_attr, SCHED_FIFO);
		struct sched_param fifo_param;
		fifo_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + sched_get_priority_max(SCHED_FIFO)) / 2;
		pthread_attr_setschedparam(&tick_thread_attr, &fifo_param);
	}
#endif
	tick_thread_active = (pthread_create(&tick_thread, &tick_thread_attr, tick_func, NULL) == 0);
	if (!tick_thread_active) {
		printf("FATAL: cannot create tick thread\n");
		QuitEmulator();
	}
#endif

#if ENABLE_MON
	// Setup SIGINT handler to enter mon
	sigemptyset(&sigint_sa.sa_mask);
	sigint_sa.sa_flags = 0;
	sigint_sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sigint_sa, NULL);
#endif

	// Start 68k and jump to ROM boot routine
	Start680x0();

	QuitEmulator();
	return 0;
}


/*
 *  Quit emulator
 */

void QuitEmulator(void)
{
	// Exit 680x0 emulation
	Exit680x0();

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
	// Stop 60Hz timer
	timer_delete(timer);
#else
	// Stop 60Hz thread
	if (tick_thread_active) {
		tick_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(tick_thread);
#endif
		pthread_join(tick_thread, NULL);
	}
#endif

	// Stop XPRAM watchdog thread
	if (xpram_thread_active) {
		xpram_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(xpram_thread);
#endif
		pthread_join(xpram_thread, NULL);
	}

	// Deinitialize everything
	ExitAll();

	// Delete ROM area
	delete[] ROMBaseHost;

	// Delete RAM area
	delete[] RAMBaseHost;

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

	// Close X11 server connection
	if (x_display)
		XCloseDisplay(x_display);

	exit(0);
}


/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

#if EMULATED_68K
void FlushCodeCache(void *start, uint32 size)
{
}
#endif


/*
 *  SIGINT handler, enters mon
 */

#if ENABLE_MON
static void sigint_handler(...)
{
	char *arg[2] = {"rmon", NULL};
	mon(1, arg);
	QuitEmulator();
}
#endif


/*
 *  Interrupt flags (must be handled atomically!)
 */

uint32 InterruptFlags = 0;

void SetInterruptFlag(uint32 flag)
{
	pthread_mutex_lock(&intflag_lock);
	InterruptFlags |= flag;
	pthread_mutex_unlock(&intflag_lock);
}

void ClearInterruptFlag(uint32 flag)
{
	pthread_mutex_lock(&intflag_lock);
	InterruptFlags &= ~flag;
	pthread_mutex_unlock(&intflag_lock);
}


/*
 *  60Hz thread (really 60.15Hz)
 */

static void one_tick(...)
{
	static int tick_counter = 0;

	// Pseudo Mac 1Hz interrupt, update local time
	if (++tick_counter > 60) {
		tick_counter = 0;
		WriteMacInt32(0x20c, TimerDateTime());
	}

	// Trigger 60Hz interrupt
	if (ROMVersion != ROM_VERSION_CLASSIC || HasMacStarted()) {
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}
}

static void *tick_func(void *arg)
{
	while (!tick_thread_cancel) {

		// Wait
#ifdef HAVE_NANOSLEEP
		struct timespec req = {0, 16625000};
		nanosleep(&req, NULL);
#else
		usleep(16625);
#endif

		// Action
		one_tick();
	}
	return NULL;
}


/*
 *  XPRAM watchdog thread (saves XPRAM every minute)
 */

void *xpram_func(void *arg)
{
	uint8 last_xpram[256];
	memcpy(last_xpram, XPRAM, 256);

	while (!xpram_thread_cancel) {
		for (int i=0; i<60 && !xpram_thread_cancel; i++) {
#ifdef HAVE_NANOSLEEP
			struct timespec req = {1, 0};
			nanosleep(&req, NULL);
#else
			usleep(1000000);
#endif
		}
		if (memcmp(last_xpram, XPRAM, 256)) {
			memcpy(last_xpram, XPRAM, 256);
			SaveXPRAM();
		}
	}
	return NULL;
}


/*
 *  Display alert
 */

#if ENABLE_GTK
static void dl_destroyed(void)
{
	gtk_main_quit();
}

static void dl_quit(GtkWidget *dialog)
{
	gtk_widget_destroy(dialog);
}

void display_alert(int title_id, int prefix_id, int button_id, const char *text)
{
	char str[256];
	sprintf(str, GetString(prefix_id), text);

	GtkWidget *dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), GetString(title_id));
	gtk_container_border_width(GTK_CONTAINER(dialog), 5);
	gtk_widget_set_uposition(GTK_WIDGET(dialog), 100, 150);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy", GTK_SIGNAL_FUNC(dl_destroyed), NULL);

	GtkWidget *label = gtk_label_new(str);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, TRUE, 0);

	GtkWidget *button = gtk_button_new_with_label(GetString(button_id));
	gtk_widget_show(button);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(dl_quit), GTK_OBJECT(dialog));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), button, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(dialog);

	gtk_main();
}
#endif


/*
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
#if ENABLE_GTK
	if (PrefsFindBool("nogui") || x_display == NULL) {
		printf(GetString(STR_SHELL_ERROR_PREFIX), text);
		return;
	}
	VideoQuitFullScreen();
	display_alert(STR_ERROR_ALERT_TITLE, STR_GUI_ERROR_PREFIX, STR_QUIT_BUTTON, text);
#else
	printf(GetString(STR_SHELL_ERROR_PREFIX), text);
#endif
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
{
#if ENABLE_GTK
	if (PrefsFindBool("nogui") || x_display == NULL) {
		printf(GetString(STR_SHELL_WARNING_PREFIX), text);
		return;
	}
	display_alert(STR_WARNING_ALERT_TITLE, STR_GUI_WARNING_PREFIX, STR_OK_BUTTON, text);
#else
	printf(GetString(STR_SHELL_WARNING_PREFIX), text);
#endif
}


/*
 *  Display choice alert
 */

bool ChoiceAlert(const char *text, const char *pos, const char *neg)
{
	printf(GetString(STR_SHELL_WARNING_PREFIX), text);
	return false;	//!!
}
