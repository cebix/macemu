/* prefs_editor_gtk3.cpp
 *
 * SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sysdeps.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>
#include <net/if_arp.h>

#include "user_strings.h"
#include "version.h"
#include "cdrom.h"
#include "xpram.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "g_resource.h"

#if defined(USE_SDL_AUDIO) || defined(USE_SDL_VIDEO)
#include <SDL.h>
#endif

#define DEBUG 0
#include "debug.h"

#if GTK_CHECK_VERSION(3, 20, 0)
#define USE_NATIVE_FILE_CHOOSER 1
#endif

// Global variables
static GtkBuilder *builder;
static GtkWidget *win;				// Preferences window
static bool start_clicked = false;	// Return value of PrefsEditor() function
static int screen_width, screen_height; // Screen dimensions

static GtkAdjustment *size_adj;

static GtkToggleButton *screen_full;
static GtkToggleButton *screen_win;
static GtkComboBox *screen_res;

static GtkEntry *mag_rate;

static GtkWidget *volumes_view;
static GtkTreeModel *volume_store;
static GtkTreeIter toplevel;
static GtkTreeSelection *selection;

static int display_type;
static int dis_width, dis_height;
#ifdef ENABLE_FBDEV_DGA
static bool is_fbdev_dga_mode = false;
#endif

struct opt_desc {
	int label_id;
	GCallback func;
};

struct combo_desc {
	int label_id;
};

static GtkWidget *create_tree_view (void);
static void cb_add_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void cb_create_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void cb_remove_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static GList *add_serial_names(void);
static GList *add_ether_names(void);
static void save_volumes(void);
static void get_graphics_settings(void);
static void get_mouse_wheel_mode(void);
static bool is_jit_capable(void);

#ifdef SHEEPSHAVER
#define VOLUME_SIZE_DEFAULT "256"
#define G_RES_PATH "/net/cebix/SheepShaver/ui/"
#define ABOUT_COPYRIGHT "© 1997-2008 Christian Bauer and Marc Hellwig"
const char *authors[] = {"Christian Bauer", "Marc Hellwig", "Gwenolé Beauchesne", NULL};
#else
#define VOLUME_SIZE_DEFAULT "64"
#define G_RES_PATH "/net/cebix/BasiliskII/ui/"
#define ABOUT_COPYRIGHT "© 1997-2008 Christian Bauer et al."
const char *authors[] = {
	"Christian Bauer", "Orlando Bassotto", "Gwenolé Beauchesne", "Marc Chabanas", "Marc Hellwig",
	"Bill Huey", "Brian J. Johnson", "Jürgen Lachmann", "Samuel Lander", "David Lawrence",
	"Lauri Pesonen", "Bernd Schmidt", "and others", NULL };
#endif

#if REAL_ADDRESSING
#define ABOUT_MODE "Addressing mode: Real"
#else
#define ABOUT_MODE "Addressing mode: Direct"
#endif

#if defined(USE_SDL_AUDIO) && defined(USE_SDL_VIDEO)
#if SDL_VERSION_ATLEAST(2,0,0)
#define ABOUT_VIDEO "SDL 2 audio"
#else
#define ABOUT_VIDEO "SDL 1.2 audio"
#endif
#define ABOUT_AUDIO "video"
#else
#ifdef USE_SDL_AUDIO
#if SDL_VERSION_ATLEAST(2,0,0)
#define ABOUT_AUDIO "SDL 2 audio"
#else
#define ABOUT_AUDIO "SDL 1.2 audio"
#endif
#elif defined(ESD_AUDIO)
#define ABOUT_AUDIO "ESD audio"
#else
#define ABOUT_AUDIO "no audio"
#endif
#ifdef USE_SDL_VIDEO
#if SDL_VERSION_ATLEAST(2,0,0)
#define ABOUT_VIDEO "SDL 2 video"
#else
#define ABOUT_VIDEO "SDL 1.2 video"
#endif
#else
#define ABOUT_VIDEO "X11 video"
#endif
#endif

const char *sysinfo = ABOUT_MODE "\nBuilt with " ABOUT_VIDEO " and " ABOUT_AUDIO "\n" ABOUT_COPYRIGHT;

/*
 *  Utility functions
 */

// The widgets from prefs-editor.ui that need their values set on launch
const char *check_boxes[] = {
	"udptunnel", "keycodes", "ignoresegv", "idlewait", "jit", "jitfpu", "jitinline",
	"jitlazyflush", "jit68k", "gfxaccel", "swap_opt_cmd", "scale_nearest", "scale_integer", NULL };
const char *inv_check_boxes[] = { "nocdrom", "nosound", "nogui", NULL };
const char *entries[] = {
	"extfs", "dsp", "mixer", "keycodefile", "scsi0", "scsi1", "scsi2", "scsi3", "scsi4",
	"scsi5", "scsi6", "rom", NULL };
const char *spin_buttons[] = { "mousewheellines", "udpport", NULL };
const char *id_combos[] = { "bootdriver", "frameskip", "modelid", NULL };
const char *text_combos[] = { "ramsize", NULL };

// Set initial widget states
static void set_initial_prefs(void)
{
	const char **id = check_boxes;
	while (*id)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, *id)),
		                             PrefsFindBool(*id));
		id++;
	}

	id = inv_check_boxes;
	while(*id)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, *id)),
		                             !PrefsFindBool(*id));
		id++;
	}
	cb_swap_opt_cmd(NULL);

	id = entries;
	while (*id)
	{
		if (PrefsFindString(*id) != NULL)
			gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(builder, *id)),
			                   PrefsFindString(*id));
		id++;
	}

	id = spin_buttons;
	while (*id)
	{
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(gtk_builder_get_object(builder, *id)),
		                          PrefsFindInt32(*id));
		id++;
	}

	id = id_combos;
	while (*id)
	{
		char *str = g_strdup_printf("%d", PrefsFindInt32(*id));
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(gtk_builder_get_object(builder, *id)), str);
		id++;
		g_free(str);
	}

	GtkComboBoxText *combo;
	id = text_combos;
	while (*id)
	{
		const char *pref_str = PrefsFindString(*id);
		combo = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, *id));
		if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), pref_str))
		{
			gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), pref_str);
		}
		id++;
	}
}



// Helper functions to create a file chooser
static GtkFileChooser *file_chooser_new (GtkWidget *parent,
                                         GtkFileChooserAction action,
                                         uint32_t title_id,
                                         uint32_t accept_id,
                                         uint32_t cancel_id,
                                         const char *directory)
{
#ifdef USE_NATIVE_FILE_CHOOSER
	GtkFileChooserNative *chooser = gtk_file_chooser_native_new(GetString(title_id),
	                                                            GTK_WINDOW(parent),
	                                                            action,
	                                                            GetString(accept_id),
	                                                            GetString(cancel_id));
	gtk_native_dialog_set_transient_for(GTK_NATIVE_DIALOG(chooser), GTK_WINDOW(parent));
	gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(chooser), true);
