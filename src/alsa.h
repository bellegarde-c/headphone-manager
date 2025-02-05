/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#ifndef ALSA_H
#define ALSA_H

#include <glib.h>
#include <glib-object.h>

#define TYPE_ALSA \
    (alsa_get_type ())
#define ALSA(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), TYPE_ALSA, Alsa))
#define ALSA_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST \
    ((cls), TYPE_ALSA, AlsaClass))
#define IS_ALSA(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE \
    ((obj), TYPE_ALSA))
#define IS_ALSA_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE \
    ((cls), TYPE_ALSA))
#define ALSA_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), TYPE_ALSA, AlsaClass))

G_BEGIN_DECLS

typedef struct _Alsa Alsa;
typedef struct _AlsaClass AlsaClass;
typedef struct _AlsaPrivate AlsaPrivate;

struct _Alsa {
    GObject parent;
    AlsaPrivate *priv;
};

struct _AlsaClass {
    GObjectClass parent_class;
};

GType           alsa_get_type            (void) G_GNUC_CONST;

GObject*        alsa_new                 (void);
void            alsa_volume_switch       (Alsa *self);

G_END_DECLS

#endif

