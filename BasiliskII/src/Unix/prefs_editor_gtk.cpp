/*
 *  prefs_editor_gtk.cpp - Preferences editor, Unix implementation using GTK+
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

#include "user_strings.h"
#include "version.h"
#include "cdrom.h"
#include "xpram.h"
#include "prefs.h"
#include "prefs_editor.h"


// Global variables
static GtkWidget *win;				// Preferences window
static bool start_clicked = true;	// Return value of PrefsEditor() function


// Prototypes
static void create_volumes_pane(GtkWidget *top);
static void create_scsi_pane(GtkWidget *top);
static void create_graphics_pane(GtkWidget *top);
static void create_serial_pane(GtkWidget *top);
static void create_memory_pane(GtkWidget *top);
static void read_settings(void);


/*
 *  Utility functions
 */

struct opt_desc {
	int label_id;
	GtkSignalFunc func;
};

static void add_menu_item(GtkWidget *menu, int label_id, GtkSignalFunc func)
{
	GtkWidget *item = gtk_menu_item_new_with_label(GetString(label_id));
	gtk_widget_show(item);
	gtk_signal_connect(GTK_OBJECT(item), "activate", func, NULL);
	gtk_menu_append(GTK_MENU(menu), item);
}

static GtkWidget *make_pane(GtkWidget *notebook, int title_id)
{
	GtkWidget *frame, *label, *box;

	frame = gtk_frame_new(NULL);
	gtk_widget_show(frame);
	gtk_container_border_width(GTK_CONTAINER(frame), 4);

	label = gtk_label_new(GetString(title_id));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);

	box = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_container_set_border_width(GTK_CONTAINER(box), 4);
	gtk_container_add(GTK_CONTAINER(frame), box);
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
		gtk_signal_connect_object(GTK_OBJECT(button), "clicked", buttons->func, NULL);
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

static GtkWidget *make_option_menu(GtkWidget *top, int label_id, const opt_desc *options, int active)
{
	GtkWidget *box, *label, *opt, *menu;

	box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(top), box, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	opt = gtk_option_menu_new();
	gtk_widget_show(opt);
	menu = gtk_menu_new();

	while (options->label_id) {
		add_menu_item(menu, options->label_id, options->func);
		options++;
	}
	gtk_menu_set_active(GTK_MENU(menu), active);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	return menu;
}

static GtkWidget *make_entry(GtkWidget *top, int label_id, const char *prefs_item)
{
	GtkWidget *box, *label, *entry;

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
	gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);
	return entry;
}

static GtkWidget *make_checkbox(GtkWidget *top, int label_id, const char *prefs_item, GtkSignalFunc func)
{
	GtkWidget *button = gtk_check_button_new_with_label(GetString(label_id));
	gtk_widget_show(button);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), PrefsFindBool(prefs_item));
	gtk_signal_connect(GTK_OBJECT(button), "toggled", func, button);
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
static void dl_quit(GtkWidget *dialog)
{
	gtk_widget_destroy(dialog);
}

// "About" selected
static void mn_about(...)
{
	GtkWidget *dialog, *label, *button;

	char str[256];
	sprintf(str, GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	strncat(str, "\n", 255);
	strncat(str, GetString(STR_ABOUT_TEXT2), 255);

	dialog = gtk_dialog_new();
	gtk_widget_set_usize(GTK_WIDGET(dialog), strlen(GetString(STR_ABOUT_TEXT2)) + 200, 120);
	gtk_window_set_title(GTK_WINDOW(dialog), GetString(STR_ABOUT_TITLE));
	gtk_container_border_width(GTK_CONTAINER(dialog), 5);
	gtk_widget_set_uposition(GTK_WIDGET(dialog), 100, 150);

	label = gtk_label_new(str);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, TRUE, 0);

	button = gtk_button_new_with_label(GetString(STR_OK_BUTTON));
	gtk_widget_show(button);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(dl_quit), GTK_OBJECT(dialog));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), button, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(dialog);
}

// "Zap PRAM" selected
static void mn_zap_pram(...)
{
	ZapPRAM();
}