#else
	GtkWidget *chooser = gtk_file_chooser_dialog_new(GetString(title_id),
	                                                 GTK_WINDOW(parent),
	                                                 action,
	                                                 GetString(cancel_id), GTK_RESPONSE_CANCEL,
	                                                 GetString(accept_id), GTK_RESPONSE_ACCEPT,
	                                                 NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
	gtk_window_set_transient_for(GTK_WINDOW(chooser), GTK_WINDOW(win));
	gtk_window_set_modal(GTK_WINDOW(chooser), true);
#endif
	if (directory)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), g_path_get_dirname(directory));
	return GTK_FILE_CHOOSER(chooser);
}

static void file_chooser_show(GtkFileChooser *chooser)
{
#ifdef USE_NATIVE_FILE_CHOOSER
	gtk_native_dialog_show(GTK_NATIVE_DIALOG(chooser));
#else
	gtk_widget_show(GTK_WIDGET(chooser));
#endif
}

static void file_chooser_destroy(GtkFileChooser *chooser)
{
#ifdef USE_NATIVE_FILE_CHOOSER
	gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(chooser));
#else
	gtk_widget_destroy(GTK_WIDGET(chooser));
#endif
	g_object_unref(G_OBJECT(chooser));
}


// User closed the file chooser dialog, possibly selecting a file
static void cb_browse_response(GtkFileChooser *chooser, int response, GtkEntry *entry)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		gtk_entry_set_text(GTK_ENTRY(entry), filename);
		g_free (filename);
	}
	file_chooser_destroy (chooser);
}

extern "C" {

// Open the file chooser dialog to select a file
void cb_browse(GtkWidget *button, GtkWidget *entry)
{
	GtkFileChooser *chooser = file_chooser_new(win,
	                                           GTK_FILE_CHOOSER_ACTION_OPEN,
	                                           STR_BROWSE_TITLE,
	                                           STR_SELECT_BUTTON,
	                                           STR_CANCEL_BUTTON,
	                                           gtk_entry_get_text(GTK_ENTRY(entry)));
	g_signal_connect(chooser, "response", G_CALLBACK(cb_browse_response), GTK_ENTRY(entry));
	file_chooser_show(chooser);
}

// Open the file chooser dialog to select a folder
void cb_browse_dir(GtkWidget *button, GtkWidget *entry)
{
	GtkFileChooser *chooser = file_chooser_new(win,
	                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                           STR_BROWSE_FOLDER_TITLE,
	                                           STR_SELECT_BUTTON,
	                                           STR_CANCEL_BUTTON,
	                                           gtk_entry_get_text(GTK_ENTRY(entry)));
	g_signal_connect(chooser, "response", G_CALLBACK(cb_browse_response), GTK_WIDGET(entry));
	file_chooser_show(chooser);
}

// User changed scaling settings
void cb_scaling(GtkWidget * widget)
{
	const char *mag_rate_str = gtk_entry_get_text(GTK_ENTRY(mag_rate));
	if (mag_rate_str) {
		PrefsReplaceString("mag_rate", mag_rate_str);
	} else {
		PrefsRemoveItem("mag_rate");
	}
}

// User changed one of the screen mode settings
void cb_screen_mode(GtkWidget *widget)
{
	const char *res = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(screen_res));
	if (res)
	{
		if (g_strcmp0(res, GetString(STR_SIZE_MAX_LAB)) == 0)
		{
			dis_width = 0;
			dis_height = 0;
		}
		else
			sscanf(res, "%d x %d", &dis_width, &dis_height);
	}

#ifdef ENABLE_FBDEV_DGA
	const char *str = gtk_toggle_button_get_active(screen_full) ? "fbdev/%d/%d" : "win/%d/%d";
#else
	const char *str = gtk_toggle_button_get_active(screen_full) ? "dga/%d/%d" : "win/%d/%d";
#endif
	char *screen = g_strdup_printf(str, dis_width, dis_height);
	PrefsReplaceString("screen", screen);
	// Old prefs are now migrated
	PrefsRemoveItem("windowmodes");
	PrefsRemoveItem("screenmodes");
	g_free(screen);
}

} // extern "C"

static void set_ramsize_combo_box(void)
{
	const char *name = "ramsize";
	int ramsize = PrefsFindInt32(name);
	int size_mb = (ramsize <= 1000) ? ramsize : ramsize >> 20;
	GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, name));
	char *id = g_strdup_printf("%d MB", size_mb);
	if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), id))
	{
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), id);
	}
	g_free(id);
}

static void set_jitcachesize_combo_box(void)
{
	const char *name = "jitcachesize";
	int size_mb = PrefsFindInt32(name) >> 10;
	GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, name));
	char *id = g_strdup_printf("%d MB", size_mb);
	if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), id))
	{
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), id);
	}
	g_free(id);
}

static void set_hotkey_buttons(void)
{
	GtkToggleButton *ctrl = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "ctrl-hotkey"));
	GtkToggleButton *alt = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "alt-hotkey"));
	GtkToggleButton *super = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "super-hotkey"));
	int32_t hotkey = PrefsFindInt32("hotkey");
	if (hotkey == 0)
		hotkey = 1;
	gtk_toggle_button_set_active(ctrl, hotkey & 1);
	gtk_toggle_button_set_active(alt, hotkey & 2);
	gtk_toggle_button_set_active(super, hotkey & 4);
}

static void set_cpu_combo_box (void)
{
	GtkComboBox *combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "cpu"));
	int32_t cpu = PrefsFindInt32("cpu");
	bool fpu = PrefsFindBool("fpu");
	char *id = g_strdup_printf("%d", (cpu << 1 | fpu));
	gtk_combo_box_set_active_id(combo, id);
	g_free(id);
}

static GtkWidget *add_combo_box_values(GList *entries, const char *pref)
{
	GtkWidget *combo = GTK_WIDGET(gtk_builder_get_object(builder, pref));
	const char *current = PrefsFindString(pref);
	GList *list;
	list = entries;
	while (list)
	{
		gtk_combo_box_text_prepend(GTK_COMBO_BOX_TEXT(combo), ((char *) list->data), ((char *) list->data));
		list = list->next;
	}

	if (pref != NULL && !gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), current))
	{
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), current);
	}
	return combo;
}

// Window closed
static gint window_closed(void)
{
	return FALSE;
}

// Window destroyed
static void window_destroyed(void)
{
	gtk_main_quit();
}

