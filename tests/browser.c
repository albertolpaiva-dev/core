/*
 Copyright (C) 2009 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "midori.h"

static gboolean
skip_gtk_bugs (const gchar*   log_domain,
               GLogLevelFlags log_level,
               const gchar*   message,
               gpointer       user_data)
{
    if (!strcmp (message, "get_column_number: assertion `i < gtk_tree_view_get_n_columns (treeview)' failed"))
        return FALSE;
    return TRUE;
}

static void
browser_create (void)
{
    MidoriApp* app;
    MidoriSpeedDial* dial;
    MidoriWebSettings* settings;
    MidoriBrowser* browser;
    gint n;
    gchar* temporary_downloads;
    GtkWidget* view;
    GFile* file;
    gchar* uri;

    g_test_log_set_fatal_handler (skip_gtk_bugs, NULL);

    app = midori_app_new ();
    dial = midori_speed_dial_new ("/", NULL);
    settings = midori_web_settings_new ();
    g_object_set (app, "speed-dial", dial, "settings", settings, NULL);
    browser = midori_app_create_browser (app);
    n = midori_browser_add_uri (browser, "about:blank");
    view = midori_browser_get_nth_tab (browser, n);

    midori_test_set_dialog_response (GTK_RESPONSE_OK);
    temporary_downloads = g_dir_make_tmp ("saveXXXXXX", NULL);
    midori_settings_set_download_folder (MIDORI_SETTINGS (settings), temporary_downloads);
    midori_browser_save_uri (browser, MIDORI_VIEW (view), NULL);

    /* View source for local file: should NOT use temporary file */
    file = g_file_new_for_commandline_arg ("./data/error.html");
    uri = g_file_get_uri (file);
    g_object_unref (file);
    n = midori_browser_add_uri (browser, uri);
    midori_browser_set_current_page (browser, n);
    g_assert_cmpstr (uri, ==, midori_browser_get_current_uri (browser));
    g_free (uri);

    gtk_widget_destroy (GTK_WIDGET (browser));
    g_object_unref (settings);
    g_object_unref (app);
    g_object_unref (dial);
}

static void
browser_tooltips (void)
{
    MidoriBrowser* browser;
    GtkActionGroup* action_group;
    GList* actions;
    gchar* toolbar;
    guint errors = 0;

    browser = midori_browser_new ();
    action_group = midori_browser_get_action_group (browser);
    actions = gtk_action_group_list_actions (action_group);
    toolbar = g_strjoinv (" ", (gchar**)midori_browser_get_toolbar_actions (browser));

    while (actions)
    {
        GtkAction* action = actions->data;
        const gchar* name = gtk_action_get_name (action);

        if (strstr ("CompactMenu Location Separator", name))
        {
            actions = g_list_next (actions);
            continue;
        }

        if (strstr (toolbar, name) != NULL)
        {
            if (!gtk_action_get_tooltip (action))
            {
                printf ("'%s' can be toolbar item but tooltip is unset\n", name);
                errors++;
            }
        }
        else
        {
            if (gtk_action_get_tooltip (action))
            {
                printf ("'%s' is no toolbar item but tooltip is set\n", name);
                errors++;
            }
        }
        actions = g_list_next (actions);
    }
    g_free (toolbar);
    g_list_free (actions);
    gtk_widget_destroy (GTK_WIDGET (browser));

    if (errors)
        g_error ("Tooltip errors");
}

static void
browser_site_data (void)
{
    typedef struct
    {
        const gchar* url;
        MidoriSiteDataPolicy policy;
    } PolicyItem;

    static const PolicyItem items[] = {
    { "google.com", MIDORI_SITE_DATA_BLOCK },
    { "facebook.com", MIDORI_SITE_DATA_BLOCK },
    { "bugzilla.gnome.org", MIDORI_SITE_DATA_PRESERVE },
    { "bugs.launchpad.net", MIDORI_SITE_DATA_ACCEPT },
    };

    const gchar* rules = "-google.com,-facebook.com,!bugzilla.gnome.org,+bugs.launchpad.net";
    MidoriWebSettings* settings = g_object_new (MIDORI_TYPE_WEB_SETTINGS,
        "site-data-rules", rules, NULL);

    guint i;
    for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
        MidoriSiteDataPolicy policy = midori_web_settings_get_site_data_policy (
            settings, items[i].url);
        if (policy != items[i].policy)
            g_error ("Match '%s' yields %d but %d expected",
                     items[i].url, policy, items[i].policy);
    }
    g_object_unref (settings);
}

int
main (int    argc,
      char** argv)
{
    g_test_init (&argc, &argv, NULL);
    midori_app_setup (&argc, &argv, NULL, NULL);
    midori_paths_init (MIDORI_RUNTIME_MODE_NORMAL, NULL);

    g_object_set_data (G_OBJECT (webkit_get_default_session ()),
                       "midori-session-initialized", (void*)1);

    g_test_add_func ("/browser/create", browser_create);
    g_test_add_func ("/browser/tooltips", browser_tooltips);
    g_test_add_func ("/browser/site_data", browser_site_data);

    return g_test_run ();
}
