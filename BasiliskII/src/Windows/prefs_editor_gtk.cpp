/*
 *  prefs_editor_gtk.cpp - Preferences editor, Unix implementation using GTK+
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

#include "sysdeps.h"

#include <gtk/gtk.h>
#include <stdlib.h>

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
static void create_input_pane(GtkWidget *top);
static void create_serial_pane(GtkWidget *top);
static void create_ethernet_pane(GtkWidget *top);
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

struct combo_desc {
	int label_id;
};

struct file_req_assoc {
	file_req_assoc(GtkWidget *r, GtkWidget *e) : req(r), entry(e) {}
	GtkWidget *req;
	GtkWidget *entry;
};

static void cb_browse_ok(GtkWidget *button, file_req_assoc *assoc)
{
	gchar *file = (char *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(assoc->req));
	gtk_entry_set_text(GTK_ENTRY(assoc->entry), file);
	gtk_widget_destroy(assoc->req);
	delete assoc;
}

static void cb_browse(GtkWidget *widget, void *user_data)
{
	GtkWidget *req = gtk_file_selection_new(GetString(STR_BROWSE_TITLE));
	gtk_signal_connect_object(GTK_OBJECT(req), "delete_event", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(req));
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(req)->ok_button), "clicked", GTK_SIGNAL_FUNC(cb_browse_ok), new file_req_assoc(req, (GtkWidget *)user_data));
	gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(req)->cancel_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(req));
	gtk_widget_show(req);
}

static GtkWidget *make_browse_button(GtkWidget *entry)
{
	GtkWidget *button;

	button = gtk_button_new_with_label(GetString(STR_BROWSE_CTRL));
	gtk_widget_show(button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)cb_browse, (void *)entry);
	return button;
}

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

static GtkWidget *table_make_option_menu(GtkWidget *table, int row, int label_id, const opt_desc *options, int active)
{
	GtkWidget *label, *opt, *menu;

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	opt = gtk_option_menu_new();
	gtk_widget_show(opt);
	menu = gtk_menu_new();

	while (options->label_id) {
		add_menu_item(menu, options->label_id, options->func);
		options++;
	}
	gtk_menu_set_active(GTK_MENU(menu), active);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
	gtk_table_attach(GTK_TABLE(table), opt, 1, 2, row, row + 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	return menu;
}

static GtkWidget *table_make_combobox(GtkWidget *table, int row, int label_id, const char *default_value, GList *glist)
{
	GtkWidget *label, *combo;
	char str[32];

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);
	
	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);

	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), default_value);
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

	button = make_browse_button(entry);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(entry), "chooser_button", button);
	return entry;
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

static GtkWidget *make_file_entry(GtkWidget *top, int label_id, const char *prefs_item, bool only_dirs = false)
{
	GtkWidget *box, *label, *entry;

	box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(top), box, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	const char *str = PrefsFindString(prefs_item);
	if (str == NULL)
		str = "";

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), str); 
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);
	return entry;
}

static const gchar *get_file_entry_path(GtkWidget *entry)
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

static GtkWidget *make_combobox(GtkWidget *top, int label_id, const char *prefs_item, const combo_desc *options)
{
	GtkWidget *box, *label, *combo;
	char str[32];

	box = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(top), box, FALSE, FALSE, 0);

	label = gtk_label_new(GetString(label_id));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	GList *glist = NULL;
	while (options->label_id) {
		glist = g_list_append(glist, (void *)GetString(options->label_id));
		options++;
	}
	
	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	
	sprintf(str, "%d", PrefsFindInt32(prefs_item));
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str);
	gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
	
	return combo;
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

// "Zap PRAM" button clicked
static void cb_zap_pram(...)
{
	ZapPRAM();
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

// "About" button clicked
static void cb_about(...)
{
	GtkWidget *dialog;

	GtkWidget *label, *button;

	char str[512];
	sprintf(str,
		"Basilisk II\nVersion %d.%d\n\n"
		"Copyright (C) 1997-2005 Christian Bauer et al.\n"
		"E-mail: Christian.Bauer@uni-mainz.de\n"
		"http://www.uni-mainz.de/~bauec002/B2Main.html\n\n"
		"Basilisk II comes with ABSOLUTELY NO\n"
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

// Menu item descriptions
static GtkItemFactoryEntry menu_items[] = {
	{(gchar *)GetString(STR_PREFS_MENU_FILE_GTK),		NULL,			NULL,							0, "<Branch>"},
	{(gchar *)GetString(STR_PREFS_ITEM_START_GTK),		NULL,			GTK_SIGNAL_FUNC(cb_start),		0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_ZAP_PRAM_GTK),	NULL,			GTK_SIGNAL_FUNC(cb_zap_pram),	0, NULL},
	{(gchar *)GetString(STR_PREFS_ITEM_SEPL_GTK),		NULL,			NULL,							0, "<Separator>"},
	{(gchar *)GetString(STR_PREFS_ITEM_QUIT_GTK),		"<control>Q",	GTK_SIGNAL_FUNC(cb_quit),		0, NULL},
	{(gchar *)GetString(STR_HELP_MENU_GTK),				NULL,			NULL,							0, "<LastBranch>"},
	{(gchar *)GetString(STR_HELP_ITEM_ABOUT_GTK),		NULL,			GTK_SIGNAL_FUNC(cb_about),		0, NULL}
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
#if GTK_CHECK_VERSION(1,3,15)
	gtk_window_add_accel_group(GTK_WINDOW(win), accel_group);
#else
	gtk_accel_group_attach(accel_group, GTK_OBJECT(win));
#endif
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
	create_input_pane(notebook);
	create_serial_pane(notebook);
	create_ethernet_pane(notebook);
	create_memory_pane(notebook);
	create_jit_pane(notebook);

	static const opt_desc buttons[] = {
		{STR_START_BUTTON, GTK_SIGNAL_FUNC(cb_start)},
		{STR_PREFS_ITEM_ZAP_PRAM, GTK_SIGNAL_FUNC(cb_zap_pram)},
		{STR_ABOUT_BUTTON, GTK_SIGNAL_FUNC(cb_about)},
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

static GtkWidget *volume_list;
static int selected_volume;

// Volume in list selected
static void cl_selected(GtkWidget *list, int row, int column)
{
	selected_volume = row;
}

// Volume selected for addition
static void add_volume_ok(GtkWidget *button, file_req_assoc *assoc)
{
	gchar *file = (gchar *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(assoc->req));
	gtk_clist_append(GTK_CLIST(volume_list), &file);
	gtk_widget_destroy(assoc->req);
	delete assoc;
}

// Volume selected for creation
static void create_volume_ok(GtkWidget *button, file_req_assoc *assoc)
{
	gchar *file = (gchar *)gtk_file_selection_get_filename(GTK_FILE_SELECTION(assoc->req));

	const gchar *str = gtk_entry_get_text(GTK_ENTRY(assoc->entry));
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
	while ((str = const_cast<char *>(PrefsFindString("disk", index++))) != NULL)
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

static GtkWidget *w_jit_fpu;
static GtkWidget *w_jit_atraps;
static GtkWidget *w_jit_cache_size;
static GtkWidget *w_jit_lazy_flush;
static GtkWidget *w_jit_follow_const_jumps;

// Set sensitivity of widgets
static void set_jit_sensitive(void)
{
	const bool jit_enabled = PrefsFindBool("jit");
	gtk_widget_set_sensitive(w_jit_fpu, jit_enabled);
	gtk_widget_set_sensitive(w_jit_cache_size, jit_enabled);
	gtk_widget_set_sensitive(w_jit_lazy_flush, jit_enabled);
	gtk_widget_set_sensitive(w_jit_follow_const_jumps, jit_enabled);
}

// "Use JIT Compiler" button toggled
static void tb_jit(GtkWidget *widget)
{
	PrefsReplaceBool("jit", GTK_TOGGLE_BUTTON(widget)->active);
	set_jit_sensitive();
}

// "Compile FPU Instructions" button toggled
static void tb_jit_fpu(GtkWidget *widget)
{
	PrefsReplaceBool("jitfpu", GTK_TOGGLE_BUTTON(widget)->active);
}

// "Lazy translation cache invalidation" button toggled
static void tb_jit_lazy_flush(GtkWidget *widget)
{
	PrefsReplaceBool("jitlazyflush", GTK_TOGGLE_BUTTON(widget)->active);
}

// "Translate through constant jumps (inline blocks)" button toggled
static void tb_jit_follow_const_jumps(GtkWidget *widget)
{
	PrefsReplaceBool("jitinline", GTK_TOGGLE_BUTTON(widget)->active);
}

// Read settings from widgets and set preferences
static void read_jit_settings(void)
{
#if USE_JIT
	bool jit_enabled = PrefsFindBool("jit");
	if (jit_enabled) {
		const char *str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(w_jit_cache_size)->entry));
		PrefsReplaceInt32("jitcachesize", atoi(str));
	}
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
	
	w_jit_fpu = make_checkbox(box, STR_JIT_FPU_CTRL, "jitfpu", GTK_SIGNAL_FUNC(tb_jit_fpu));
	
	// Translation cache size
	static const combo_desc options[] = {
		STR_JIT_CACHE_SIZE_2MB_LAB,
		STR_JIT_CACHE_SIZE_4MB_LAB,
		STR_JIT_CACHE_SIZE_8MB_LAB,
		STR_JIT_CACHE_SIZE_16MB_LAB,
		0
	};
	w_jit_cache_size = make_combobox(box, STR_JIT_CACHE_SIZE_CTRL, "jitcachesize", options);
	
	// Lazy translation cache invalidation
	w_jit_lazy_flush = make_checkbox(box, STR_JIT_LAZY_CINV_CTRL, "jitlazyflush", GTK_SIGNAL_FUNC(tb_jit_lazy_flush));

	// Follow constant jumps (inline basic blocks)
	w_jit_follow_const_jumps = make_checkbox(box, STR_JIT_FOLLOW_CONST_JUMPS, "jitinline", GTK_SIGNAL_FUNC(tb_jit_follow_const_jumps));

	set_jit_sensitive();
#endif
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
		const char *str = get_file_entry_path(w_scsi[id]);
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
		w_scsi[id] = make_file_entry(box, STR_SCSI_ID_0 + id, prefs_name);
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

// Hide/show graphics widgets
static void hide_show_graphics_widgets(void)
{
	switch (display_type) {
		case DISPLAY_WINDOW:
			gtk_widget_show(w_frameskip); gtk_widget_show(l_frameskip);
			break;
		case DISPLAY_SCREEN:
			gtk_widget_hide(w_frameskip); gtk_widget_hide(l_frameskip);
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
static void mn_dynamic(...) {PrefsReplaceInt32("frameskip", 0);}

// Set sensitivity of widgets
static void set_graphics_sensitive(void)
{
	const bool sound_enabled = !PrefsFindBool("nosound");
}

// "Disable Sound Output" button toggled
static void tb_nosound(GtkWidget *widget)
{
	PrefsReplaceBool("nosound", GTK_TOGGLE_BUTTON(widget)->active);
	set_graphics_sensitive();
}

// Read graphics preferences
static void parse_graphics_prefs(void)
{
	display_type = DISPLAY_WINDOW;
	dis_width = 512;
	dis_height = 384;

	const char *str = PrefsFindString("screen");
	if (str) {
		if (sscanf(str, "win/%d/%d", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(str, "dga/%d/%d", &dis_width, &dis_height) == 2)
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
			sprintf(pref, "dga/%d/%d", dis_width, dis_height);
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
	add_menu_item(menu, STR_REF_DYNAMIC_LAB, GTK_SIGNAL_FUNC(mn_dynamic));
	int frameskip = PrefsFindInt32("frameskip");
	int item = -1;
	switch (frameskip) {
		case 12: item = 0; break;
		case 8: item = 1; break;
		case 6: item = 2; break;
		case 4: item = 3; break;
		case 2: item = 4; break;
		case 1: item = 5; break;
		case 0: item = 6; break;
	}
	if (item >= 0)
		gtk_menu_set_active(GTK_MENU(menu), item);
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

	make_separator(box);
	make_checkbox(box, STR_NOSOUND_CTRL, "nosound", GTK_SIGNAL_FUNC(tb_nosound));

	set_graphics_sensitive();

	hide_show_graphics_widgets();
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
	w_keycode_file = make_file_entry(box, STR_KEYCODE_FILE_CTRL, "keycodefile");

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
 *  "Serial" pane
 */