// Emulator is ready to start
static void run_emulator()
{
	start_clicked = true;
	save_volumes();
	SavePrefs();
	gtk_widget_destroy(win);
}
// "Start" button clicked
static void cb_start (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (access(PrefsFindString("rom"), F_OK) != 0)
	{
		cb_infobar_show(GTK_WIDGET(gtk_builder_get_object(builder, "rom-error-bar")));
		return;
	}
	else
		run_emulator();
}

// "Save Settings" menu item clicked
static void
cb_save_settings (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	save_volumes();
	SavePrefs();
}

// "Quit" button clicked
static void cb_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	start_clicked = false;
	gtk_widget_destroy(win);
}

// Dialog box has been closed
extern "C" void dl_quit(GtkWidget *dialog)
{
	gtk_widget_destroy(dialog);
}

// "About" selected
static void mn_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    char *version = g_strdup_printf("%d.%d", VERSION_MAJOR, VERSION_MINOR);
	gtk_show_about_dialog(GTK_WINDOW(win),
	                      "version", version,
	                      "copyright", sysinfo,
	                      "authors", authors,
	                      "comments", GetString(STR_ABOUT_COMMENTS),
	                      "website", GetString(STR_ABOUT_WEBSITE),
	                      "website-label", GetString(STR_ABOUT_WEBSITE_LABEL),
	                      "license", GetString(STR_ABOUT_LICENSE),
	                      "wrap-license", true,
	                      "logo-icon-name", GetString(STR_APP_NAME),
	                      NULL);
    g_free(version);
}

// "Help" selected
static void mn_help (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_show_uri_on_window(GTK_WINDOW(win), "https://github.com/kanjitalk755/macemu", GDK_CURRENT_TIME, NULL);
#else
	gtk_show_uri(gdk_screen_get_default(), "https://github.com/kanjitalk755/macemu", GDK_CURRENT_TIME, NULL);
#endif
}

// "Zap NVRAM" selected
static void mn_zap_pram (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	ZapPRAM();
}

// Action entries (used in menus and buttons)
static GActionEntry win_entries[] = {
	{ "add-volume", cb_add_volume },
	{ "create-volume", cb_create_volume },
	{ "remove-volume", cb_remove_volume },
	{ "start", cb_start },
	{ "save-settings", cb_save_settings },
	{ "zap-pram", mn_zap_pram },
	{ "quit", cb_quit },
	{ "help", mn_help },
	{ "about", mn_about },
};

// Hide widgets which aren't applicable to this emulator
static void hide_widget(const char *name)
{
	GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, name));
	gtk_widget_hide(widget);
}

// Bind the sensitivity of a widget to the active property of another
static void bind_sensitivity(const char *source_name, const char *target_name, GBindingFlags flags = G_BINDING_DEFAULT)
{
	GtkWidget *source = GTK_WIDGET(gtk_builder_get_object(builder, source_name));
	GtkWidget *target = GTK_WIDGET(gtk_builder_get_object(builder, target_name));
	g_object_bind_property(source, "active", target, "sensitive", GBindingFlags(flags | G_BINDING_SYNC_CREATE));
}

static void set_file_menu(GtkApplication *app)
{
	GtkBuilder *menubuilder = gtk_builder_new_from_resource(G_RES_PATH "menu.ui");
	GtkMenuButton *menu_button = GTK_MENU_BUTTON(gtk_builder_get_object(builder, "menu-button"));
	GMenuModel *app_menu = G_MENU_MODEL(gtk_builder_get_object(menubuilder, "app-menu"));
	GMenuModel *file_menu = G_MENU_MODEL(gtk_builder_get_object(menubuilder, "prefs-editor-menu"));

	gtk_menu_button_set_menu_model(menu_button, app_menu);
	gtk_application_set_menubar(app, file_menu);
	if (gtk_application_prefers_app_menu(app))
		gtk_application_set_app_menu(app, app_menu);

	g_object_unref(menubuilder);
}

static void set_help_overlay (GtkApplicationWindow *win)
{
#if GTK_CHECK_VERSION(3, 20, 0)
	GtkBuilder *helpbuilder = gtk_builder_new_from_resource(G_RES_PATH "help-overlay.ui");
	gtk_application_window_set_help_overlay(win,
			GTK_SHORTCUTS_WINDOW(gtk_builder_get_object(helpbuilder, "emulator-shortcuts")));
	g_object_unref(helpbuilder);
#endif
}

/*
 *  Show preferences editor
 *  Returns true when user clicked on "Start", false otherwise
 */

bool PrefsEditor(void)
{
	GApplication *app = g_application_get_default();
	if (g_application_get_is_remote(app))
	{
		printf("Another instance of %s is running, quitting...\n", GetString(STR_WINDOW_TITLE));
		return false;
	}
	builder = gtk_builder_new_from_resource(G_RES_PATH "prefs-editor.ui");
	gboolean use_headerbar = false;
	GtkSettings *settings = gtk_settings_get_default();
	if (g_strcmp0(getenv("GTK_CSD"), "0") != 0)
	{
	    g_object_get(settings, "gtk-dialogs-use-header", &use_headerbar, NULL);
	}
	set_file_menu(GTK_APPLICATION(app));

	// Create window
	win = GTK_WIDGET(gtk_builder_get_object(builder, "prefs-editor"));
	g_assert(GTK_IS_APPLICATION_WINDOW (win));
	gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(win));
	set_initial_prefs();
	set_ramsize_combo_box();
	set_jitcachesize_combo_box();
	set_hotkey_buttons();
	set_cpu_combo_box();
	gtk_builder_connect_signals(builder, NULL);
	if (use_headerbar)
	{
		gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(win), false);
	}
	else
	{
		gtk_window_set_titlebar(GTK_WINDOW(win), NULL);
		gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "action-bar")));
	}
	set_help_overlay(GTK_APPLICATION_WINDOW(win));

	gtk_window_set_title(GTK_WINDOW(win), GetString(STR_PREFS_TITLE));
	g_action_map_add_action_entries (G_ACTION_MAP (win),
	                                 win_entries,
	                                 G_N_ELEMENTS (win_entries),
	                                 win);
	g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP (win), "remove-volume")), false);
	g_signal_connect(GTK_WIDGET(win), "delete_event", G_CALLBACK(window_closed), NULL);
	g_signal_connect(GTK_WIDGET(win), "destroy", G_CALLBACK(window_destroyed), NULL);

	// Get screen dimensions
#if GTK_CHECK_VERSION(3, 22, 0)
	GdkMonitor *monitor = gdk_display_get_monitor(gdk_display_get_default(), 0);
	GdkRectangle *geometry = new GdkRectangle;
	gdk_monitor_get_geometry(monitor, geometry);
	screen_width = geometry->width;
	screen_height = geometry->height;
	g_free(geometry);
