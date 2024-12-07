/*
 *  prefs_editor_linux.cpp - Preferences editor, Linux implementation using GTK+
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <cerrno>

#include "user_strings.h"
#include "version.h"
#include "cdrom.h"
#include "xpram.h"
#include "prefs.h"
#include "prefs_editor.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static GtkWidget *win;				// Preferences window
static bool start_clicked = false;	// Return value of PrefsEditor() function
static int screen_width, screen_height; // Screen dimensions


// Prototypes
static void create_volumes_pane(GtkWidget *top);
static void create_graphics_pane(GtkWidget *top);
static void create_input_pane(GtkWidget *top);
static void create_serial_pane(GtkWidget *top);
static void create_memory_pane(GtkWidget *top);
static void create_jit_pane(GtkWidget *top);
static void read_settings(void);


/*
 *  Utility functions
 */

#if ! GLIB_CHECK_VERSION(2,0,0)
#define G_OBJECT(obj)							GTK_OBJECT(obj)
#define g_object_get_data(obj, key)				gtk_object_get_data((obj), (key))
#define g_object_set_data(obj, key, data)		gtk_object_set_data((obj), (key), (data))
#endif

struct opt_desc {
	int label_id;
	GCallback func;
};

struct combo_desc {
	int label_id;
};

// User closed the file chooser dialog, possibly selecting a file
static void cb_browse_response(GtkWidget *chooser, int response, GtkEntry *entry)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		gtk_entry_set_text(GTK_ENTRY(entry), filename);
		g_free (filename);
	}
	gtk_widget_destroy (chooser);
}

// Open the file chooser dialog to select a file
static void cb_browse(GtkWidget *button, GtkWidget *entry)
{
	GtkWidget *chooser = gtk_file_chooser_dialog_new(GetString(STR_BROWSE_TITLE),
							GTK_WINDOW(win),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							"Cancel", GTK_RESPONSE_CANCEL,
							"Open", GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), g_dirname(gtk_entry_get_text(GTK_ENTRY(entry))));
	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
	gtk_window_set_transient_for(GTK_WINDOW(chooser), GTK_WINDOW(win));
	gtk_window_set_modal(GTK_WINDOW(chooser), true);
	g_signal_connect(chooser, "response", G_CALLBACK(cb_browse_response), GTK_ENTRY(entry));
	gtk_widget_show(chooser);
}

// Open the file chooser dialog to select a folder
static void cb_browse_dir(GtkWidget *button, GtkWidget *entry)
{
	GtkWidget *chooser = gtk_file_chooser_dialog_new(GetString(STR_BROWSE_FOLDER_TITLE),
							GTK_WINDOW(win),
							GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							"Cancel", GTK_RESPONSE_CANCEL,
							"Select", GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
	gtk_window_set_transient_for(GTK_WINDOW(chooser), GTK_WINDOW(win));
	gtk_window_set_modal(GTK_WINDOW(chooser), true);
	g_signal_connect(chooser, "response", G_CALLBACK(cb_browse_response), GTK_WIDGET(entry));
	gtk_widget_show(chooser);
}

static GtkWidget *make_browse_button(GtkWidget *entry, bool only_dirs)
{
	GtkWidget *button;

	button = gtk_button_new_with_label(GetString(STR_BROWSE_CTRL));
	gtk_widget_show(button);
	g_signal_connect(button, "clicked", only_dirs ? G_CALLBACK(cb_browse_dir) : G_CALLBACK(cb_browse), entry);
	return button;
}

static void add_menu_item(GtkWidget *menu, int label_id, GCallback func)
{
	GtkWidget *item = gtk_menu_item_new_with_label(GetString(label_id));
	gtk_widget_show(item);
	g_signal_connect(item, "activate", func, NULL);
	gtk_menu_append(GTK_MENU(menu), item);
}

static GtkWidget *make_pane(GtkWidget *notebook, int title_id)
{
	GtkWidget *frame, *label, *box;

	frame = gtk_frame_new(NULL);
	gtk_container_border_width(GTK_CONTAINER(frame), 4);

	box = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(box), 4);
	gtk_container_add(GTK_CONTAINER(frame), box);

	gtk_widget_show_all(frame);

	label = gtk_label_new(GetString(title_id));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
	return box;
}

static GtkWidget *make_button_box(GtkWidget *top, int border, const opt_desc *buttons)
{
	GtkWidget *bb, *button;

	bb = gtk_hbutton_box_new();
	gtk_widget_show(bb);
	gtk_container_set_border_width(GTK_CONTAINER(bb), border);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bb), GTK_BUTTONBOX_DEFAULT_STYLE);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bb), 4);
	gtk_box_pack_start(GTK_BOX(top), bb, FALSE, FALSE, 0);

	while (buttons->label_id) {
		button = gtk_button_new_with_label(GetString(buttons->label_id));
		gtk_widget_show(button);
		g_signal_connect_object(button, "clicked", buttons->func, NULL, (GConnectFlags) 0);
		gtk_box_pack_start(GTK_BOX(bb), button, TRUE, TRUE, 0);
		buttons++;
	}
	return bb;
}

static GtkWidget *make_separator(GtkWidget *top)
{
	GtkWidget *sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(top), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);
	return sep;
}

static GtkWidget *make_table(GtkWidget *top, int x, int y)
{
	GtkWidget *table = gtk_table_new(x, y, FALSE);
	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(top), table, FALSE, FALSE, 0);
	return table;
}

static GtkWidget *table_make_combobox(GtkWidget *table, int row, int label_id, const char *pref, GList *list)
{
	GtkWidget *label, *combo;
	char str[32];
	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	combo = gtk_combo_box_entry_new_text();
	gtk_widget_show(combo);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	while(list)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), ((gchar *) list->data));
		list = list->next;
	}
	if (pref != NULL)
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN (combo))), pref);

	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row + 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);

	return combo;
}