static GtkWidget *w_seriala, *w_portfile0;
static GtkWidget *w_serialb, *w_portfile1;

// Set sensitivity of widgets
static void set_serial_sensitive(void)
{
	const char *str;
	bool is_file;

	str = gtk_entry_get_text(GTK_ENTRY(w_seriala));
	is_file = strcmp(str, "FILE") == 0;
	gtk_widget_set_sensitive(w_portfile0, is_file);
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(w_portfile0), "chooser_button")), is_file);

	str = gtk_entry_get_text(GTK_ENTRY(w_serialb));
	is_file = strcmp(str, "FILE") == 0;
	gtk_widget_set_sensitive(w_portfile1, is_file);
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(w_portfile1), "chooser_button")), is_file);
}

// Read settings from widgets and set preferences
static void read_serial_settings(void)
{
	const char *str;

	str = gtk_entry_get_text(GTK_ENTRY(w_seriala));
	PrefsReplaceString("seriala", str);

	str = gtk_entry_get_text(GTK_ENTRY(w_serialb));
	PrefsReplaceString("serialb", str);

	str = gtk_entry_get_text(GTK_ENTRY(w_portfile0));
	PrefsReplaceString("portfile0", str);

	str = gtk_entry_get_text(GTK_ENTRY(w_portfile1));
	PrefsReplaceString("portfile1", str);
}

