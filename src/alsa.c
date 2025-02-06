/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <alsa/asoundlib.h>

#include "config.h"
#include "events.h"
#include "alsa.h"

struct _AlsaPrivate {
    long volume;
};

G_DEFINE_TYPE_WITH_CODE (
    Alsa,
    alsa,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Alsa)
)

static void
volume_switch (Alsa *self)
{
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "Master";
    snd_mixer_elem_t* elem;
    long volume = self->priv->volume;

    snd_mixer_open (&handle, 0);
    snd_mixer_attach (handle, card);
    snd_mixer_selem_register (handle, NULL, NULL);
    snd_mixer_load (handle);

    snd_mixer_selem_id_alloca (&sid);
    snd_mixer_selem_id_set_index (sid, 0);
    snd_mixer_selem_id_set_name (sid, selem_name);
    elem = snd_mixer_find_selem (handle, sid);

    snd_mixer_selem_get_playback_volume(elem, 0, &self->priv->volume);

    if (volume != -1) {
        snd_mixer_selem_set_playback_volume_all (
            elem, volume
        );
    }

    snd_mixer_close (handle);
}

static void
alsa_dispose (GObject *alsa)
{
    G_OBJECT_CLASS (alsa_parent_class)->dispose (alsa);
}

static void
alsa_finalize (GObject *alsa)
{
    G_OBJECT_CLASS (alsa_parent_class)->finalize (alsa);
}

static void
alsa_class_init (AlsaClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = alsa_dispose;
    object_class->finalize = alsa_finalize;
}

static void
alsa_init (Alsa *self)
{
    self->priv = alsa_get_instance_private (self);
    self->priv->volume = -1;
}

/**
 * alsa_new:
 *
 * Creates a new #Alsa
 *
 * Returns: (transfer full): a new #Alsa
 *
 **/
GObject *
alsa_new (void)
{
    GObject *alsa;

    alsa = g_object_new (TYPE_ALSA, NULL);

    return alsa;
}

/**
 * alsa_volume_switch:
 *
 * Switch volume to previous value if any
 *
 **/
void
alsa_volume_switch (Alsa *self)
{
    volume_switch (self);
}