static GtkWidget *table_make_combobox(GtkWidget *table, int row, int label_id, const char *default_value, const combo_desc *options)
{
	GList *glist = NULL;
	while (options->label_id) {
		glist = g_list_append(glist, (void *)GetString(options->label_id));
		options++;
	}

	return table_make_combobox(table, row, label_id, default_value, glist);
}

static GtkWidget *table_make_file_entry(GtkWidget *table, int row, int label_id, const char *prefs_item, bool only_dirs = false)
{
	GtkWidget *box, *label, *entry, *button;

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	const char *str = PrefsFindString(prefs_item);
	if (str == NULL)
		str = "";

	box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_table_attach(GTK_TABLE(table), box, 1, 2, row, row + 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);

	button = make_browse_button(entry, false);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(entry), "chooser_button", button);
	return entry;
}

static GtkWidget *make_option_menu(GtkWidget *top, int label_id, const combo_desc *options, GCallback func, int active)
{
	GtkWidget *box, *label, *combo;

	box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(top), box, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	combo = gtk_combo_box_new_text();
	gtk_widget_show(combo);
	while (options->label_id) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), GetString(options->label_id));
		options++;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);
	g_signal_connect(combo, "changed", func, NULL);
	return combo;
}

static GtkWidget *make_file_entry(GtkWidget *top, int label_id, const char *prefs_item, bool only_dirs = false)
{
	GtkWidget *box, *label, *entry, *button;

	box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(top), box, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_widget_show(entry);
	const char *str = PrefsFindString(prefs_item);
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	button = make_browse_button(entry, only_dirs);

	gtk_widget_show(entry);
#if GLIB_CHECK_VERSION(2,26,0)
	g_object_bind_property(entry, "sensitive", button, "sensitive", G_BINDING_SYNC_CREATE);
#endif
	gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	return entry;
}

static const gchar *get_file_entry_path(GtkWidget *entry)
{
	return gtk_entry_get_text(GTK_ENTRY(entry));
}

static GtkWidget *make_checkbox(GtkWidget *top, int label_id, const char *prefs_item, GCallback func)
{
	GtkWidget *button = gtk_check_button_new_with_label(GetString(label_id));
	gtk_widget_show(button);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), PrefsFindBool(prefs_item));
	g_signal_connect(button, "toggled", func, NULL);
	if (top)
	    gtk_box_pack_start(GTK_BOX(top), button, FALSE, FALSE, 0);
	return button;
}

static GtkWidget *make_checkbox(GtkWidget *top, int label_id, bool active, GCallback func)
{
	GtkWidget *button = gtk_check_button_new_with_label(GetString(label_id));
	gtk_widget_show(button);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), active);
	g_signal_connect(button, "toggled", func, NULL);
	gtk_box_pack_start(GTK_BOX(top), button, FALSE, FALSE, 0);
	return button;
}


/*
 *  Show preferences editor
 *  Returns true when user clicked on "Start", false otherwise
 */

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

// "Save" button clicked
static void cb_save(...)
{
	read_settings();
	SavePrefs();
}

// "Start" button clicked
static void cb_start(...)
{
	start_clicked = true;
	read_settings();
	SavePrefs();
	gtk_widget_destroy(win);
}

// "Quit" button clicked
static void cb_quit(...)
{
	start_clicked = false;
	gtk_widget_destroy(win);
}

// "OK" button of "About" dialog clicked
extern "C" void dl_quit(GtkWidget *dialog)
{
	gtk_widget_destroy(dialog);
}

// "About" selected
static void mn_about(...)
{
	GtkWidget *dialog, *label, *button;
	const char *authors[] = {
		"Christian Bauer",
		"Marc Hellwig",
		"Gwenolé Beauchesne",
		NULL
	};
	char version[64];
	sprintf(version, "%d.%d", VERSION_MAJOR, VERSION_MINOR);
	gtk_show_about_dialog(GTK_WINDOW(win), "version", version,
	                     "copyright", GetString(STR_ABOUT_COPYRIGHT),
	                     "authors", authors,
	                     "comments", GetString(STR_ABOUT_COMMENTS),
	                     "website", GetString(STR_ABOUT_WEBSITE),
	                     "website-label", GetString(STR_ABOUT_WEBSITE_LABEL),
	                     "license", GetString(STR_ABOUT_LICENSE),
	                     "wrap-license", true,
	                     "logo-icon-name", "SheepShaver",
	                     NULL);
}

// "Zap NVRAM" selected
static void mn_zap_pram(...)
{
	ZapPRAM();
}

// Menu item descriptions
static GtkItemFactoryEntry menu_items[] = {
	{(gchar *)GetString(STR_PREFS_MENU_FILE_GTK),		NULL,			NULL,							0, "<Branch>"},
	{(gchar *)GetString(STR_PREFS_ITEM_START_GTK),		"<control>S",	G_CALLBACK(cb_start),		0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_SAVE_GTK),		NULL,			G_CALLBACK(cb_save),		0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_ZAP_PRAM_GTK),	NULL,			G_CALLBACK(mn_zap_pram),	0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_SEPL_GTK),		NULL,			NULL,							0, "<Separator>"},
	{(gchar *)GetString(STR_PREFS_ITEM_QUIT_GTK),		"<control>Q",	G_CALLBACK(cb_quit),		0, NULL},
	{(gchar *)GetString(STR_HELP_MENU_GTK),				NULL,			NULL,							0, "<LastBranch>"},
	{(gchar *)GetString(STR_HELP_ITEM_ABOUT_GTK),		NULL,			G_CALLBACK(mn_about),		0, NULL}
};

