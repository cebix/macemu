/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Define to 'off_t' if <sys/types.h> doesn't define. */
#undef loff_t

/* Define if using ESD. */
#undef ENABLE_ESD

/* Define if using GTK. */
#undef ENABLE_GTK

/* Define if using "mon". */
#undef ENABLE_MON

/* Define if using XFree86 DGA extension. */
#undef ENABLE_XF86_DGA

/* Define if using XFree86 VidMode extension. */
#undef ENABLE_XF86_VIDMODE


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