// Menu item descriptions
static GtkItemFactoryEntry menu_items[] = {
	{(gchar *)GetString(STR_PREFS_MENU_FILE_GTK),		NULL,			NULL,							0, "<Branch>"},
	{(gchar *)GetString(STR_PREFS_ITEM_START_GTK),		NULL,			GTK_SIGNAL_FUNC(cb_start),		0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_ZAP_PRAM_GTK),	NULL,			GTK_SIGNAL_FUNC(mn_zap_pram),	0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_SEPL_GTK),		NULL,			NULL,							0, "<Separator>"},
	{(gchar *)GetString(STR_PREFS_ITEM_QUIT_GTK),		"<control>Q",	GTK_SIGNAL_FUNC(cb_quit),		0, NULL},
	{(gchar *)GetString(STR_HELP_MENU_GTK),				NULL,			NULL,							0, "<LastBranch>"},
	{(gchar *)GetString(STR_HELP_ITEM_ABOUT_GTK),		NULL,			GTK_SIGNAL_FUNC(mn_about),		0, NULL}
};

bool PrefsEditor(void)
{
	// Create window
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), GetString(STR_PREFS_TITLE));
	gtk_signal_connect(GTK_OBJECT(win), "delete_event", GTK_SIGNAL_FUNC(window_closed), NULL);
	gtk_signal_connect(GTK_OBJECT(win), "destroy", GTK_SIGNAL_FUNC(window_destroyed), NULL);

	// Create window contents
	GtkWidget *box = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_container_add(GTK_CONTAINER(win), box);

	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkItemFactory *item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items(item_factory, sizeof(menu_items) / sizeof(menu_items[0]), menu_items, NULL);
	gtk_accel_group_attach(accel_group, GTK_OBJECT(win));
	GtkWidget *menu_bar = gtk_item_factory_get_widget(item_factory, "<main>");
	gtk_widget_show(menu_bar);
	gtk_box_pack_start(GTK_BOX(box), menu_bar, FALSE, TRUE, 0);

	GtkWidget *notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(box), notebook, TRUE, TRUE, 0);

	create_volumes_pane(notebook);
	create_scsi_pane(notebook);
	create_graphics_pane(notebook);
	create_serial_pane(notebook);
	create_memory_pane(notebook);

	static const opt_desc buttons[] = {
		{STR_START_BUTTON, GTK_SIGNAL_FUNC(cb_start)},
		{STR_QUIT_BUTTON, GTK_SIGNAL_FUNC(cb_quit)},
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

struct file_req_assoc {
	file_req_assoc(GtkWidget *r, GtkWidget *e) : req(r), entry(e) {}
	GtkWidget *req;
	GtkWidget *entry;
};

// Volume selected for addition
static void add_volume_ok(GtkWidget *button, file_req_assoc *assoc)
{
	char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(assoc->req));
	gtk_clist_append(GTK_CLIST(volume_list), &file);
	gtk_widget_destroy(assoc->req);
	delete assoc;
}

// Volume selected for creation
static void create_volume_ok(GtkWidget *button, file_req_assoc *assoc)
{
	char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(assoc->req));

	char *str = gtk_entry_get_text(GTK_ENTRY(assoc->entry));
	int size = atoi(str);

	char cmd[1024];
	sprintf(cmd, "dd if=/dev/zero \"of=%s\" bs=1024k count=%d", file, size);
	int ret = system(cmd);
	if (ret == 0)
		gtk_clist_append(GTK_CLIST(volume_list), &file);
	gtk_widget_destroy(GTK_WIDGET(assoc->req));
	delete assoc;
}

// "Add Volume" button clicked
static void cb_add_volume(...)
{
	GtkWidget *req = gtk_file_selection_new(GetString(STR_ADD_VOLUME_TITLE));
	gtk_signal_connect_object(GTK_OBJECT(req), "delete_event", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(req));
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(req)->ok_button), "clicked", GTK_SIGNAL_FUNC(add_volume_ok), new file_req_assoc(req, NULL));
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(req)->cancel_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(req));
	gtk_widget_show(req);
}