bool PrefsEditor(void)
{
	// Get screen dimensions
	screen_width = gdk_screen_width();
	screen_height = gdk_screen_height();

	// Create window
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), GetString(STR_PREFS_TITLE));
	g_signal_connect(win, "delete_event", G_CALLBACK(window_closed), NULL);
	g_signal_connect(win, "destroy", G_CALLBACK(window_destroyed), NULL);

	// Create window contents
	GtkWidget *box = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_container_add(GTK_CONTAINER(win), box);

	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkItemFactory *item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items(item_factory, sizeof(menu_items) / sizeof(menu_items[0]), menu_items, NULL);
	gtk_window_add_accel_group(GTK_WINDOW(win), accel_group);

	GtkWidget *menu_bar = gtk_item_factory_get_widget(item_factory, "<main>");
	gtk_widget_show(menu_bar);
	gtk_box_pack_start(GTK_BOX(box), menu_bar, FALSE, TRUE, 0);

	GtkWidget *notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(box), notebook, TRUE, TRUE, 0);
	gtk_widget_realize(notebook);

	create_volumes_pane(notebook);
	create_graphics_pane(notebook);
	create_input_pane(notebook);
	create_serial_pane(notebook);
	create_memory_pane(notebook);
	create_jit_pane(notebook);
	gtk_widget_show(notebook);

	static const opt_desc buttons[] = {
		{STR_START_BUTTON, G_CALLBACK(cb_start)},
		{STR_QUIT_BUTTON, G_CALLBACK(cb_quit)},
		{0, NULL}
	};
	make_button_box(box, 4, buttons);

	// Show window and enter main loop
	gtk_widget_show(win);
	gtk_main();
	return start_clicked;
}


/*
 *  "Volumes" pane
 */

static GtkWidget *volume_list, *w_extfs;
static int selected_volume;

// Volume in list selected
static void cl_selected(GtkWidget *list, int row, int column)
{
	selected_volume = row;
}

// Something dropped on volume list
static void drag_data_received(GtkWidget *list, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data,
	guint info, guint time, gpointer user_data)
{
	// reordering drags have already been handled by clist
	if (data->type == gdk_atom_intern("gtk-clist-drag-reorder", true)) {
		return;
	}

	// get URIs from the drag selection data and add them
	gchar ** uris = g_strsplit((gchar *)(data->data), "\r\n", -1);
	for (gchar ** uri = uris; *uri != NULL; uri++) {
		if (strlen(*uri) < 7) continue;
		if (strncmp("file://", *uri, 7) != 0) continue;

		gchar * filename = g_filename_from_uri(*uri, NULL, NULL);
		if (filename) {
			gtk_clist_append(GTK_CLIST(volume_list), &filename);
			g_free(filename);
		}
	}
	g_strfreev(uris);
}

// Volume selected for addition
static void cb_add_volume_response (GtkWidget *chooser, int response)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
		gtk_clist_append(GTK_CLIST(volume_list), &file);
	}
	gtk_widget_destroy(chooser);
}

// Volume selected for creation
static void cb_create_volume_response (GtkWidget *chooser, int response, GtkEntry *size_entry)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
		const gchar *str = gtk_entry_get_text(GTK_ENTRY(size_entry));
		int disk_size = atoi(str);
		if (disk_size < 1 || disk_size > 2000)
		{
			GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(win),
							(GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
							GTK_MESSAGE_WARNING,
							GTK_BUTTONS_CLOSE,
							"Enter a valid size", NULL);
			gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "The volume size should be between 1 and 2000.");
			gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(chooser));
			g_signal_connect(dialog, "response", G_CALLBACK(dl_quit), NULL);
			gtk_widget_show(dialog);
			return; // Don't close the file chooser dialog
		}
		int fd = open(file, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (fd < 0) {
			fprintf(stderr, "Could not create %s (%s)\n", file, strerror(errno));
		} else {
			ftruncate(fd, disk_size * 1024 * 1024);
			gtk_clist_append(GTK_CLIST(volume_list), &file);
		}
	}
	gtk_widget_destroy (chooser);
}

// "Add Volume" button clicked
static void cb_add_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *chooser = gtk_file_chooser_dialog_new(GetString(STR_ADD_VOLUME_TITLE),
							GTK_WINDOW(win),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							"Cancel", GTK_RESPONSE_CANCEL,
							"Add", GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), g_get_home_dir());
	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(chooser), true);
	g_signal_connect(chooser, "response", G_CALLBACK(cb_add_volume_response), NULL);
	gtk_widget_show(chooser);
}

// "Create Hardfile" button clicked
static void cb_create_volume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GtkWidget *chooser = gtk_file_chooser_dialog_new(GetString(STR_CREATE_VOLUME_TITLE),
							GTK_WINDOW(win),
							GTK_FILE_CHOOSER_ACTION_SAVE,
							"Cancel", GTK_RESPONSE_CANCEL,
							"Create", GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), g_get_home_dir());
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser), TRUE);
	gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
	gtk_window_set_transient_for(GTK_WINDOW(chooser), GTK_WINDOW(win));
	gtk_window_set_modal(GTK_WINDOW(chooser), true);

	GtkWidget *box = gtk_hbox_new(false, 8);
	gtk_widget_show(box);
	GtkWidget *label = gtk_label_new(GetString(STR_HARDFILE_SIZE_CTRL));
	gtk_widget_show(label);
	GtkWidget *size_entry = gtk_entry_new();
	gtk_widget_show(size_entry);
	gtk_entry_set_activates_default(GTK_ENTRY(size_entry), TRUE);
	gtk_entry_set_text(GTK_ENTRY(size_entry), "256");
	gtk_box_pack_end(GTK_BOX(box), size_entry, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(box), label, FALSE, FALSE, 0);

	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(chooser), box);

	g_signal_connect(chooser, "response", G_CALLBACK(cb_create_volume_response), size_entry);
	gtk_widget_show(chooser);
}
// "Remove Volume" button clicked
static void cb_remove_volume(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	gtk_clist_remove(GTK_CLIST(volume_list), selected_volume);
}