// Port changed in combo
static void cb_serial_port_changed(...)
{
	printf("serial port changed\n");
	set_serial_sensitive();
}

// Add names of serial devices
static GList *add_serial_names(void)
{
	GList *glist = NULL;

	static const char *port_names[] = {
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6",
		"FILE",
		NULL
	};

	for (int i = 0; port_names[i] != NULL; i++)
		glist = g_list_append(glist, (void *)port_names[i]);

	return glist;
}

// Create "Serial" pane
static void create_serial_pane(GtkWidget *top)
{
	GtkWidget *box, *hbox, *table, *label, *combo, *sep, *entry;
	GtkObject *adj;

	box = make_pane(top, STR_SERIAL_PANE_TITLE);
	table = make_table(box, 2, 5);

	GList *glist = add_serial_names();
	const char *str = PrefsFindString("seriala");
	combo = table_make_combobox(table, 0, STR_SERIALA_CTRL, str, glist);
	w_seriala = GTK_COMBO(combo)->entry;
	gtk_signal_connect(GTK_OBJECT(w_seriala), "changed", GTK_SIGNAL_FUNC(cb_serial_port_changed), NULL);

	w_portfile0 = table_make_file_entry(table, 1, STR_FILE_CTRL, "portfile0");

	sep = gtk_hseparator_new();
	gtk_widget_show(sep);
	gtk_table_attach(GTK_TABLE(table), sep, 0, 2, 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	str = PrefsFindString("serialb");
	combo = table_make_combobox(table, 3, STR_SERIALB_CTRL, str, glist);
	w_serialb = GTK_COMBO(combo)->entry;
	gtk_signal_connect(GTK_OBJECT(w_serialb), "changed", GTK_SIGNAL_FUNC(cb_serial_port_changed), NULL);

	w_portfile1 = table_make_file_entry(table, 4, STR_FILE_CTRL, "portfile1");

	set_serial_sensitive();
}


/*
 *  "Ethernet" pane
 */

static GtkWidget *w_ether, *w_udp_port;

// Set sensitivity of widgets
static void set_ethernet_sensitive(void)
{
}

// Read settings from widgets and set preferences
static void read_ethernet_settings(void)
{
	const char *str = gtk_entry_get_text(GTK_ENTRY(w_ether));
	if (str && strlen(str))
		PrefsReplaceString("ether", str);
	else
		PrefsRemoveItem("ether");
}

// Add names of ethernet interfaces
static GList *add_ether_names(void)
{
	GList *glist = NULL;

	// TODO: Get list of all Ethernet interfaces
#ifdef HAVE_SLIRP
	static char s_slirp[] = "slirp";
	glist = g_list_append(glist, s_slirp);
#endif
#if 0
	if (glist)
		g_list_sort(glist, gl_str_cmp);
	else
#endif
		glist = g_list_append(glist, (void *)GetString(STR_NONE_LAB));
	return glist;
}


// Create "Ethernet" pane
static void create_ethernet_pane(GtkWidget *top)
{
	GtkWidget *box, *hbox, *table, *label, *combo, *sep, *entry;

	box = make_pane(top, STR_NETWORK_PANE_TITLE);
	table = make_table(box, 2, 5);

	label = gtk_label_new(GetString(STR_ETHERNET_IF_CTRL));
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 4);

	GList *glist = add_ether_names();
	combo = gtk_combo_new();
	gtk_widget_show(combo);
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);
	const char *str = PrefsFindString("ether");
	if (str == NULL)
		str = "";
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), str); 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 2, 0, 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)0, 4, 4);
	w_ether = GTK_COMBO(combo)->entry;

	set_ethernet_sensitive();
}