// "Create Hardfile" button clicked
static void cb_create_volume(...)
{
	GtkWidget *req = gtk_file_selection_new(GetString(STR_CREATE_VOLUME_TITLE));

	GtkWidget *box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	GtkWidget *label = gtk_label_new(GetString(STR_HARDFILE_SIZE_CTRL));
	gtk_widget_show(label);
	GtkWidget *entry = gtk_entry_new();
	gtk_widget_show(entry);
	char str[32];
	sprintf(str, "%d", 40);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(req)->main_vbox), box, FALSE, FALSE, 0);

	gtk_signal_connect_object(GTK_OBJECT(req), "delete_event", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(req));
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(req)->ok_button), "clicked", GTK_SIGNAL_FUNC(create_volume_ok), new file_req_assoc(req, entry));
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(req)->cancel_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(req));
	gtk_widget_show(req);
}

// "Remove Volume" button clicked
static void cb_remove_volume(...)
{
	gtk_clist_remove(GTK_CLIST(volume_list), selected_volume);
}

// "Boot From" selected
static void mn_boot_any(...) {PrefsReplaceInt16("bootdriver", 0);}
static void mn_boot_cdrom(...) {PrefsReplaceInt16("bootdriver", CDROMRefNum);}

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
	gtk_signal_connect(GTK_OBJECT(volume_list), "select_row", GTK_SIGNAL_FUNC(cl_selected), NULL);
	char *str;
	int32 index = 0;
	while ((str = (char *)PrefsFindString("disk", index++)) != NULL)
		gtk_clist_append(GTK_CLIST(volume_list), &str);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), volume_list);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
	selected_volume = 0;

	static const opt_desc buttons[] = {
		{STR_ADD_VOLUME_BUTTON, GTK_SIGNAL_FUNC(cb_add_volume)},
		{STR_CREATE_VOLUME_BUTTON, GTK_SIGNAL_FUNC(cb_create_volume)},
		{STR_REMOVE_VOLUME_BUTTON, GTK_SIGNAL_FUNC(cb_remove_volume)},
		{0, NULL},
	};
	make_button_box(box, 0, buttons);
	make_separator(box);

	w_extfs = make_entry(box, STR_EXTFS_CTRL, "extfs");

	static const opt_desc options[] = {
		{STR_BOOT_ANY_LAB, GTK_SIGNAL_FUNC(mn_boot_any)},
		{STR_BOOT_CDROM_LAB, GTK_SIGNAL_FUNC(mn_boot_cdrom)},
		{0, NULL}
	};
	int bootdriver = PrefsFindInt16("bootdriver"), active = 0;
	switch (bootdriver) {
		case 0: active = 0; break;
		case CDROMRefNum: active = 1; break;
	}
	menu = make_option_menu(box, STR_BOOTDRIVER_CTRL, options, active);

	make_checkbox(box, STR_NOCDROM_CTRL, "nocdrom", GTK_SIGNAL_FUNC(tb_nocdrom));
}


/*
 *  "SCSI" pane
 */

static GtkWidget *w_scsi[7];

// Read settings from widgets and set preferences
static void read_scsi_settings(void)
{
	for (int id=0; id<7; id++) {
		char prefs_name[32];
		sprintf(prefs_name, "scsi%d", id);
		const char *str = gtk_entry_get_text(GTK_ENTRY(w_scsi[id]));
		if (str && strlen(str))
			PrefsReplaceString(prefs_name, str);
		else
			PrefsRemoveItem(prefs_name);
	}
}