// "Boot From" selected
static void mn_bootdriver(GtkWidget *widget)
{
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)))
		PrefsReplaceInt32("bootdriver", CDROMRefNum);
	else
		PrefsReplaceInt32("bootdriver", 0);
}

// "No CD-ROM Driver" button toggled
static void tb_nocdrom(GtkWidget *widget)
{
	PrefsReplaceBool("nocdrom", GTK_TOGGLE_BUTTON(widget)->active);
}

// Read settings from widgets and set preferences
static void read_volumes_settings(void)
{
	while (PrefsFindString("disk"))
		PrefsRemoveItem("disk");

	for (int i=0; i<GTK_CLIST(volume_list)->rows; i++) {
		char *str;
		gtk_clist_get_text(GTK_CLIST(volume_list), i, 0, &str);
		PrefsAddString("disk", str);
	}

	PrefsReplaceString("extfs", gtk_entry_get_text(GTK_ENTRY(w_extfs)));
}

// Create "Volumes" pane
static void create_volumes_pane(GtkWidget *top)
{
	GtkWidget *box, *scroll, *menu;

	box = make_pane(top, STR_VOLUMES_PANE_TITLE);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	volume_list = gtk_clist_new(1);
	gtk_widget_show(volume_list);
	gtk_clist_set_selection_mode(GTK_CLIST(volume_list), GTK_SELECTION_SINGLE);
	gtk_clist_set_shadow_type(GTK_CLIST(volume_list), GTK_SHADOW_NONE);
	gtk_clist_set_reorderable(GTK_CLIST(volume_list), true);
	g_signal_connect(volume_list, "select_row", G_CALLBACK(cl_selected), NULL);

	// also support volume files dragged onto the list from outside
	gtk_drag_dest_add_uri_targets(volume_list);
	// add a drop handler to get dropped files; don't supersede the drop handler for reordering
	gtk_signal_connect_after(GTK_OBJECT(volume_list), "drag_data_received", GTK_SIGNAL_FUNC(drag_data_received), NULL);

	char *str;
	int32 index = 0;
	while ((str = (char *)PrefsFindString("disk", index++)) != NULL)
		gtk_clist_append(GTK_CLIST(volume_list), &str);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), volume_list);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
	selected_volume = 0;

	static const opt_desc buttons[] = {
		{STR_ADD_VOLUME_BUTTON, G_CALLBACK(cb_add_volume)},
		{STR_CREATE_VOLUME_BUTTON, G_CALLBACK(cb_create_volume)},
		{STR_REMOVE_VOLUME_BUTTON, G_CALLBACK(cb_remove_volume)},
		{0, NULL},
	};
	make_button_box(box, 0, buttons);
	make_separator(box);

	w_extfs = make_file_entry(box, STR_EXTFS_CTRL, "extfs", true);

	static const combo_desc options[] = {
		STR_BOOT_ANY_LAB,
		STR_BOOT_CDROM_LAB,
		0
	};
	int bootdriver = PrefsFindInt32("bootdriver"), active = 0;
	switch (bootdriver) {
		case 0: active = 0; break;
		case CDROMRefNum: active = 1; break;
	}
	menu = make_option_menu(box, STR_BOOTDRIVER_CTRL, options, G_CALLBACK(mn_bootdriver), active);

	make_checkbox(box, STR_NOCDROM_CTRL, "nocdrom", G_CALLBACK(tb_nocdrom));
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

// Set sensitivity of widgets
static void set_jit_sensitive(void)
{
	const bool jit_enabled = PrefsFindBool("jit");
}

// "Use JIT Compiler" button toggled
static void tb_jit(GtkWidget *widget)
{
	PrefsReplaceBool("jit", GTK_TOGGLE_BUTTON(widget)->active);
	set_jit_sensitive();
}

// Read settings from widgets and set preferences
static void read_jit_settings(void)
{
	bool jit_enabled = is_jit_capable() && PrefsFindBool("jit");
}

// "Use built-in 68k DR emulator" button toggled
static void tb_jit_68k(GtkWidget *widget)
{
	PrefsReplaceBool("jit68k", GTK_TOGGLE_BUTTON(widget)->active);
}

// Create "JIT Compiler" pane
static void create_jit_pane(GtkWidget *top)
{
	GtkWidget *box, *table, *label, *menu;
	char str[32];

	box = make_pane(top, STR_JIT_PANE_TITLE);

	if (is_jit_capable()) {
		make_checkbox(box, STR_JIT_CTRL, "jit", G_CALLBACK(tb_jit));
		set_jit_sensitive();
	}

	make_checkbox(box, STR_JIT_68K_CTRL, "jit68k", G_CALLBACK(tb_jit_68k));
}


/*
 *  "Graphics/Sound" pane
 */

// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_SCREEN
};

static GtkWidget *w_frameskip, *w_display_x, *w_display_y;
static GtkWidget *l_frameskip, *l_display_x, *l_display_y;
static int display_type;
static int dis_width, dis_height;
static bool is_fbdev_dga_mode = false;

static GtkWidget *w_dspdevice_file, *w_mixerdevice_file;

static GtkWidget *mag_rate, *scale_nearest, *scale_integer;