#else
	screen_width = gdk_screen_width();
	screen_height = gdk_screen_height();
#endif

	// Create window contents
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(win), accel_group);
	volumes_view = create_tree_view();
	get_graphics_settings();
	get_mouse_wheel_mode();
	bind_sensitivity("mousewheelmode", "mousewheellines");
	bind_sensitivity("keycodes", "keycodefile");
	bind_sensitivity("keycodes", "keycodefile-browse");
#ifdef SHEEPSHAVER
	hide_widget("scsi-expander");
	bind_sensitivity("jit", "jitfpu");
	hide_widget("udptunnel");
	hide_widget("udpport");
	hide_widget("udpport-label");
	hide_widget("cpu");
	hide_widget("cpu-label");
	hide_widget("modelid");
	hide_widget("modelid-label");
	hide_widget("jitcachesize");
	hide_widget("jitcachesize-label");
	hide_widget("jitfpu");
	hide_widget("jitlazyflush");
	hide_widget("jitinline");
#else
	hide_widget("gfxaccel");
	hide_widget("jit68k");
	bind_sensitivity("udptunnel", "udpport");
	bind_sensitivity("jit", "jitfpu");
	bind_sensitivity("jit", "jitlazyflush");
	bind_sensitivity("jit", "jitcachesize");
	bind_sensitivity("jit", "jitcachesize-label");
	bind_sensitivity("jit", "jitinline");
	bind_sensitivity("udptunnel", "ether", G_BINDING_INVERT_BOOLEAN);
#endif
	if (!is_jit_capable())
		gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder, "jit-pane")), false);
	GList *glist = add_serial_names();
	add_combo_box_values(glist, "seriala");
	add_combo_box_values(glist, "serialb");
	glist = add_ether_names();
	add_combo_box_values(glist, "ether");
	bind_sensitivity("udptunnel", "udpport");

	// Show window and enter main loop
	gtk_widget_show(win);
	gtk_main();
	return start_clicked;
}

/*
 *  "Volumes" pane
 */

// Values for the columns of the list store
enum {
	COLUMN_PATH,
	COLUMN_SIZE,
	COLUMN_CDROM,
	N_COLUMNS
};


// Gets the size of the volume as a pretty string
static const char* get_file_size (GFile *file)
{
	GFileInfo *info;
	if (g_file_query_exists(file, NULL))
	{
		info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
		uint64_t size = uint64_t(g_file_info_get_size(info));
		char *sizestr = g_format_size_full(size, G_FORMAT_SIZE_IEC_UNITS);
		return sizestr;
	}
	else
	{
		return "Not Found";
	}
	g_object_unref(info);
}

static bool has_file_ext (GFile *file, const char *ext)
{
	char *str = g_file_get_path(file);
	char *file_ext = g_utf8_strrchr(str, 255, '.');
	if (!file_ext)
		return 0;
	return (g_strcmp0(file_ext, ext) == 0);
}

// User selected a volume to add
static void cb_add_volume_response (GtkFileChooser *chooser, int response)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		GFile *volume = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(chooser));
		gtk_list_store_append (GTK_LIST_STORE(volume_store), &toplevel);
		gtk_list_store_set (GTK_LIST_STORE(volume_store), &toplevel,
				COLUMN_PATH, g_file_get_path(volume),
				COLUMN_SIZE, get_file_size(volume),
				COLUMN_CDROM, has_file_ext(volume, ".iso"),
				-1);
		g_object_unref(volume);
	}
	file_chooser_destroy(chooser);
}

// Something dropped on volume list
static void cb_volume_drag_data_received(GtkWidget *view, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data,
	guint info, guint time, gpointer user_data)
{
	if (gtk_selection_data_get_data_type(data) == gdk_atom_intern_static_string("text/uri-list")) {
		// get URIs from the drag selection data and add them
		gchar ** uris = gtk_selection_data_get_uris(data);
		for (gchar ** uri = uris; *uri != NULL; uri++) {

			GFile *volume = g_file_new_for_uri(*uri);
			gtk_list_store_append (GTK_LIST_STORE(volume_store), &toplevel);
			gtk_list_store_set (GTK_LIST_STORE(volume_store), &toplevel,
			        COLUMN_PATH, g_file_get_path(volume),
			        COLUMN_SIZE, get_file_size(volume),
			        COLUMN_CDROM, has_file_ext(volume, ".iso"),
			        -1);
			g_object_unref(volume);

		}
		g_strfreev(uris);
	}
}

// "Add Volume" button clicked
static void cb_add_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkFileChooser *chooser = file_chooser_new(win,
	                                           GTK_FILE_CHOOSER_ACTION_OPEN,
	                                           STR_ADD_VOLUME_TITLE,
	                                           STR_SELECT_BUTTON,
	                                           STR_CANCEL_BUTTON,
	                                           NULL);
	g_signal_connect(chooser, "response", G_CALLBACK(cb_add_volume_response), NULL);
	file_chooser_show(chooser);
}

static gboolean volume_file_create (GFile *file, uint32_t size_mb)
{
	// The dialog asks to confirm overwrite, so no need to prevent overwriting if file already exists
	GFileOutputStream *fd = g_file_replace(file, NULL, false, G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL);
	if (fd != NULL && g_seekable_can_truncate(G_SEEKABLE(fd)))
	{
		g_seekable_truncate(G_SEEKABLE(fd), size_mb * 1024 * 1024, NULL, NULL);
	}
	else return false;
	g_object_unref(fd);

	gtk_list_store_append (GTK_LIST_STORE(volume_store), &toplevel);
	gtk_list_store_set (GTK_LIST_STORE(volume_store), &toplevel,
			COLUMN_PATH, g_file_get_path(file),
			COLUMN_SIZE, get_file_size(file),
			-1);
	g_object_unref(file);
	return true;
}

static void cb_volume_create_size (GtkWidget *dialog, int response, GFile *file)
{
	if (response == GTK_RESPONSE_OK)
	{
		volume_file_create(file, gtk_adjustment_get_value(size_adj));
	}
	gtk_widget_destroy(dialog);
}

