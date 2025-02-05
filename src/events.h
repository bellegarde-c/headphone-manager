/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#ifndef EVENTS_H
#define EVENTS_H

#include <glib.h>
#include <glib-object.h>

#define TYPE_EVENTS \
    (events_get_type ())
#define EVENTS(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
    ((obj), TYPE_EVENTS, Events))
#define EVENTS_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_CAST \
    ((cls), TYPE_EVENTS, EventsClass))
#define IS_EVENTS(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE \
    ((obj), TYPE_EVENTS))
#define IS_EVENTS_CLASS(cls) \
    (G_TYPE_CHECK_CLASS_TYPE \
    ((cls), TYPE_EVENTS))
#define EVENTS_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS \
    ((obj), TYPE_EVENTS, EventsClass))

G_BEGIN_DECLS

typedef struct _Events Events;
typedef struct _EventsClass EventsClass;
typedef struct _EventsPrivate EventsPrivate;

struct _Events {
    GObject parent;
    EventsPrivate *priv;
};

struct _EventsClass {
    GObjectClass parent_class;
};

GType           events_get_type            (void) G_GNUC_CONST;

GObject*        events_new                 (void);

G_END_DECLS

#endif