// "Window"/"Fullscreen" video type selected
static void mn_display(GtkWidget *widget)
{
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)))
		display_type = DISPLAY_SCREEN;
	else
		display_type = DISPLAY_WINDOW;
}

// "5 Hz".."60Hz" selected
static void mn_frameskip(GtkWidget *widget)
{
	int frameskip = 1;
	switch(gtk_combo_box_get_active(GTK_COMBO_BOX(widget)))
	{
		case 0: frameskip = 12; break;
		case 1: frameskip = 8; break;
		case 2: frameskip = 6; break;
		case 3: frameskip = 4; break;
		case 4: frameskip = 2; break;
		case 5: frameskip = 1; break;
	}
	PrefsReplaceInt32("frameskip", frameskip);
}

// QuickDraw acceleration
static void tb_gfxaccel(GtkWidget *widget)
{
	PrefsReplaceBool("gfxaccel", GTK_TOGGLE_BUTTON(widget)->active);
}

// Set sensitivity of widgets
static void set_graphics_sensitive(void)
{
	const bool sound_enabled = !PrefsFindBool("nosound");
	gtk_widget_set_sensitive(w_dspdevice_file, sound_enabled);
	gtk_widget_set_sensitive(w_mixerdevice_file, sound_enabled);
}

// "Disable Sound Output" button toggled
static void tb_nosound(GtkWidget *widget)
{
	PrefsReplaceBool("nosound", GTK_TOGGLE_BUTTON(widget)->active);
	set_graphics_sensitive();
}

// "Nearest" button toggled
static void tb_scale_nearest(GtkWidget *widget)
{
    PrefsReplaceBool("scale_nearest", GTK_TOGGLE_BUTTON(widget)->active);
}

// "Integer Scaling" button toggled
static void tb_scale_integer(GtkWidget *widget)
{
    PrefsReplaceBool("scale_integer", GTK_TOGGLE_BUTTON(widget)->active);
}

// Read and convert graphics preferences
static void parse_graphics_prefs(void)
{
	display_type = DISPLAY_WINDOW;
	dis_width = 640;
	dis_height = 480;

	const char *str = PrefsFindString("screen");
	if (str) {
		if (sscanf(str, "win/%d/%d", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(str, "dga/%d/%d", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_SCREEN;
#ifdef ENABLE_FBDEV_DGA
		else if (sscanf(str, "fbdev/%d/%d", &dis_width, &dis_height) == 2) {
			is_fbdev_dga_mode = true;
			display_type = DISPLAY_SCREEN;
		}
#endif
	}
	else {
		uint32 window_modes = PrefsFindInt32("windowmodes");
		uint32 screen_modes = PrefsFindInt32("screenmodes");
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
}

// Read settings from widgets and set preferences
static void read_graphics_settings(void)
{
	const char *str;

	str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w_display_x));
	dis_width = atoi(str);

	str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w_display_y));
	dis_height = atoi(str);

	char pref[256];
	bool use_screen_mode = true;
	switch (display_type) {
		case DISPLAY_WINDOW:
			sprintf(pref, "win/%d/%d", dis_width, dis_height);
			break;
		case DISPLAY_SCREEN:
			sprintf(pref, "dga/%d/%d", dis_width, dis_height);
			break;
		default:
			use_screen_mode = false;
			PrefsRemoveItem("screen");
			return;
	}
	if (use_screen_mode) {
		PrefsReplaceString("screen", pref);
		// Old prefs are now migrated
		PrefsRemoveItem("windowmodes");
		PrefsRemoveItem("screenmodes");
	}

	PrefsReplaceString("dsp", get_file_entry_path(w_dspdevice_file));
	PrefsReplaceString("mixer", get_file_entry_path(w_mixerdevice_file));

	PrefsReplaceString("mag_rate", gtk_entry_get_text(GTK_ENTRY(mag_rate)));
}