// User selected to create a new volume
static void cb_create_volume_response (GtkFileChooser *chooser, int response, GtkEntry *size_entry)
{
	GtkWidget *dialog, *spinbutton, *box;
	if (response == GTK_RESPONSE_ACCEPT)
	{
		GFile *volume = gtk_file_chooser_get_file(chooser);
		const char *str = gtk_entry_get_text(GTK_ENTRY(size_entry));
		gboolean chooser_is_widget = (strlen(str) >= 1);

		uint32_t disk_size = atoi(str);
		if (!chooser_is_widget)
		{
			dialog = gtk_message_dialog_new(GTK_WINDOW(win),
					(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_OK_CANCEL,
					"Volume Size (MB):");
			box = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog));
			size_adj = gtk_adjustment_new(atoi(VOLUME_SIZE_DEFAULT), 1, 2000, 1, 100, 100);
			spinbutton = gtk_spin_button_new(size_adj, 10, 0);
			gtk_box_pack_end(GTK_BOX(box), spinbutton, FALSE, FALSE, 0);
			gtk_widget_show(box);
			gtk_widget_show(spinbutton);
			gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win));
			g_signal_connect(dialog, "response", G_CALLBACK(cb_volume_create_size), volume);
			gtk_widget_show(dialog);
			file_chooser_destroy(chooser);
			return;
		}
		if (disk_size < 1 || disk_size > 2000)
		{
			GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(win),
					(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_CLOSE,
					"Enter a valid size");
			gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "The volume size should be between 1 and 2000.");
			gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(chooser));
			g_signal_connect(dialog, "response", G_CALLBACK(dl_quit), NULL);
			gtk_widget_show(dialog);
			return; // Don't close the file chooser dialog
		}
		volume_file_create(volume, disk_size);
	}
	file_chooser_destroy (chooser);
}

// Doesn't run if the file chooser is displayed by the portal. We use this
// to find out if we need to use a dialog to ask the user to enter a size
static void size_entry_displayed(GtkWidget *size_entry, gpointer user_data)
{
	gtk_entry_set_text(GTK_ENTRY(size_entry), VOLUME_SIZE_DEFAULT);
}

// "Create" button clicked
static void cb_create_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkFileChooser *chooser = file_chooser_new(win,
	                                           GTK_FILE_CHOOSER_ACTION_SAVE,
	                                           STR_CREATE_VOLUME_TITLE,
	                                           STR_CREATE_BUTTON,
	                                           STR_CANCEL_BUTTON,
	                                           NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(box);
	GtkWidget *label = gtk_label_new(GetString(STR_HARDFILE_SIZE_CTRL));
	gtk_widget_show(label);
	GtkWidget *size_entry = gtk_entry_new();
	gtk_widget_show(size_entry);
	gtk_entry_set_activates_default(GTK_ENTRY(size_entry), true);
	gtk_box_pack_end(GTK_BOX(box), size_entry, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(box), label, FALSE, FALSE, 0);
	gtk_widget_set_hexpand(box, TRUE);

	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(chooser), box);
	g_signal_connect(size_entry, "map", G_CALLBACK(size_entry_displayed), NULL);

	g_signal_connect(chooser, "response", G_CALLBACK(cb_create_volume_response), size_entry);
	file_chooser_show(chooser);
}

// Enables and disables the "Remove" button depending on whether a volume is selected
static void cb_remove_enable(GtkWidget *widget)
{
	bool enable = false;
	if (selection != NULL && gtk_tree_selection_count_selected_rows(selection))
		enable = true;
	g_simple_action_set_enabled(G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP (win), "remove-volume")), enable);
}

// "Remove" button clicked
static void cb_remove_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	if (gtk_tree_selection_count_selected_rows(selection))
	{
		gtk_tree_selection_get_selected(selection, &volume_store, &toplevel);
		gtk_list_store_remove(GTK_LIST_STORE(volume_store), &toplevel);
	}
}

static void cb_cdrom (GtkCellRendererToggle *cell, char *path_str, gpointer data)
{
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gboolean cd_rom;

	gtk_tree_model_get_iter (volume_store, &iter, path);
	gtk_tree_model_get (volume_store, &iter, COLUMN_CDROM, &cd_rom, -1);

	cd_rom ^= 1;

	gtk_list_store_set (GTK_LIST_STORE (volume_store), &iter, COLUMN_CDROM, cd_rom, -1);
	gtk_tree_path_free (path);
}

// Save volumes from list store to prefs
static void save_volumes (void)
{
	while (PrefsFindString("disk"))
		PrefsRemoveItem("disk");
	while (PrefsFindString("cdrom"))
		PrefsRemoveItem("cdrom");
	if (gtk_tree_model_get_iter_first(volume_store, &toplevel))
	{
		do
		{
			const char *path;
			gboolean cd_rom = true;
			gtk_tree_model_get(volume_store, &toplevel, COLUMN_CDROM, &cd_rom, -1);
			gtk_tree_model_get(volume_store, &toplevel, COLUMN_PATH, &path, -1);
			if (cd_rom)
				PrefsAddString("cdrom", path);
			else
				PrefsAddString("disk", path);
		}
		while (gtk_tree_model_iter_next(volume_store, &toplevel));
	}
}

// Gets the list of disks and cdroms from the prefs file and adds them to the list store
static GtkTreeModel *get_volumes (void)
{
	const char *str;
	int32_t index = 0;
	volume_store = GTK_TREE_MODEL(gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN));
	while ((str = (const char *)PrefsFindString("disk", index++)) != NULL)
	{
		int read_only = 0;
		if (strncmp(str, "*", 1) == 0)
		{
			read_only = 1;
		}
		gtk_list_store_append (GTK_LIST_STORE(volume_store), &toplevel);
			gtk_list_store_set (GTK_LIST_STORE(volume_store), &toplevel,
				COLUMN_PATH, str + read_only,
				COLUMN_SIZE, get_file_size(g_file_new_for_path(str + read_only)),
				COLUMN_CDROM, read_only,
				-1);
	}
	index = 0;
	while ((str = (const char *)PrefsFindString("cdrom", index++)) != NULL)
	{
		gtk_list_store_append (GTK_LIST_STORE(volume_store), &toplevel);
			gtk_list_store_set (GTK_LIST_STORE(volume_store), &toplevel,
				COLUMN_PATH, str,
				COLUMN_SIZE, get_file_size(g_file_new_for_path(str)),
				COLUMN_CDROM, true,
				-1);
	}
	return volume_store;
}

// Creates the tree view widget to display the volume list
static GtkWidget *create_tree_view (void)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	GtkWidget *view;
	GtkTreeModel *model;

	view = GTK_WIDGET(gtk_builder_get_object(builder, "volumes-view"));
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Location");
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), true);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_expand(col, true);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_PATH);

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "CD-ROM");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect (renderer, "toggled",
	                  G_CALLBACK (cb_cdrom), NULL);
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "active", COLUMN_CDROM);

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Size");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_SIZE);

	model = get_volumes();
	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
	g_object_unref(model); // destroy model automatically with view

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(selection, "changed", G_CALLBACK(cb_remove_enable), NULL);

	// also support volume files dragged onto the list from outside
	gtk_drag_dest_add_uri_targets(view);
	g_signal_connect(view, "drag_data_received", G_CALLBACK(cb_volume_drag_data_received), NULL);

	return view;
}

