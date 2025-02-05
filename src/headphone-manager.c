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

struct _HeadphoneManagerPrivate {
    Alsa *alsa;
    Events *events;
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

    alsa_volume_switch (self->priv->alsa);
}

static void
headphone_manager_dispose (GObject *headphone_manager)
{
    HeadphoneManager *self = HEADPHONE_MANAGER (headphone_manager);

    G_OBJECT_CLASS (headphone_manager_parent_class)->dispose (headphone_manager);
}

static void
headphone_manager_finalize (GObject *headphone_manager)
{
    HeadphoneManager *self = HEADPHONE_MANAGER (headphone_manager);

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