/*
 *  "Memory/Misc" pane
 */

static GtkWidget *w_ramsize;
static GtkWidget *w_rom_file;

// "Ignore SEGV" button toggled
#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
static void tb_ignoresegv(GtkWidget *widget)
{
	PrefsReplaceBool("ignoresegv", GTK_TOGGLE_BUTTON(widget)->active);
}
#endif

// Model ID selected
static void mn_modelid_5(...) {PrefsReplaceInt32("modelid", 5);}
static void mn_modelid_14(...) {PrefsReplaceInt32("modelid", 14);}

// CPU/FPU type
static void mn_cpu_68020(...) {PrefsReplaceInt32("cpu", 2); PrefsReplaceBool("fpu", false);}
static void mn_cpu_68020_fpu(...) {PrefsReplaceInt32("cpu", 2); PrefsReplaceBool("fpu", true);}
static void mn_cpu_68030(...) {PrefsReplaceInt32("cpu", 3); PrefsReplaceBool("fpu", false);}
static void mn_cpu_68030_fpu(...) {PrefsReplaceInt32("cpu", 3); PrefsReplaceBool("fpu", true);}
static void mn_cpu_68040(...) {PrefsReplaceInt32("cpu", 4); PrefsReplaceBool("fpu", true);}

// Read settings from widgets and set preferences
static void read_memory_settings(void)
{
	const char *str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(w_ramsize)->entry));
	PrefsReplaceInt32("ramsize", atoi(str) << 20);

	str = get_file_entry_path(w_rom_file);
	if (str && strlen(str))
		PrefsReplaceString("rom", str);
	else
		PrefsRemoveItem("rom");

}

