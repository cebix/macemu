/*
 *  prefs_editor_linux.cpp - Preferences editor, Linux implementation using GTK+
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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
static void create_graphics_pane(GtkWidget *top);
static void create_input_pane(GtkWidget *top);
static void create_serial_pane(GtkWidget *top);
static void create_memory_pane(GtkWidget *top);
static void create_jit_pane(GtkWidget *top);
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

static char *get_file_entry_path(GtkWidget *entry)
{
	return gtk_entry_get_text(GTK_ENTRY(entry));
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

static GtkWidget *make_checkbox(GtkWidget *top, int label_id, bool active, GtkSignalFunc func)
{
	GtkWidget *button = gtk_check_button_new_with_label(GetString(label_id));
	gtk_widget_show(button);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), active);
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

	char str[512];
	sprintf(str,
		"SheepShaver\nVersion %d.%d\n\n"
		"Copyright (C) 1997-2002 Christian Bauer and Marc Hellwig\n"
		"E-mail: Christian.Bauer@uni-mainz.de\n"
		"http://www.uni-mainz.de/~bauec002/\n\n"
		"SheepShaver comes with ABSOLUTELY NO\n"
		"WARRANTY. This is free software, and\n"
		"you are welcome to redistribute it\n"
		"under the terms of the GNU General\n"
		"Public License.\n",
		VERSION_MAJOR, VERSION_MINOR
	);

	dialog = gtk_dialog_new();
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

// "Zap NVRAM" selected
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
	create_graphics_pane(notebook);
	create_input_pane(notebook);
	create_serial_pane(notebook);
	create_memory_pane(notebook);
	create_jit_pane(notebook);

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
static void mn_boot_any(...) {PrefsReplaceInt32("bootdriver", 0);}
static void mn_boot_cdrom(...) {PrefsReplaceInt32("bootdriver", CDROMRefNum);}

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
	int bootdriver = PrefsFindInt32("bootdriver"), active = 0;
	switch (bootdriver) {
		case 0: active = 0; break;
		case CDROMRefNum: active = 1; break;
	}
	menu = make_option_menu(box, STR_BOOTDRIVER_CTRL, options, active);

	make_checkbox(box, STR_NOCDROM_CTRL, "nocdrom", GTK_SIGNAL_FUNC(tb_nocdrom));
}


/*
 *  "JIT Compiler" pane
 */

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
#if USE_JIT
	bool jit_enabled = PrefsFindBool("jit");
#endif
}

// Create "JIT Compiler" pane
static void create_jit_pane(GtkWidget *top)
{
#if USE_JIT
	GtkWidget *box, *table, *label, *menu;
	char str[32];
	
	box = make_pane(top, STR_JIT_PANE_TITLE);
	make_checkbox(box, STR_JIT_CTRL, "jit", GTK_SIGNAL_FUNC(tb_jit));
	
	set_jit_sensitive();
#endif
}


/*
 *  "Graphics/Sound" pane
 */

static GtkWidget *w_frameskip;

static GtkWidget *w_dspdevice_file, *w_mixerdevice_file;

// "5 Hz".."60Hz" selected
static void mn_5hz(...) {PrefsReplaceInt32("frameskip", 12);}
static void mn_7hz(...) {PrefsReplaceInt32("frameskip", 8);}
static void mn_10hz(...) {PrefsReplaceInt32("frameskip", 6);}
static void mn_15hz(...) {PrefsReplaceInt32("frameskip", 4);}
static void mn_30hz(...) {PrefsReplaceInt32("frameskip", 2);}
static void mn_60hz(...) {PrefsReplaceInt32("frameskip", 1);}

// Video modes
static void tb_w640x480(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("windowmodes", PrefsFindInt32("windowmodes") | 1);
	else
		PrefsReplaceInt32("windowmodes", PrefsFindInt32("windowmodes") & ~1);
}

static void tb_w800x600(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("windowmodes", PrefsFindInt32("windowmodes") | 2);
	else
		PrefsReplaceInt32("windowmodes", PrefsFindInt32("windowmodes") & ~2);
}

static void tb_fs640x480(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") | 1);
	else
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") & ~1);
}

static void tb_fs800x600(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") | 2);
	else
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") & ~2);
}

static void tb_fs1024x768(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") | 4);
	else
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") & ~4);
}

static void tb_fs1152x900(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") | 8);
	else
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") & ~8);
}

static void tb_fs1280x1024(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") | 16);
	else
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") & ~16);
}