/*
 *  "JIT Compiler" pane
 */

// Are we running a JIT capable CPU?
static bool is_jit_capable(void)
{
#if USE_JIT
	return true;
#elif defined __APPLE__ && defined __MACH__
	// XXX run-time detect so that we can use a PPC GUI prefs editor
	static char cpu[10];
	if (cpu[0] == 0) {
		FILE *fp = popen("uname -p", "r");
		if (fp == NULL)
			return false;
		fgets(cpu, sizeof(cpu) - 1, fp);
		fclose(fp);
	}
	if (cpu[0] == 'i' && cpu[2] == '8' && cpu[3] == '6') // XXX assuming i?86
		return true;
#endif
	return false;
}

/*
 *  "Graphics/Sound" pane
 */

// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_SCREEN
};

extern "C" {

// User changed the hotkey combination
void cb_hotkey (GtkWidget *widget)
{
	GtkToggleButton *ctrl = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "ctrl-hotkey"));
	GtkToggleButton *alt = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "alt-hotkey"));
	GtkToggleButton *super = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "super-hotkey"));
	int hotkey = gtk_toggle_button_get_active(ctrl) |
	             gtk_toggle_button_get_active(alt) << 1 |
	             gtk_toggle_button_get_active(super) << 2;
	if (hotkey == 0)
		return;
	PrefsReplaceInt32("hotkey", hotkey);
}

// Adds the MB suffix to the value entered by the user
void cb_format_ramsize (GtkWidget *widget)
{
	int size_mb = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
	if (size_mb < 4)
		size_mb = 4;
	if (size_mb > 1024)
		size_mb = 1024;
	char *text = g_strdup_printf("%d MB", size_mb);
	gtk_entry_set_text(GTK_ENTRY(widget), text);
	g_free(text);
}

// Saves the new ram size preference
void cb_ramsize (GtkWidget *widget)
{
	int size_mb = atoi(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget)));
	if (size_mb >= 4 && size_mb <= 1024)
	{
		int size_bytes = size_mb << 20;
		PrefsReplaceInt32("ramsize", size_bytes);
	}
}

// Adds the MB suffix to the value entered by the user
void cb_format_jitcachesize (GtkWidget *widget)
{
	int size_mb = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
	if (size_mb < 1)
		size_mb = 1;
	if (size_mb > 128)
		size_mb = 16;
	char *text = g_strdup_printf("%d MB", size_mb);
	gtk_entry_set_text(GTK_ENTRY(widget), text);
	g_free(text);
}

// Saves the new JIT cache size preference
void cb_jitcachesize (GtkWidget *widget)
{
	int size_mb = atoi(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget)));
	if (size_mb >= 1 && size_mb <= 128)
	{
		int size_kb = size_mb << 10;
		PrefsReplaceInt32("jitcachesize", size_kb);
	}
}

// Saves the new cpu and fpu preferences
void cb_cpu (GtkWidget *widget)
{
	int value = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget)));
	int cpu = value >> 1;
	bool fpu = false;
	if (value & 1)
		fpu = true;
	PrefsReplaceInt32("cpu", cpu);
	PrefsReplaceBool("fpu", fpu);
}

// Save the id of the selected combo box item to its associated preference
void cb_combo_int (GtkWidget *widget)
{
	PrefsReplaceInt32(gtk_widget_get_name(widget), atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget))));
}

void cb_combo_str (GtkWidget *widget)
{
	PrefsReplaceString(gtk_widget_get_name(widget), gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget)));
}

// Save the value of the combo box entry to its associated preference
void cb_combo_entry_int (GtkWidget *widget)
{
	PrefsReplaceInt32(gtk_widget_get_name(widget),
	                  atoi(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget))));
}

void cb_combo_entry_str (GtkWidget *widget)
{
	PrefsReplaceString(gtk_widget_get_name(widget),
	                   gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget)));
}

// Save the value of the spin button to its associated preference
void cb_spin_button (GtkWidget *widget)
{
	PrefsReplaceInt32(gtk_widget_get_name(widget), gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)));
}

