/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "config.h"
#include "alsa.h"
#include "events.h"
#include "headphone-manager.h"
#include "mpris.h"

struct _HeadphoneManagerPrivate {
    Alsa *alsa;
    Events *events;
    Mpris *mpris;
    GSettings *settings;
};

G_DEFINE_TYPE_WITH_CODE (
    HeadphoneManager,
    headphone_manager,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (HeadphoneManager)
)

static void
on_headphone_state_changed (Events *events,
                            gboolean  headphone_state,
                            gpointer  user_data)
{
    HeadphoneManager *self = HEADPHONE_MANAGER (user_data);

    if (g_settings_get_boolean(self->priv->settings, "restore-sound-level"))
        alsa_volume_switch (self->priv->alsa);

    if (headphone_state &&
            g_settings_get_boolean(self->priv->settings, "launch-player")) {
        GAppInfo *app_info = g_app_info_get_default_for_type (
            "audio/mp3", FALSE
        );
        if (app_info != NULL)
            g_app_info_launch (app_info, NULL, NULL, NULL);
    }

    if (headphone_state &&
            g_settings_get_boolean(self->priv->settings, "pause-mpris")) {
        mpris_play (self->priv->mpris);
    } else {
        mpris_pause (self->priv->mpris);
    }
}

static void
headphone_manager_dispose (GObject *headphone_manager)
{
    HeadphoneManager *self = HEADPHONE_MANAGER (headphone_manager);

    g_clear_object (&self->priv->alsa);
    g_clear_object (&self->priv->mpris);
    g_clear_object (&self->priv->events);
    g_clear_object (&self->priv->settings);

    G_OBJECT_CLASS (headphone_manager_parent_class)->dispose (headphone_manager);
}

static void
headphone_manager_finalize (GObject *headphone_manager)
{
    G_OBJECT_CLASS (headphone_manager_parent_class)->finalize (headphone_manager);
}

static void
headphone_manager_class_init (HeadphoneManagerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = headphone_manager_dispose;
    object_class->finalize = headphone_manager_finalize;
}

static void
headphone_manager_init (HeadphoneManager *self)
{
    self->priv = headphone_manager_get_instance_private (self);

    self->priv->alsa = ALSA (alsa_new ());
    self->priv->events = EVENTS (events_new ());
    self->priv->mpris = MPRIS (mpris_new ());
    self->priv->settings = g_settings_new (APP_ID);

    g_signal_connect (
        self->priv->events,
        "headphone-state-changed",
        G_CALLBACK (on_headphone_state_changed),
        self
    );
}

/**
 * headphone_manager_new:
 *
 * Creates a new #HeadphoneManager
 *
 * Returns: (transfer full): a new #HeadphoneManager
 *
 **/
GObject *
headphone_manager_new (void)
{
    GObject *headphone_manager;

    headphone_manager = g_object_new (TYPE_HEADPHONE_MANAGER, NULL);

    return headphone_manager;
}