// Create "Graphics/Sound" pane
static void create_graphics_pane(GtkWidget *top)
{
	GtkWidget *box, *table, *label, *combo;
	char str[32];
	char *markup;

	parse_graphics_prefs();

	box = make_pane(top, STR_GRAPHICS_SOUND_PANE_TITLE);
	table = make_table(box, 4, 4);

	label = gtk_label_new(GetString(STR_VIDEO_TYPE_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	combo = gtk_combo_box_new_text();
	gtk_widget_show(combo);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), GetString(STR_WINDOW_CTRL));
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), GetString(STR_FULLSCREEN_CTRL));
	switch (display_type) {
		case DISPLAY_WINDOW:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
		case DISPLAY_SCREEN:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 1);
			break;
	}
	g_signal_connect(combo, "changed", G_CALLBACK(mn_display), NULL);
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 0, 1, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	l_frameskip = gtk_label_new(GetString(STR_FRAMESKIP_CTRL));
	gtk_widget_show(l_frameskip);
	gtk_table_attach(GTK_TABLE(table), l_frameskip, 0, 1, 1, 2, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_frameskip = gtk_combo_box_new_text();
	gtk_widget_show(w_frameskip);
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_frameskip), GetString(STR_REF_5HZ_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_frameskip), GetString(STR_REF_7_5HZ_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_frameskip), GetString(STR_REF_10HZ_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_frameskip), GetString(STR_REF_15HZ_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_frameskip), GetString(STR_REF_30HZ_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_frameskip), GetString(STR_REF_60HZ_LAB));
	int frameskip = PrefsFindInt32("frameskip");
	int item = -1;
	switch (frameskip) {
		case 12: item = 0; break;
		case 8: item = 1; break;
		case 6: item = 2; break;
		case 4: item = 3; break;
		case 2: item = 4; break;
		case 1: item = 5; break;
		case 0: item = 5; break;
	}
	if (item >= 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w_frameskip), item);
	g_signal_connect(w_frameskip, "changed", G_CALLBACK(mn_frameskip), NULL);
	gtk_table_attach(GTK_TABLE(table), w_frameskip, 1, 2, 1, 2, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	l_display_x = gtk_label_new(GetString(STR_DISPLAY_X_CTRL));
	gtk_widget_show(l_display_x);
	gtk_table_attach(GTK_TABLE(table), l_display_x, 0, 1, 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_display_x = gtk_combo_box_entry_new_text();
	gtk_widget_show(w_display_x);
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_x), GetString(STR_SIZE_512_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_x), GetString(STR_SIZE_640_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_x), GetString(STR_SIZE_800_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_x), GetString(STR_SIZE_1024_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_x), GetString(STR_SIZE_MAX_LAB));
	if (dis_width)
		sprintf(str, "%d", dis_width);
	else
		strcpy(str, GetString(STR_SIZE_MAX_LAB));
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w_display_x))), str);
	gtk_table_attach(GTK_TABLE(table), w_display_x, 1, 2, 2, 3, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	l_display_y = gtk_label_new(GetString(STR_DISPLAY_Y_CTRL));
	gtk_widget_show(l_display_y);
	gtk_table_attach(GTK_TABLE(table), l_display_y, 0, 1, 3, 4, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_display_y = gtk_combo_box_entry_new_text();
	gtk_widget_show(w_display_y);
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_y), GetString(STR_SIZE_384_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_y), GetString(STR_SIZE_480_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_y), GetString(STR_SIZE_600_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_y), GetString(STR_SIZE_768_LAB));
	gtk_combo_box_append_text(GTK_COMBO_BOX(w_display_y), GetString(STR_SIZE_MAX_LAB));
	if (dis_height)
		sprintf(str, "%d", dis_height);
	else
		strcpy(str, GetString(STR_SIZE_MAX_LAB));
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w_display_y))), str);
	gtk_table_attach(GTK_TABLE(table), w_display_y, 1, 2, 3, 4, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	make_checkbox(box, STR_GFXACCEL_CTRL, PrefsFindBool("gfxaccel"), G_CALLBACK(tb_gfxaccel));

	make_separator(box);
	make_checkbox(box, STR_NOSOUND_CTRL, "nosound", G_CALLBACK(tb_nosound));
	w_dspdevice_file = make_file_entry(box, STR_DSPDEVICE_FILE_CTRL, "dsp");
	w_mixerdevice_file = make_file_entry(box, STR_MIXERDEVICE_FILE_CTRL, "mixer");

	// SDL scaling settings section
	label = gtk_label_new(GetString(STR_SDL_SCALING));
	markup = g_markup_printf_escaped ("<b>%s</b>", GetString(STR_SDL_SCALING));
	gtk_label_set_markup(GTK_LABEL(label), markup);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	// attach((table), child, left_attach, right_attach, top_attach, bottom_attach, xoptions, yoptions, xpadding, ypadding)
	gtk_table_attach(GTK_TABLE(table), label, 2, 4, 0, 1, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	label = gtk_label_new(GetString(STR_SCALE_FACTOR));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	mag_rate = gtk_entry_new();
	const char *mag_rate_str = PrefsFindString("mag_rate");
	if (!mag_rate_str)
		mag_rate_str = "1.0";
	gtk_entry_set_text(GTK_ENTRY(mag_rate), mag_rate_str);

	gtk_widget_show(mag_rate);
	gtk_table_attach(GTK_TABLE(table), mag_rate, 3, 4, 1, 2, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	scale_nearest = make_checkbox(NULL, STR_SCALE_NEAREST, "scale_nearest", G_CALLBACK(tb_scale_nearest));
	gtk_table_attach(GTK_TABLE(table), scale_nearest, 2, 4, 2, 3, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);
	scale_integer = make_checkbox(NULL, STR_SCALE_INTEGER, "scale_integer", G_CALLBACK(tb_scale_integer));
	gtk_table_attach(GTK_TABLE(table), scale_integer, 2, 4, 3, 4, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	set_graphics_sensitive();
}


/*
 *  "Input" pane
 */

static GtkWidget *w_keycode_file;
static GtkWidget *w_mouse_wheel_lines;

// Set sensitivity of widgets
static void set_input_sensitive(void)
{
	const bool use_keycodes = PrefsFindBool("keycodes");
	gtk_widget_set_sensitive(w_keycode_file, use_keycodes);
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(w_keycode_file), "chooser_button")), use_keycodes);
	gtk_widget_set_sensitive(w_mouse_wheel_lines, PrefsFindInt32("mousewheelmode") == 1);
}

// "Use Raw Keycodes" button toggled
static void tb_keycodes(GtkWidget *widget)
{
	PrefsReplaceBool("keycodes", GTK_TOGGLE_BUTTON(widget)->active);
	set_input_sensitive();
}

// "Mouse Wheel Mode" selected
static void mn_wheelmode(GtkWidget *widget)
{
	PrefsReplaceInt32("mousewheelmode", gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
	set_input_sensitive();
}

// Read settings from widgets and set preferences
static void read_input_settings(void)
{
	const char *str = get_file_entry_path(w_keycode_file);
	if (str && strlen(str))
		PrefsReplaceString("keycodefile", str);
	else
		PrefsRemoveItem("keycodefile");

	PrefsReplaceInt32("mousewheellines", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w_mouse_wheel_lines)));
}

