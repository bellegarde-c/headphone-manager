/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "config.h"
#include "mpris.h"
#include "utils.h"

#define DBUS_FREEDESKTOP_NAME           "org.freedesktop.DBus"
#define DBUS_FREEDESKTOP_PATH           "/org/freedesktop/DBus"
#define DBUS_FREEDESKTOP_INTERFACE      "org.freedesktop.DBus"

#define DBUS_MPRIS_PATH                 "/org/mpris/MediaPlayer2"
#define DBUS_MPRIS_INTERFACE            "org.mpris.MediaPlayer2"
#define DBUS_MPRIS_PLAYER_INTERFACE     "org.mpris.MediaPlayer2.Player"
#define DBUS_MPRIS_PREFIX               "org.mpris.MediaPlayer2."

struct Player {
    GDBusProxy *bus;
    char       *name;
    gboolean    was_playing;
};

struct _MprisPrivate {
    GDBusProxy *dbus_proxy;

    GList *players;
};

G_DEFINE_TYPE_WITH_CODE (Mpris, mpris, G_TYPE_OBJECT,
    G_ADD_PRIVATE (Mpris))

static struct Player *
get_player (GDBusProxy *bus,
            const char *name)
{
    struct Player *player;

    player = g_malloc (sizeof (struct Player));
    player->bus = bus;
    player->name = g_strdup (name);
    player->was_playing = FALSE;

    return player;
}

static void
clear_player (struct Player *player)
{
    g_clear_object (&player->bus);
    g_free (player->name);
    g_free (player);
}

static void
add_player (Mpris      *self,
            const char *name)
{
    GDBusProxy *player_bus;
    struct Player *player;

    if (!g_str_has_prefix (name, DBUS_MPRIS_PREFIX))
        return;

    g_message ("Player added: %s", name);

    player_bus = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SESSION,
        0,
        NULL,
        name,
        DBUS_MPRIS_PATH,
        DBUS_MPRIS_PLAYER_INTERFACE,
        NULL,
        NULL
    );

    g_return_if_fail (player_bus != NULL);

    player = get_player (player_bus, name);

    self->priv->players = g_list_append (self->priv->players, player);
}

static void
del_player (Mpris      *self,
            const char *name)
{
    struct Player *player;

    if (!g_str_has_prefix (name, DBUS_MPRIS_PREFIX))
        return;

    g_message ("Player removed: %s", name);

    GFOREACH (self->priv->players, player) {
        if (g_strcmp0 (player->name, name) == 0) {
            self->priv->players = g_list_remove_all (
                self->priv->players, player
            );
            clear_player (player);
            return;
        }
    }
}

static void
add_players (Mpris *self)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) value = NULL;
    g_autoptr (GVariantIter) iter;
    const char *player;

    value = g_dbus_proxy_call_sync (
        self->priv->dbus_proxy,
        "ListNames",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't get MPRIS players: %s", error->message);
        return;
    }

    g_variant_get (value, "(as)", &iter);
    while (g_variant_iter_loop (iter, "&s", &player))
        add_player (self, player);
}

static void
on_dbus_signal (GDBusProxy *proxy,
                const char *sender_name,
                const char *signal_name,
                GVariant   *parameters,
                gpointer    user_data)
{
    Mpris *self = MPRIS (user_data);

    if (g_strcmp0 (signal_name, "NameOwnerChanged") == 0) {
        const char *name = NULL;
        const char *old_owner = NULL;
        const char *new_owner = NULL;

        g_variant_get (parameters,
            "(&s&s&s)",
            &name,
            &old_owner,
            &new_owner
        );

        if (old_owner != NULL && strlen (old_owner) > 0) {
            del_player (self, name);
        }
        if (new_owner != NULL && strlen (new_owner) > 0) {
            add_player (self, name);
        }
    }
}

static void
mpris_dispose (GObject *mpris)
{
    Mpris *self = MPRIS (mpris);
    struct Player *player;

    GFOREACH (self->priv->players, player)
        clear_player (player);

    g_clear_object (&self->priv->dbus_proxy);

    G_OBJECT_CLASS (mpris_parent_class)->dispose (mpris);
}

static void
mpris_finalize (GObject *mpris)
{
    Mpris *self = MPRIS (mpris);

    g_list_free (self->priv->players);

    G_OBJECT_CLASS (mpris_parent_class)->finalize (mpris);
}

static void
mpris_class_init (MprisClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = mpris_dispose;
    object_class->finalize = mpris_finalize;
}

static void
mpris_init (Mpris *self)
{
    self->priv = mpris_get_instance_private (self);

    self->priv->dbus_proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SESSION,
        0,
        NULL,
        DBUS_FREEDESKTOP_NAME,
        DBUS_FREEDESKTOP_PATH,
        DBUS_FREEDESKTOP_INTERFACE,
        NULL,
        NULL
    );

    add_players (self);

    g_signal_connect (
        self->priv->dbus_proxy,
        "g-signal",
        G_CALLBACK (on_dbus_signal),
        self
    );
}

/**
 * mpris_new:
 *
 * Creates a new #Mpris
 *
 * Returns: (transfer full): a new #Mpris
 *
 **/
GObject *
mpris_new (void)
{
    GObject *mpris;

    mpris = g_object_new (
        TYPE_MPRIS,
        NULL
    );

    return mpris;
}

/**
 * mpris_play:
 *
 * Start playing last playing mpris player
 *
 * @self: a #Mpris
 *
 **/
void
mpris_play (Mpris *self)
{
    struct Player *player;

    GFOREACH (self->priv->players, player) {
        if (player->was_playing) {
             g_dbus_proxy_call (
                player->bus,
                "Play",
                NULL,
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                NULL,
                NULL
            );
        }
    }
}

/**
 * mpris_pause:
 *
 * Pause any playing mpris player
 *
 * @self: a #Mpris
 *
 **/
void
mpris_pause (Mpris *self)
{
        struct Player *player;

    GFOREACH (self->priv->players, player) {
        GVariant *value;

        value = g_dbus_proxy_get_cached_property (
            player->bus, "PlaybackStatus"
        );

        if (value == NULL)
            continue;

        player->was_playing = g_strcmp0 (
            g_variant_get_string (value, NULL), "Playing"
        ) == 0;
        g_variant_unref (value);

        if (player->was_playing) {
             g_dbus_proxy_call (
                player->bus,
                "Pause",
                NULL,
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                NULL,
                NULL
            );
        }
    }
}