// Save the value of the check box to its associated preference
void cb_check_box (GtkWidget *widget)
{
	PrefsReplaceBool(gtk_widget_get_name(widget), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

// Save the value of the check box as an int to its associated preference
void cb_check_box_int (GtkWidget *widget)
{
	PrefsReplaceInt32(gtk_widget_get_name(widget), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

// Save the value of the check box (inverted) to its associated preference
void cb_check_box_inv (GtkWidget *widget)
{
	PrefsReplaceBool(gtk_widget_get_name(widget), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

// Save the contents of the entry to its associated preference
void cb_entry (GtkWidget *widget)
{
	if (gtk_entry_get_text_length(GTK_ENTRY(widget)))
		PrefsReplaceString(gtk_widget_get_name(widget), gtk_entry_get_text(GTK_ENTRY(widget)));
	else
		PrefsRemoveItem(gtk_widget_get_name(widget));
}

// Display the info bar
void cb_infobar_show (GtkWidget *widget)
{
#if GTK_CHECK_VERSION(3, 22, 29)
	/* Workaround for the fact that having the revealed property in the XML is not valid
	in older GTK versions. Instead we hide it until it is about to be revealed. */
	if (!gtk_widget_get_visible(widget))
	{
		gtk_info_bar_set_revealed(GTK_INFO_BAR(widget), false);
		gtk_widget_show(widget);
	}
	gtk_info_bar_set_revealed(GTK_INFO_BAR(widget), true);
#else
	gtk_widget_show(widget);
#endif
}

// Close the info bar
void cb_infobar_hide (GtkWidget *widget)
{
#if GTK_CHECK_VERSION(3, 22, 29)
	gtk_info_bar_set_revealed(GTK_INFO_BAR(widget), false);
#else
	gtk_widget_hide(GTK_WIDGET(widget));
#endif
}

void cb_swap_opt_cmd (GtkWidget *widget)
{
	bool swapped = PrefsFindBool("swap_opt_cmd");
	GtkLabel *opt_label = GTK_LABEL(gtk_builder_get_object(builder, "opt-label"));
	GtkLabel *cmd_label = GTK_LABEL(gtk_builder_get_object(builder, "cmd-label"));
	gtk_label_set_text(opt_label, swapped ? "Super" : "Alt");
	gtk_label_set_text(cmd_label, swapped ? "Alt" : "Super");
}

} // extern "C"

// Read and set mouse wheel mode preference
static void get_mouse_wheel_mode (void)
{
	GtkToggleButton *page = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "mousewheelmode-inv"));
	GtkToggleButton *cursor = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "mousewheelmode"));
	gtk_toggle_button_set_active((PrefsFindInt32("mousewheelmode") ? cursor : page), true);
}

// Read and convert graphics preferences
static void get_graphics_settings (void)
{
	display_type = DISPLAY_WINDOW;
	dis_width = 640;
	dis_height = 480;
	screen_full = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "screen-mode-full"));
	screen_win = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "screen-mode-window"));
	screen_res = GTK_COMBO_BOX(gtk_builder_get_object(builder, "screen-res"));

	mag_rate = GTK_ENTRY(gtk_builder_get_object(builder, "mag_rate"));

	const char *str = PrefsFindString("screen");
	if (str) {
		if (sscanf(str, "win/%d/%d", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(str, "dga/%d/%d", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_SCREEN;
		else if (sscanf(str, "fbdev/%d/%d", &dis_width, &dis_height) == 2) {
#ifdef ENABLE_FBDEV_DGA
			is_fbdev_dga_mode = true;
#endif
			display_type = DISPLAY_SCREEN;
		}
	}
	else {
		uint32_t window_modes = PrefsFindInt32("windowmodes");
		uint32_t screen_modes = PrefsFindInt32("screenmodes");
		if (screen_modes) {
			display_type = DISPLAY_SCREEN;
			static const struct {
				int id;
				int width;
				int height;
			}
			modes[] = {
				{  1,	 640,	 480 },
				{  2,	 800,	 600 },
				{  4,	1024,	 768 },
				{ 64,	1152,	 768 },
				{  8,	1152,	 900 },
				{ 16,	1280,	1024 },
				{ 32,	1600,	1200 },
				{ 0, }
			};
			for (int i = 0; modes[i].id != 0; i++) {
				if (screen_modes & modes[i].id) {
					if (modes[i].width <= screen_width && modes[i].height <= screen_height) {
						dis_width = modes[i].width;
						dis_height = modes[i].height;
					}
				}
			}
		}
		else if (window_modes) {
			display_type = DISPLAY_WINDOW;
			if (window_modes & 1)
				dis_width = 640, dis_height = 480;
			if (window_modes & 2)
				dis_width = 800, dis_height = 600;
		}
	}
	if (dis_width == screen_width)
		dis_width = 0;
	if (dis_height == screen_height)
		dis_height = 0;
	gtk_toggle_button_set_active ((display_type ? screen_full : screen_win), true);
	char *res_str = g_strdup_printf("%d x %d", dis_width, dis_height);
	if (!gtk_combo_box_set_active_id(screen_res, res_str))
	{
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(screen_res))), res_str);
	}
	g_free(res_str);

	// scaling
	const char *mag_rate_str = PrefsFindString("mag_rate");
	if (!mag_rate_str) {
		mag_rate_str = "1.0";
	}
	gtk_entry_set_text(GTK_ENTRY(mag_rate), mag_rate_str);
	// check boxes are handled separately
}