// Create "Input" pane
static void create_input_pane(GtkWidget *top)
{
	GtkWidget *box, *hbox, *menu, *label, *button;
	GtkObject *adj;

	box = make_pane(top, STR_INPUT_PANE_TITLE);

	make_checkbox(box, STR_KEYCODES_CTRL, "keycodes", G_CALLBACK(tb_keycodes));

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(STR_KEYCODES_CTRL));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	const char *str = PrefsFindString("keycodefile");
	if (str == NULL)
		str = "";

	w_keycode_file = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(w_keycode_file), str);
	gtk_widget_show(w_keycode_file);
	gtk_box_pack_start(GTK_BOX(hbox), w_keycode_file, TRUE, TRUE, 0);

	button = make_browse_button(w_keycode_file, false);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(w_keycode_file), "chooser_button", button);

	make_separator(box);

	static const combo_desc options[] = {
		STR_MOUSEWHEELMODE_PAGE_LAB,
		STR_MOUSEWHEELMODE_CURSOR_LAB,
		0
	};
	int wheelmode = PrefsFindInt32("mousewheelmode"), active = 0;
	switch (wheelmode) {
		case 0: active = 0; break;
		case 1: active = 1; break;
	}
	menu = make_option_menu(box, STR_MOUSEWHEELMODE_CTRL, options, G_CALLBACK(mn_wheelmode), active);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(STR_MOUSEWHEELLINES_CTRL));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	adj = gtk_adjustment_new(PrefsFindInt32("mousewheellines"), 1, 1000, 1, 5, 0);
	w_mouse_wheel_lines = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0.0, 0);
	gtk_widget_show(w_mouse_wheel_lines);
	gtk_box_pack_start(GTK_BOX(hbox), w_mouse_wheel_lines, FALSE, FALSE, 0);

	set_input_sensitive();
}


/*
 *  "Serial/Network" pane
 */

static GtkWidget *w_seriala, *w_serialb, *w_ether;

// Read settings from widgets and set preferences
static void read_serial_settings(void)
{
	const char *str;

	str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w_seriala));
	PrefsReplaceString("seriala", str);

	str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w_serialb));
	PrefsReplaceString("serialb", str);

	str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w_ether));
	if (str && strlen(str))
		PrefsReplaceString("ether", str);
	else
		PrefsRemoveItem("ether");
}

// Add names of serial devices
static gint gl_str_cmp(gconstpointer a, gconstpointer b)
{
	return strcmp((char *)a, (char *)b);
}

static GList *add_serial_names(void)
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
			if (strncmp(de->d_name, "cuaa", 4) == 0 || strncmp(de->d_name, "lpt", 3) == 0) {
#elif defined(__NetBSD__)
			if (strncmp(de->d_name, "tty0", 4) == 0 || strncmp(de->d_name, "lpt", 3) == 0) {
#elif defined(sgi)
			if (strncmp(de->d_name, "ttyf", 4) == 0 || strncmp(de->d_name, "plp", 3) == 0) {
#else
			if (false) {
#endif
				char *str = new char[64];
				sprintf(str, "/dev/%s", de->d_name);
				glist = g_list_append(glist, str);
			}
		}
		closedir(d);
	}
	if (glist)
		glist = g_list_sort(glist, gl_str_cmp);
	else
		glist = g_list_append(glist, (void *)"<none>");
	return glist;
}

// Add names of ethernet interfaces
static GList *add_ether_names(void)
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
	if (glist)
		glist = g_list_sort(glist, gl_str_cmp);
	else
		glist = g_list_append(glist, (void *)"<none>");
	return glist;
}

// Create "Serial/Network" pane
static void create_serial_pane(GtkWidget *top)
{
	GtkWidget *box, *table, *label, *combo;
	GList *glist = add_serial_names();

	box = make_pane(top, STR_SERIAL_NETWORK_PANE_TITLE);
	table = make_table(box, 2, 3);

	label = gtk_label_new(GetString(STR_SERPORTA_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);
	label = gtk_label_new(GetString(STR_SERPORTB_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_seriala = gtk_combo_box_entry_new_text();
	gtk_widget_show(w_seriala);
	w_serialb = gtk_combo_box_entry_new_text();
	gtk_widget_show(w_serialb);
	while (glist)
	{
	    gtk_combo_box_append_text(GTK_COMBO_BOX(w_seriala), (gchar *)glist->data);
	    gtk_combo_box_append_text(GTK_COMBO_BOX(w_serialb), (gchar *)glist->data);
	    glist = glist->next;
	}

	const char *str = PrefsFindString("seriala");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w_seriala))), str);
	str = PrefsFindString("serialb");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w_serialb))), str);

	gtk_table_attach(GTK_TABLE(table), w_seriala, 1, 2, 0, 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	gtk_table_attach(GTK_TABLE(table), w_serialb, 1, 2, 1, 2, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);

	label = gtk_label_new(GetString(STR_ETHERNET_IF_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	glist = add_ether_names();
	w_ether = gtk_combo_box_entry_new_text();
	gtk_widget_show(w_ether);
	while (glist)
	{
	    gtk_combo_box_append_text(GTK_COMBO_BOX(w_ether), (gchar *)glist->data);
	    glist = glist->next;
	}
	str = PrefsFindString("ether");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w_ether))), str);
	gtk_table_attach(GTK_TABLE(table), w_ether, 1, 2, 2, 3, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
}


/*
 *  "Memory/Misc" pane
 */

static GtkWidget *w_ramsize;
static GtkWidget *w_rom_file;

// Don't use CPU when idle?
static void tb_idlewait(GtkWidget *widget)
{
	PrefsReplaceBool("idlewait", GTK_TOGGLE_BUTTON(widget)->active);
}

