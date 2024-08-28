/* color_scheme.h - Change application theme based on D-Bus
 *
 * Copyright (C) 2022-2024 Rob Hall
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * An enum representing the color state requested by
 * the application. Before the color-scheme library is
 * set up, the application is effectively in the
 * APP_FORCES_LIGHT state.
 */
typedef enum {
    APP_PREFERS_LIGHT,
    APP_PREFERS_DARK,
    APP_FORCES_LIGHT,
    APP_FORCES_DARK
} AppColorScheme;

/*
 * An enum representing the color state given by the
 * desktop portal. If the portal is not available, it
 * defaults to DESKTOP_NO_PREFERENCE.
 */
typedef enum {
    DESKTOP_NO_PREFERENCE,
    DESKTOP_PREFERS_DARK,
    DESKTOP_PREFERS_LIGHT
} DesktopColorScheme;

gboolean color_scheme_set (AppColorScheme scheme);
void color_scheme_set_async (AppColorScheme scheme);
gboolean color_scheme_is_dark_theme (void);
gboolean color_scheme_toggle (void);
AppColorScheme color_scheme_get_app_scheme (void);
DesktopColorScheme color_scheme_get_desktop_scheme (void);
void color_scheme_disconnect (void);

G_END_DECLS