static void tb_fs1600x1200(GtkWidget *widget)
{
	if (GTK_TOGGLE_BUTTON(widget)->active)
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") | 32);
	else
		PrefsReplaceInt32("screenmodes", PrefsFindInt32("screenmodes") & ~32);
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

// Read settings from widgets and set preferences
static void read_graphics_settings(void)
{
	PrefsReplaceString("dsp", get_file_entry_path(w_dspdevice_file));
	PrefsReplaceString("mixer", get_file_entry_path(w_mixerdevice_file));
}

// Create "Graphics/Sound" pane
static void create_graphics_pane(GtkWidget *top)
{
	GtkWidget *box, *vbox, *frame;

	box = make_pane(top, STR_GRAPHICS_SOUND_PANE_TITLE);

	static const opt_desc options[] = {
		{STR_REF_5HZ_LAB, GTK_SIGNAL_FUNC(mn_5hz)},
		{STR_REF_7_5HZ_LAB, GTK_SIGNAL_FUNC(mn_7hz)},
		{STR_REF_10HZ_LAB, GTK_SIGNAL_FUNC(mn_10hz)},
		{STR_REF_15HZ_LAB, GTK_SIGNAL_FUNC(mn_15hz)},
		{STR_REF_30HZ_LAB, GTK_SIGNAL_FUNC(mn_30hz)},
		{STR_REF_60HZ_LAB, GTK_SIGNAL_FUNC(mn_60hz)},
		{0, NULL}
	};
	int frameskip = PrefsFindInt32("frameskip"), active = 0;
	switch (frameskip) {
		case 12: active = 0; break;
		case 8: active = 1; break;
		case 6: active = 2; break;
		case 4: active = 3; break;
		case 2: active = 4; break;
		case 1: active = 5; break;
	}
	w_frameskip = make_option_menu(box, STR_FRAMESKIP_CTRL, options, active);

	frame = gtk_frame_new (GetString(STR_VIDEO_MODE_CTRL));
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	make_checkbox(vbox, STR_W_640x480_CTRL, PrefsFindInt32("windowmodes") & 1, GTK_SIGNAL_FUNC(tb_w640x480));
	make_checkbox(vbox, STR_W_800x600_CTRL, PrefsFindInt32("windowmodes") & 2, GTK_SIGNAL_FUNC(tb_w800x600));
	make_checkbox(vbox, STR_640x480_CTRL, PrefsFindInt32("screenmodes") & 1, GTK_SIGNAL_FUNC(tb_fs640x480));
	make_checkbox(vbox, STR_800x600_CTRL, PrefsFindInt32("screenmodes") & 2, GTK_SIGNAL_FUNC(tb_fs800x600));
	make_checkbox(vbox, STR_1024x768_CTRL, PrefsFindInt32("screenmodes") & 4, GTK_SIGNAL_FUNC(tb_fs1024x768));
	make_checkbox(vbox, STR_1152x900_CTRL, PrefsFindInt32("screenmodes") & 8, GTK_SIGNAL_FUNC(tb_fs1152x900));
	make_checkbox(vbox, STR_1280x1024_CTRL, PrefsFindInt32("screenmodes") & 16, GTK_SIGNAL_FUNC(tb_fs1280x1024));
	make_checkbox(vbox, STR_1600x1200_CTRL, PrefsFindInt32("screenmodes") & 32, GTK_SIGNAL_FUNC(tb_fs1600x1200));

	make_separator(box);
	make_checkbox(box, STR_NOSOUND_CTRL, "nosound", GTK_SIGNAL_FUNC(tb_nosound));
	w_dspdevice_file = make_entry(box, STR_DSPDEVICE_FILE_CTRL, "dsp");
	w_mixerdevice_file = make_entry(box, STR_MIXERDEVICE_FILE_CTRL, "mixer");

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
	gtk_widget_set_sensitive(w_keycode_file, PrefsFindBool("keycodes"));
	gtk_widget_set_sensitive(w_mouse_wheel_lines, PrefsFindInt32("mousewheelmode") == 1);
}

// "Use Raw Keycodes" button toggled
static void tb_keycodes(GtkWidget *widget)
{
	PrefsReplaceBool("keycodes", GTK_TOGGLE_BUTTON(widget)->active);
	set_input_sensitive();
}

// "Mouse Wheel Mode" selected
static void mn_wheel_page(...) {PrefsReplaceInt32("mousewheelmode", 0); set_input_sensitive();}
static void mn_wheel_cursor(...) {PrefsReplaceInt32("mousewheelmode", 1); set_input_sensitive();}

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
	GtkWidget *box, *hbox, *menu, *label;
	GtkObject *adj;

	box = make_pane(top, STR_INPUT_PANE_TITLE);

	make_checkbox(box, STR_KEYCODES_CTRL, "keycodes", GTK_SIGNAL_FUNC(tb_keycodes));
	w_keycode_file = make_entry(box, STR_KEYCODE_FILE_CTRL, "keycodefile");

	make_separator(box);

	static const opt_desc options[] = {
		{STR_MOUSEWHEELMODE_PAGE_LAB, GTK_SIGNAL_FUNC(mn_wheel_page)},
		{STR_MOUSEWHEELMODE_CURSOR_LAB, GTK_SIGNAL_FUNC(mn_wheel_cursor)},
		{0, NULL}
	};
	int wheelmode = PrefsFindInt32("mousewheelmode"), active = 0;
	switch (wheelmode) {
		case 0: active = 0; break;
		case 1: active = 1; break;
	}
	menu = make_option_menu(box, STR_MOUSEWHEELMODE_CTRL, options, active);

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
			if (strncmp(de->d_name, "ttyS", 4) == 0 || strncmp(de->d_name, "lp", 2) == 0) {
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
				if (ioctl(s, SIOCGIFHWADDR, &req) == 0 && req.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
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

	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	const char *str = PrefsFindString("seriala");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 0, 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	w_seriala = GTK_COMBO(combo)->entry;

	label = gtk_label_new(GetString(STR_SERPORTB_CTRL));
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

// "Ignore SEGV" button toggled
static void tb_ignoresegv(GtkWidget *widget)
{
	PrefsReplaceBool("ignoresegv", GTK_TOGGLE_BUTTON(widget)->active);
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
}

// Create "Memory/Misc" pane
static void create_memory_pane(GtkWidget *top)
{
	GtkWidget *box, *vbox, *hbox, *hbox2, *label, *scale;

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
	max = 256;
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

	w_rom_file = make_entry(box, STR_ROM_FILE_CTRL, "rom");

	make_checkbox(box, STR_IGNORESEGV_CTRL, "ignoresegv", GTK_SIGNAL_FUNC(tb_ignoresegv));
}


/*
 *  Read settings from widgets and set preferences
 */

static void read_settings(void)
{
	read_volumes_settings();
	read_graphics_settings();
	read_serial_settings();
	read_memory_settings();
	read_jit_settings();
}
