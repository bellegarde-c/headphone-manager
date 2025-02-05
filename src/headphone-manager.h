/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#ifndef HEADPHONE_MANAGER_H
#define HEADPHONE_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#define TYPE_HEADPHONE_MANAGER \
    (headphone_manager_get_type ())
#define HEADPHONE_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), TYPE_HEADPHONE_MANAGER, HeadphoneManager))
#define HEADPHONE_MANAGER_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST \
    ((cls), TYPE_HEADPHONE_MANAGER, HeadphoneManagerClass))
#define IS_HEADPHONE_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE \
    ((obj), TYPE_HEADPHONE_MANAGER))
#define IS_HEADPHONE_MANAGER_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE \
    ((cls), TYPE_HEADPHONE_MANAGER))
#define HEADPHONE_MANAGER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), TYPE_HEADPHONE_MANAGER, HeadphoneManagerClass))

G_BEGIN_DECLS

typedef struct _HeadphoneManager HeadphoneManager;
typedef struct _HeadphoneManagerClass HeadphoneManagerClass;
typedef struct _HeadphoneManagerPrivate HeadphoneManagerPrivate;

struct _HeadphoneManager {
    GObject parent;
    HeadphoneManagerPrivate *priv;
};

struct _HeadphoneManagerClass {
    GObjectClass parent_class;
};

GType           headphone_manager_get_type            (void) G_GNUC_CONST;

GObject*        headphone_manager_new                 (void);

G_END_DECLS

#endif