// Create "SCSI" pane
static void create_scsi_pane(GtkWidget *top)
{
	GtkWidget *box;

	box = make_pane(top, STR_SCSI_PANE_TITLE);

	for (int id=0; id<7; id++) {
		char prefs_name[32];
		sprintf(prefs_name, "scsi%d", id);
		w_scsi[id] = make_entry(box, STR_SCSI_ID_0 + id, prefs_name);
	}
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

#if ENABLE_FBDEV_DGA
static GtkWidget *w_fbdev_name, *w_fbdevice_file;
static GtkWidget *l_fbdev_name, *l_fbdevice_file;
static char fbdev_name[256];
#endif

// Hide/show graphics widgets
static void hide_show_graphics_widgets(void)
{
	switch (display_type) {
		case DISPLAY_WINDOW:
			gtk_widget_show(w_frameskip); gtk_widget_show(l_frameskip);
#if ENABLE_FBDEV_DGA
			gtk_widget_show(w_display_x); gtk_widget_show(l_display_x);
			gtk_widget_show(w_display_y); gtk_widget_show(l_display_y);
			gtk_widget_hide(w_fbdev_name); gtk_widget_hide(l_fbdev_name);
#endif
			break;
		case DISPLAY_SCREEN:
			gtk_widget_hide(w_frameskip); gtk_widget_hide(l_frameskip);
#if ENABLE_FBDEV_DGA
			gtk_widget_hide(w_display_x); gtk_widget_hide(l_display_x);
			gtk_widget_hide(w_display_y); gtk_widget_hide(l_display_y);
			gtk_widget_show(w_fbdev_name); gtk_widget_show(l_fbdev_name);
#endif
			break;
	}
}

// "Window" video type selected
static void mn_window(...)
{
	display_type = DISPLAY_WINDOW;
	hide_show_graphics_widgets();
}

// "Fullscreen" video type selected
static void mn_fullscreen(...)
{
	display_type = DISPLAY_SCREEN;
	hide_show_graphics_widgets();
}

// "5 Hz".."60Hz" selected
static void mn_5hz(...) {PrefsReplaceInt32("frameskip", 12);}
static void mn_7hz(...) {PrefsReplaceInt32("frameskip", 8);}
static void mn_10hz(...) {PrefsReplaceInt32("frameskip", 6);}
static void mn_15hz(...) {PrefsReplaceInt32("frameskip", 4);}
static void mn_30hz(...) {PrefsReplaceInt32("frameskip", 2);}
static void mn_60hz(...) {PrefsReplaceInt32("frameskip", 1);}

// "Disable Sound Output" button toggled
static void tb_nosound(GtkWidget *widget)
{
	PrefsReplaceBool("nosound", GTK_TOGGLE_BUTTON(widget)->active);
}

// Read graphics preferences
static void parse_graphics_prefs(void)
{
	display_type = DISPLAY_WINDOW;
	dis_width = 512;
	dis_height = 384;
#if ENABLE_FBDEV_DGA
	fbdev_name[0] = 0;
#endif

	const char *str = PrefsFindString("screen");
	if (str) {
		if (sscanf(str, "win/%d/%d", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_WINDOW;
#if ENABLE_FBDEV_DGA
		else if (sscanf(str, "dga/%255s", fbdev_name) == 1)
#else
		else if (sscanf(str, "dga/%d/%d", &dis_width, &dis_height) == 2)
#endif
			display_type = DISPLAY_SCREEN;
	}
}

// Read settings from widgets and set preferences
static void read_graphics_settings(void)
{
	const char *str;

	str = gtk_entry_get_text(GTK_ENTRY(w_display_x));
	dis_width = atoi(str);

	str = gtk_entry_get_text(GTK_ENTRY(w_display_y));
	dis_height = atoi(str);

	char pref[256];
	switch (display_type) {
		case DISPLAY_WINDOW:
			sprintf(pref, "win/%d/%d", dis_width, dis_height);
			break;
		case DISPLAY_SCREEN:
#if ENABLE_FBDEV_DGA
			str = gtk_entry_get_text(GTK_ENTRY(w_fbdev_name));
			sprintf(pref, "dga/%s", str);
#else
			sprintf(pref, "dga/%d/%d", dis_width, dis_height);
#endif
			break;
		default:
			PrefsRemoveItem("screen");
			return;
	}
	PrefsReplaceString("screen", pref);
}

// Create "Graphics/Sound" pane
static void create_graphics_pane(GtkWidget *top)
{
	GtkWidget *box, *table, *label, *opt, *menu, *combo;
	char str[32];

	parse_graphics_prefs();

	box = make_pane(top, STR_GRAPHICS_SOUND_PANE_TITLE);
	table = make_table(box, 2, 5);

	label = gtk_label_new(GetString(STR_VIDEO_TYPE_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	opt = gtk_option_menu_new();
	gtk_widget_show(opt);
	menu = gtk_menu_new();
	add_menu_item(menu, STR_WINDOW_LAB, GTK_SIGNAL_FUNC(mn_window));
	add_menu_item(menu, STR_FULLSCREEN_LAB, GTK_SIGNAL_FUNC(mn_fullscreen));
	switch (display_type) {
		case DISPLAY_WINDOW:
			gtk_menu_set_active(GTK_MENU(menu), 0);
			break;
		case DISPLAY_SCREEN:
			gtk_menu_set_active(GTK_MENU(menu), 1);
			break;
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
	gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 0, 1, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	l_frameskip = gtk_label_new(GetString(STR_FRAMESKIP_CTRL));
	gtk_widget_show(l_frameskip);
	gtk_table_attach(GTK_TABLE(table), l_frameskip, 0, 1, 1, 2, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_frameskip = gtk_option_menu_new();
	gtk_widget_show(w_frameskip);
	menu = gtk_menu_new();
	add_menu_item(menu, STR_REF_5HZ_LAB, GTK_SIGNAL_FUNC(mn_5hz));
	add_menu_item(menu, STR_REF_7_5HZ_LAB, GTK_SIGNAL_FUNC(mn_7hz));
	add_menu_item(menu, STR_REF_10HZ_LAB, GTK_SIGNAL_FUNC(mn_10hz));
	add_menu_item(menu, STR_REF_15HZ_LAB, GTK_SIGNAL_FUNC(mn_15hz));
	add_menu_item(menu, STR_REF_30HZ_LAB, GTK_SIGNAL_FUNC(mn_30hz));
	add_menu_item(menu, STR_REF_60HZ_LAB, GTK_SIGNAL_FUNC(mn_60hz));
	int frameskip = PrefsFindInt32("frameskip");
	switch (frameskip) {
		case 12:
			gtk_menu_set_active(GTK_MENU(menu), 0);
			break;
		case 8:
			gtk_menu_set_active(GTK_MENU(menu), 1);
			break;
		case 6:
			gtk_menu_set_active(GTK_MENU(menu), 2);
			break;
		case 4:
			gtk_menu_set_active(GTK_MENU(menu), 3);
			break;
		case 2:
			gtk_menu_set_active(GTK_MENU(menu), 4);
			break;
		case 1:
			gtk_menu_set_active(GTK_MENU(menu), 5);
			break;
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(w_frameskip), menu);
	gtk_table_attach(GTK_TABLE(table), w_frameskip, 1, 2, 1, 2, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);

	l_display_x = gtk_label_new(GetString(STR_DISPLAY_X_CTRL));
	gtk_widget_show(l_display_x);
	gtk_table_attach(GTK_TABLE(table), l_display_x, 0, 1, 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	combo = gtk_combo_new();
	gtk_widget_show(combo);
	GList *glist1 = NULL;
	glist1 = g_list_append(glist1, (void *)GetString(STR_SIZE_512_LAB));
	glist1 = g_list_append(glist1, (void *)GetString(STR_SIZE_640_LAB));
	glist1 = g_list_append(glist1, (void *)GetString(STR_SIZE_800_LAB));
	glist1 = g_list_append(glist1, (void *)GetString(STR_SIZE_1024_LAB));
	glist1 = g_list_append(glist1, (void *)GetString(STR_SIZE_MAX_LAB));
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist1);
	if (dis_width)
		sprintf(str, "%d", dis_width);
	else
		strcpy(str, GetString(STR_SIZE_MAX_LAB));
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 2, 3, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);
	w_display_x = GTK_COMBO(combo)->entry;

	l_display_y = gtk_label_new(GetString(STR_DISPLAY_Y_CTRL));
	gtk_widget_show(l_display_y);
	gtk_table_attach(GTK_TABLE(table), l_display_y, 0, 1, 3, 4, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	combo = gtk_combo_new();
	gtk_widget_show(combo);
	GList *glist2 = NULL;
	glist2 = g_list_append(glist2, (void *)GetString(STR_SIZE_384_LAB));
	glist2 = g_list_append(glist2, (void *)GetString(STR_SIZE_480_LAB));
	glist2 = g_list_append(glist2, (void *)GetString(STR_SIZE_600_LAB));
	glist2 = g_list_append(glist2, (void *)GetString(STR_SIZE_768_LAB));
	glist2 = g_list_append(glist2, (void *)GetString(STR_SIZE_MAX_LAB));
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist2);
	if (dis_height)
		sprintf(str, "%d", dis_height);
	else
		strcpy(str, GetString(STR_SIZE_MAX_LAB));
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 3, 4, (GtkAttachOptions)GTK_FILL, (GtkAttachOptions)0, 4, 4);
	w_display_y = GTK_COMBO(combo)->entry;

#if ENABLE_FBDEV_DGA
	l_fbdev_name = gtk_label_new(GetString(STR_FBDEV_NAME_CTRL));
	gtk_widget_show(l_fbdev_name);
	gtk_table_attach(GTK_TABLE(table), l_fbdev_name, 0, 1, 4, 5, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_fbdev_name = gtk_entry_new();
	gtk_widget_show(w_fbdev_name);
	gtk_entry_set_text(GTK_ENTRY(w_fbdev_name), fbdev_name); 
	gtk_table_attach(GTK_TABLE(table), w_fbdev_name, 1, 2, 4, 5, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	w_fbdevice_file = make_entry(box, STR_FBDEVICE_FILE_CTRL, "fbdevicefile");
#endif

	make_checkbox(box, STR_NOSOUND_CTRL, "nosound", GTK_SIGNAL_FUNC(tb_nosound));

	hide_show_graphics_widgets();
}


/*
 *  "Serial/Network" pane
 */

static GtkWidget *w_seriala, *w_serialb, *w_ether;

// Read settings from widgets and set preferences
static void read_serial_settings(void)
{
	const char *str;

	str = gtk_entry_get_text(GTK_ENTRY(w_seriala));
	PrefsReplaceString("seriala", str);

	str = gtk_entry_get_text(GTK_ENTRY(w_serialb));
	PrefsReplaceString("serialb", str);

	str = gtk_entry_get_text(GTK_ENTRY(w_ether));
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
		g_list_sort(glist, gl_str_cmp);
	else
		glist = g_list_append(glist, (void *)GetString(STR_NONE_LAB));
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
	if (glist)
		g_list_sort(glist, gl_str_cmp);
	else
		glist = g_list_append(glist, (void *)GetString(STR_NONE_LAB));
	return glist;
}

// Create "Serial/Network" pane
static void create_serial_pane(GtkWidget *top)
{
	GtkWidget *box, *table, *label, *combo;
	GList *glist = add_serial_names();

	box = make_pane(top, STR_SERIAL_NETWORK_PANE_TITLE);
	table = make_table(box, 2, 3);

	label = gtk_label_new(GetString(STR_SERIALA_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	const char *str = PrefsFindString("seriala");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 0, 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	w_seriala = GTK_COMBO(combo)->entry;

	label = gtk_label_new(GetString(STR_SERIALB_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	str = PrefsFindString("serialb");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 1, 2, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	w_serialb = GTK_COMBO(combo)->entry;

	label = gtk_label_new(GetString(STR_ETHERNET_IF_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	glist = add_ether_names();
	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	str = PrefsFindString("ether");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 2, 3, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	w_ether = GTK_COMBO(combo)->entry;
}


/*
 *  "Memory/Misc" pane
 */

static GtkObject *w_ramsize_adj;
static GtkWidget *w_rom_file;
static GtkWidget *w_keycode_file;

// Model ID selected
static void mn_modelid_5(...) {PrefsReplaceInt32("modelid", 5);}
static void mn_modelid_14(...) {PrefsReplaceInt32("modelid", 14);}

// CPU/FPU type
static void mn_cpu_68020(...) {PrefsReplaceInt32("cpu", 2); PrefsReplaceBool("fpu", false);}
static void mn_cpu_68020_fpu(...) {PrefsReplaceInt32("cpu", 2); PrefsReplaceBool("fpu", true);}
static void mn_cpu_68030(...) {PrefsReplaceInt32("cpu", 3); PrefsReplaceBool("fpu", false);}
static void mn_cpu_68030_fpu(...) {PrefsReplaceInt32("cpu", 3); PrefsReplaceBool("fpu", true);}
static void mn_cpu_68040(...) {PrefsReplaceInt32("cpu", 4); PrefsReplaceBool("fpu", true);}

// "Use Raw Keycodes" button toggled
static void tb_keycodes(GtkWidget *widget)
{
	PrefsReplaceBool("keycodes", GTK_TOGGLE_BUTTON(widget)->active);
}

// Read settings from widgets and set preferences
static void read_memory_settings(void)
{
	PrefsReplaceInt32("ramsize", int(GTK_ADJUSTMENT(w_ramsize_adj)->value) << 20);

	const char *str = gtk_entry_get_text(GTK_ENTRY(w_rom_file));
	if (str && strlen(str))
		PrefsReplaceString("rom", str);
	else
		PrefsRemoveItem("rom");

	str = gtk_entry_get_text(GTK_ENTRY(w_keycode_file));
	if (str && strlen(str))
		PrefsReplaceString("keycodefile", str);
	else
		PrefsRemoveItem("keycodefile");
}

// Create "Memory/Misc" pane
static void create_memory_pane(GtkWidget *top)
{
	GtkWidget *box, *vbox, *hbox, *hbox2, *label, *scale, *menu;

	box = make_pane(top, STR_MEMORY_MISC_PANE_TITLE);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);

	label = gtk_label_new(GetString(STR_RAMSIZE_SLIDER));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox);

	gfloat min, max;
	min = 1;
	max = 1024;
	w_ramsize_adj = gtk_adjustment_new(min, min, max, 1, 16, 0);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(w_ramsize_adj), PrefsFindInt32("ramsize") >> 20);

	scale = gtk_hscale_new(GTK_ADJUSTMENT(w_ramsize_adj));
	gtk_widget_show(scale);
	gtk_scale_set_digits(GTK_SCALE(scale), 0);
	gtk_box_pack_start(GTK_BOX(vbox), scale, TRUE, TRUE, 0);

	hbox2 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox2);

	char val[32];
	sprintf(val, GetString(STR_RAMSIZE_FMT), int(min));
	label = gtk_label_new(val);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);

	sprintf(val, GetString(STR_RAMSIZE_FMT), int(max));
	label = gtk_label_new(val);
	gtk_widget_show(label);
	gtk_box_pack_end(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	static const opt_desc model_options[] = {
		{STR_MODELID_5_LAB, GTK_SIGNAL_FUNC(mn_modelid_5)},
		{STR_MODELID_14_LAB, GTK_SIGNAL_FUNC(mn_modelid_14)},
		{0, NULL}
	};
	int modelid = PrefsFindInt32("modelid"), active = 0;
	switch (modelid) {
		case 5: active = 0; break;
		case 14: active = 1; break;
	}
	make_option_menu(box, STR_MODELID_CTRL, model_options, active);

	static const opt_desc cpu_options[] = {
		{STR_CPU_68020_LAB, GTK_SIGNAL_FUNC(mn_cpu_68020)},
		{STR_CPU_68020_FPU_LAB, GTK_SIGNAL_FUNC(mn_cpu_68020_fpu)},
		{STR_CPU_68030_LAB, GTK_SIGNAL_FUNC(mn_cpu_68030)},
		{STR_CPU_68030_FPU_LAB, GTK_SIGNAL_FUNC(mn_cpu_68030_fpu)},
		{STR_CPU_68040_LAB, GTK_SIGNAL_FUNC(mn_cpu_68040)},
		{0, NULL}
	};
	int cpu = PrefsFindInt32("cpu");
	bool fpu = PrefsFindBool("fpu");
	active = 0;
	switch (cpu) {
		case 2: active = fpu ? 1 : 0; break;
		case 3: active = fpu ? 3 : 2; break;
		case 4: active = 4;
	}
	make_option_menu(box, STR_CPU_CTRL, cpu_options, active);

	w_rom_file = make_entry(box, STR_ROM_FILE_CTRL, "rom");

	make_checkbox(box, STR_KEYCODES_CTRL, "keycodes", GTK_SIGNAL_FUNC(tb_keycodes));
	w_keycode_file = make_entry(box, STR_KEYCODE_FILE_CTRL, "keycodefile");
}


/*
 *  Read settings from widgets and set preferences
 */

static void read_settings(void)
{
	read_volumes_settings();
	read_scsi_settings();
	read_graphics_settings();
	read_serial_settings();
	read_memory_settings();
}