// Create "Memory/Misc" pane
static void create_memory_pane(GtkWidget *top)
{
	GtkWidget *box, *hbox, *table, *label, *scale, *menu;

	box = make_pane(top, STR_MEMORY_MISC_PANE_TITLE);
	table = make_table(box, 2, 5);

	static const combo_desc options[] = {
		STR_RAMSIZE_2MB_LAB,
		STR_RAMSIZE_4MB_LAB,
		STR_RAMSIZE_8MB_LAB,
		STR_RAMSIZE_16MB_LAB,
		STR_RAMSIZE_32MB_LAB,
		STR_RAMSIZE_64MB_LAB,
		STR_RAMSIZE_128MB_LAB,
		STR_RAMSIZE_256MB_LAB,
		STR_RAMSIZE_512MB_LAB,
		STR_RAMSIZE_1024MB_LAB,
		0
	};
	char default_ramsize[10];
	sprintf(default_ramsize, "%d", PrefsFindInt32("ramsize") >> 20);
	w_ramsize = table_make_combobox(table, 0, STR_RAMSIZE_CTRL, default_ramsize, options);

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
	table_make_option_menu(table, 2, STR_MODELID_CTRL, model_options, active);

#if EMULATED_68K
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
	table_make_option_menu(table, 3, STR_CPU_CTRL, cpu_options, active);
#endif

	w_rom_file = table_make_file_entry(table, 4, STR_ROM_FILE_CTRL, "rom");

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	make_checkbox(box, STR_IGNORESEGV_CTRL, "ignoresegv", GTK_SIGNAL_FUNC(tb_ignoresegv));
#endif
}


/*
 *  Read settings from widgets and set preferences
 */

static void read_settings(void)
{
	read_volumes_settings();
	read_scsi_settings();
	read_graphics_settings();
	read_input_settings();
	read_serial_settings();
	read_ethernet_settings();
	read_memory_settings();
	read_jit_settings();
}


/*
 *  Fake unused data and functions
 */

uint8 XPRAM[XPRAM_SIZE];
void MountVolume(void *fh) { }
void FileDiskLayout(loff_t size, uint8 *data, loff_t &start_byte, loff_t &real_size) { }
void WarningAlert(const char *text) { }


/*
 *  Add default serial prefs (must be added, even if no ports present)
 */

void SysAddSerialPrefs(void)
{
	PrefsAddString("seriala", "COM1");
	PrefsAddString("serialb", "COM2");
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
	PrefsInit(argc, argv);

	// Show preferences editor
	bool start = PrefsEditor();

	// Exit preferences
	PrefsExit();

	// Transfer control to the Basilisk II executable
	if (start) {
		printf("Start Basilisk II\n");
	}

	return 0;
}
