/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdlib.h>
#include <gio/gio.h>
#include <glib-unix.h>

#include "headphone-manager.h"
#include "config.h"

static gboolean
exit_handler (gpointer user_data)
{
    GMainLoop *loop = user_data;

    g_main_loop_quit (loop);
    return G_SOURCE_REMOVE;
}

gint
main (gint argc, gchar * argv[])
{
    GMainLoop *loop;
    GObject *headphone_manager;
    g_autoptr (GOptionContext) context = NULL;
    g_autoptr (GError) error = NULL;
    gboolean version = FALSE;
    GOptionEntry main_entries[] = {
        {"version", 0, 0, G_OPTION_ARG_NONE, &version, "Show version"},
        {NULL}
    };

    context = g_option_context_new ("headphone-manager");
    g_option_context_add_main_entries (context, main_entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("%s\n", error->message);
        return EXIT_FAILURE;
    }

    if (version) {
        g_printerr ("%s\n", PACKAGE_VERSION);
        return EXIT_SUCCESS;
    }

    headphone_manager = headphone_manager_new ();

    loop = g_main_loop_new (NULL, FALSE);
    g_unix_signal_add (SIGTERM, exit_handler, loop);
    g_unix_signal_add (SIGINT, exit_handler, loop);
    g_main_loop_run (loop);

    g_clear_pointer (&loop, g_main_loop_unref);
    g_clear_object (&headphone_manager);

    return EXIT_SUCCESS;
}