// Add names of serial devices
static GList *add_serial_names (void)
{
	GList *glist = NULL;

	// Search /dev for ttyS* and lp*
	DIR *d = opendir("/dev");
	if (d) {
		struct dirent *de;
		while ((de = readdir(d)) != NULL) {
#if defined(__linux__)
			if (strncmp(de->d_name, "ttyS", 4) == 0 || strncmp(de->d_name, "lp", 2) == 0) {
#elif defined(__FreeBSD__)
			if (strncmp(de->d_name, "cua", 3) == 0 || strncmp(de->d_name, "lpt", 3) == 0) {
#elif defined(__NetBSD__)
			if (strncmp(de->d_name, "tty0", 4) == 0 || strncmp(de->d_name, "lpt", 3) == 0) {
#elif defined(sgi)
			if (strncmp(de->d_name, "ttyf", 4) == 0 || strncmp(de->d_name, "plp", 3) == 0) {
#else
			if (false) {
#endif
				char *str = g_strdup_printf("/dev/%s", de->d_name);
				glist = g_list_append(glist, str);
			}
		}
		closedir(d);
	}
	if (!glist)
		glist = g_list_append(glist, (void *)"<none>");
	return glist;
}

// Add names of ethernet interfaces
static GList *add_ether_names (void)
{
	GList *glist = NULL;

	// Get list of all Ethernet interfaces
	int s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
		char inbuf[8192];
		struct ifconf ifc;
		ifc.ifc_len = sizeof(inbuf);
		ifc.ifc_buf = inbuf;
		if (ioctl(s, SIOCGIFCONF, &ifc) == 0) {
			struct ifreq req, *ifr = ifc.ifc_req;
			for (int i=0; i<ifc.ifc_len; i+=sizeof(ifreq), ifr++) {
				req = *ifr;
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(sgi)
				if (ioctl(s, SIOCGIFADDR, &req) == 0 && (req.ifr_addr.sa_family == ARPHRD_ETHER || req.ifr_addr.sa_family == ARPHRD_ETHER+1)) {
#elif defined(__linux__)
				if (ioctl(s, SIOCGIFHWADDR, &req) == 0 && req.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
#else
				if (false) {
#endif
					char *str = new char[64];
					strncpy(str, ifr->ifr_name, 63);
					glist = g_list_append(glist, str);
				}
			}
		}
		close(s);
	}
#ifdef HAVE_SLIRP
	static char s_slirp[] = "slirp";
	glist = g_list_append(glist, s_slirp);
#endif
	if (!glist)
		glist = g_list_append(glist, (void *)"<none>");
	return glist;
}

#ifdef STANDALONE_GUI
#include <errno.h>
#include <sys/wait.h>
#include "rpc.h"

/*
 *  Fake unused data and functions
 */

uint8_t XPRAM[XPRAM_SIZE];
void MountVolume(void *fh) { }
void FileDiskLayout(loff_t size, uint8_t *data, loff_t &start_byte, loff_t &real_size) { }

#if defined __APPLE__ && defined __MACH__
void DarwinSysInit(void) { }
void DarwinSysExit(void) { }
void DarwinAddFloppyPrefs(void) { }
void DarwinAddSerialPrefs(void) { }
bool DarwinCDReadTOC(char *, uint8_t *) { }
#endif

static GCallback dl_destroyed(GtkWidget *dialog)
{
	gtk_widget_destroy(dialog);
	gtk_main_quit();
	return NULL;
}

void display_alert(int title_id, int prefix_id, int button_id, const char *text)
{
	GtkWidget *dialog = gtk_message_dialog_new(NULL,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_WARNING,
						GTK_BUTTONS_NONE,
						GetString(title_id), NULL);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), text);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GetString(button_id), GTK_RESPONSE_CLOSE);
	g_signal_connect(dialog, "response", G_CALLBACK(dl_destroyed), NULL);
	gtk_widget_show(dialog);
	gtk_main();
	return;
}

/*
 *  Display error alert
 */

void ErrorAlert (const char *text)
{
	display_alert(STR_ERROR_ALERT_TITLE, STR_GUI_ERROR_PREFIX, STR_QUIT_BUTTON, text);
}


/*
 *  Display warning alert
 */

void WarningAlert (const char *text)
{
	display_alert(STR_WARNING_ALERT_TITLE, STR_GUI_WARNING_PREFIX, STR_OK_BUTTON, text);
}


/*
 *  RPC handlers
 */

static GMainLoop *g_gui_loop;

static int handle_ErrorAlert(rpc_connection_t *connection)
{
	D(bug("handle_ErrorAlert\n"));

	int error;
	char *str;
	if ((error = rpc_method_get_args(connection, RPC_TYPE_STRING, &str, RPC_TYPE_INVALID)) < 0)
		return error;

	ErrorAlert(str);
	free(str);
	return RPC_ERROR_NO_ERROR;
}

static int handle_WarningAlert(rpc_connection_t *connection)
{
	D(bug("handle_WarningAlert\n"));

	int error;
	char *str;
	if ((error = rpc_method_get_args(connection, RPC_TYPE_STRING, &str, RPC_TYPE_INVALID)) < 0)
		return error;

	WarningAlert(str);
	free(str);
	return RPC_ERROR_NO_ERROR;
}

static int handle_Exit(rpc_connection_t *connection)
{
	D(bug("handle_Exit\n"));

	g_main_quit(g_gui_loop);
	return RPC_ERROR_NO_ERROR;
}


/*
 *  SIGCHLD handler
 */

static char g_app_path[PATH_MAX];
static rpc_connection_t *g_gui_connection = NULL;

static void sigchld_handler (int sig, siginfo_t *sip, void *)
{
	D(bug("Child %d exitted with status = %x\n", sip->si_pid, sip->si_status));

	// XXX perform a new wait because sip->si_status is sometimes not
	// the exit _value_ on MacOS X but rather the usual status field
	// from waitpid() -- we could arrange this code in some other way...
	int status;
	if (waitpid(sip->si_pid, &status, 0) < 0)
		status = sip->si_status;
	if (WIFEXITED(status))
		status = WEXITSTATUS(status);
	if (status & 0x80)
		status |= -1 ^0xff;

	if (status < 0) {	// negative -> execlp/-errno
		char *str = g_strdup_printf(GetString(STR_NO_B2_EXE_FOUND), g_app_path, strerror(-status));
		ErrorAlert(str);
		status = 1;
		g_free(str);
	}

	if (status != 0) {
		if (g_gui_connection)
			rpc_exit(g_gui_connection);
		exit(status);
	}
}


/*
 *  Start standalone GUI
 */

int main (int argc, char *argv[])
{
	// Read preferences
	PrefsInit(0, argc, argv);
	// Show preferences editor
	bool start = PrefsEditor();

	// Exit preferences
	PrefsExit();

	// Transfer control to the executable
	if (start) {
		// Catch exits from the child process
		struct sigaction sigchld_sa, old_sigchld_sa;
		sigemptyset(&sigchld_sa.sa_mask);
		sigchld_sa.sa_sigaction = sigchld_handler;
		sigchld_sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
		if (sigaction(SIGCHLD, &sigchld_sa, &old_sigchld_sa) < 0) {
			char *str = g_strdup_printf(GetString(STR_SIG_INSTALL_ERR), SIGCHLD, strerror(errno));
			ErrorAlert(str);
			return 1;
			g_free(str);
		}

		// Search and run the SheepShaver executable
		char *p;
		strcpy(g_app_path, argv[0]);
		if ((p = strstr(g_app_path, "SheepShaverGUI.app/Contents/MacOS")) != NULL) {
			strcpy(p, "SheepShaver.app/Contents/MacOS/SheepShaver");
			if (access(g_app_path, X_OK) < 0) {
				char *str = g_strdup_printf(GetString(STR_NO_B2_EXE_FOUND), g_app_path, strerror(errno));
				WarningAlert(str);
				strcpy(g_app_path, "/Applications/SheepShaver.app/Contents/MacOS/SheepShaver");
				g_free(str);
			}
		} else {
			p = strrchr(g_app_path, '/');
			p = p ? p + 1 : g_app_path;
			strcpy(p, "SheepShaver");
		}

		char *gui_connection_path = g_strdup_printf("/org/SheepShaver/GUI/%d", getpid());
		int pid = fork();
		if (pid == 0) {
			D(bug("Trying to execute %s\n", g_app_path));
			execlp(g_app_path, g_app_path, "--gui-connection", gui_connection_path, (char *)NULL);
			g_free(gui_connection_path);
#ifdef _POSIX_PRIORITY_SCHEDULING
			// XXX get a chance to run the parent process so that to not confuse/upset GTK...
			sched_yield();
#endif
			_exit(-errno);
		}

		// Establish a connection to Basilisk II
		if ((g_gui_connection = rpc_init_server(gui_connection_path)) == NULL) {
			printf("ERROR: failed to initialize GUI-side RPC server connection\n");
			g_free(gui_connection_path);
			return 1;
		}
		g_free(gui_connection_path);

		static const rpc_method_descriptor_t vtable[] = {
			{ RPC_METHOD_ERROR_ALERT,			handle_ErrorAlert },
			{ RPC_METHOD_WARNING_ALERT,			handle_WarningAlert },
			{ RPC_METHOD_EXIT,					handle_Exit }
		};
		if (rpc_method_add_callbacks(g_gui_connection, vtable, sizeof(vtable) / sizeof(vtable[0])) < 0) {
			printf("ERROR: failed to setup GUI method callbacks\n");
			return 1;
		}
		int socket;
		if ((socket = rpc_listen_socket(g_gui_connection)) < 0) {
			printf("ERROR: failed to initialize RPC server thread\n");
			return 1;
		}

		g_gui_loop = g_main_new(TRUE);
		while (g_main_is_running(g_gui_loop)) {

			// Process a few events pending
			const int N_EVENTS_DISPATCH = 10;
			for (int i = 0; i < N_EVENTS_DISPATCH; i++) {
				if (!g_main_iteration(FALSE))
					break;
			}

			// Check for RPC events (100 ms timeout)
			int ret = rpc_wait_dispatch(g_gui_connection, 100000);
			if (ret == 0)
				continue;
			if (ret < 0)
				break;
			rpc_dispatch(g_gui_connection);
		}
		rpc_exit(g_gui_connection);
		return 0;
	}
	return 0;
}
#endif