// "Ignore SEGV" button toggled
static void tb_ignoresegv(GtkWidget *widget)
{
	PrefsReplaceBool("ignoresegv", GTK_TOGGLE_BUTTON(widget)->active);
}

// Read settings from widgets and set preferences
static void read_memory_settings(void)
{
	const char *str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w_ramsize));
	PrefsReplaceInt32("ramsize", atoi(str) << 20);

	str = gtk_entry_get_text(GTK_ENTRY(w_rom_file));
	if (str && strlen(str))
		PrefsReplaceString("rom", str);
	else
		PrefsRemoveItem("rom");
}

// Create "Memory/Misc" pane
static void create_memory_pane(GtkWidget *top)
{
	GtkWidget *box, *hbox, *table, *label, *menu;

	box = make_pane(top, STR_MEMORY_MISC_PANE_TITLE);
	table = make_table(box, 2, 5);

	static const combo_desc options[] = {
		STR_RAMSIZE_16MB_LAB,
		STR_RAMSIZE_32MB_LAB,
		STR_RAMSIZE_64MB_LAB,
		STR_RAMSIZE_128MB_LAB,
		STR_RAMSIZE_256MB_LAB,
		STR_RAMSIZE_512MB_LAB,
		STR_RAMSIZE_1024MB_LAB,
		0
	};
	char default_ramsize[16];
	sprintf(default_ramsize, "%d", PrefsFindInt32("ramsize") >> 20);
	w_ramsize = table_make_combobox(table, 0, STR_RAMSIZE_CTRL, default_ramsize, options);

	w_rom_file = table_make_file_entry(table, 1, STR_ROM_FILE_CTRL, "rom");

	make_checkbox(box, STR_IGNORESEGV_CTRL, "ignoresegv", G_CALLBACK(tb_ignoresegv));
	make_checkbox(box, STR_IDLEWAIT_CTRL, "idlewait", G_CALLBACK(tb_idlewait));
}


/*
 *  Read settings from widgets and set preferences
 */

static void read_settings(void)
{
	read_volumes_settings();
	read_graphics_settings();
	read_input_settings();
	read_serial_settings();
	read_memory_settings();
	read_jit_settings();
}


#ifdef STANDALONE_GUI
#include <errno.h>
#include <sys/wait.h>
#include "rpc.h"

/*
 *  Fake unused data and functions
 */

uint8 XPRAM[XPRAM_SIZE];
void MountVolume(void *fh) { }
void FileDiskLayout(loff_t size, uint8 *data, loff_t &start_byte, loff_t &real_size) { }

#if defined __APPLE__ && defined __MACH__
void DarwinSysInit(void) { }
void DarwinSysExit(void) { }
void DarwinAddFloppyPrefs(void) { }
void DarwinAddSerialPrefs(void) { }
bool DarwinCDReadTOC(char *, uint8 *) { }
#endif


/*
 *  Display alert
 */

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
}


/*
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
	display_alert(STR_ERROR_ALERT_TITLE, STR_GUI_ERROR_PREFIX, STR_QUIT_BUTTON, text);
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
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

static void sigchld_handler(int sig, siginfo_t *sip, void *)
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
		char str[256];
		sprintf(str, GetString(STR_NO_B2_EXE_FOUND), g_app_path, strerror(-status));
		ErrorAlert(str);
		status = 1;
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

int main(int argc, char *argv[])
{
	// Init GTK
	gtk_set_locale();
	gtk_init(&argc, &argv);

	// Read preferences
	PrefsInit(0, argc, argv);

	// Show preferences editor
	bool start = PrefsEditor();

	// Exit preferences
	PrefsExit();

	// Transfer control to the executable
	if (start) {
		char gui_connection_path[64];
		sprintf(gui_connection_path, "/org/SheepShaver/GUI/%d", getpid());

		// Catch exits from the child process
		struct sigaction sigchld_sa, old_sigchld_sa;
		sigemptyset(&sigchld_sa.sa_mask);
		sigchld_sa.sa_sigaction = sigchld_handler;
		sigchld_sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
		if (sigaction(SIGCHLD, &sigchld_sa, &old_sigchld_sa) < 0) {
			char str[256];
			sprintf(str, GetString(STR_SIG_INSTALL_ERR), SIGCHLD, strerror(errno));
			ErrorAlert(str);
			return 1;
		}

		// Search and run the SheepShaver executable
		char *p;
		strcpy(g_app_path, argv[0]);
		if ((p = strstr(g_app_path, "SheepShaverGUI.app/Contents/MacOS")) != NULL) {
		    strcpy(p, "SheepShaver.app/Contents/MacOS/SheepShaver");
			if (access(g_app_path, X_OK) < 0) {
				char str[256];
				sprintf(str, GetString(STR_NO_B2_EXE_FOUND), g_app_path, strerror(errno));
				WarningAlert(str);
				strcpy(g_app_path, "/Applications/SheepShaver.app/Contents/MacOS/SheepShaver");
			}
		} else {
			p = strrchr(g_app_path, '/');
			p = p ? p + 1 : g_app_path;
			strcpy(p, "SheepShaver");
		}

		int pid = fork();
		if (pid == 0) {
			D(bug("Trying to execute %s\n", g_app_path));
			execlp(g_app_path, g_app_path, "--gui-connection", gui_connection_path, (char *)NULL);
#ifdef _POSIX_PRIORITY_SCHEDULING
			// XXX get a chance to run the parent process so that to not confuse/upset GTK...
			sched_yield();
#endif
			_exit(-errno);
		}

		// Establish a connection to Basilisk II
		if ((g_gui_connection = rpc_init_server(gui_connection_path)) == NULL) {
			printf("ERROR: failed to initialize GUI-side RPC server connection\n");
			return 1;
		}
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
