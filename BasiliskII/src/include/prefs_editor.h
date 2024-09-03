/*
 *  prefs_editor.h - Preferences editor
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

#ifndef PREFS_EDITOR_H
#define PREFS_EDITOR_H

#ifdef __BEOS__
extern void PrefsEditor(uint32 msg);
#else
extern bool PrefsEditor(void);
#endif

#if defined(ENABLE_GTK) || defined(STANDALONE_GUI)

#include <gtk/gtk.h>

#if !GLIB_CHECK_VERSION(2, 24, 0)
#define GVariant void
#endif
#if !GLIB_CHECK_VERSION(2, 28, 0)
#define GSimpleAction void
#endif

extern "C" {
void dl_quit(GtkWidget *dialog);
void cb_swap_opt_cmd (GtkWidget *widget);
void cb_infobar_show (GtkWidget *widget);
}
#endif
#endif
