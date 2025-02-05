/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 * Based on https://gitlab.freedesktop.org/libevdev/evtest
 * Copyright (c) 1999-2000 Vojtech Pavlik
 * Copyright (c) 2009-2011 Red Hat, Inc
 */

#include <stdio.h>
#include <stdarg.h>
#include <linux/input.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#include "config.h"
#include "events.h"

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x)  ((x) % BITS_PER_LONG)
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

#define GFOREACH(list, item) \
    for(GList *__glist = list; \
        __glist && (item = __glist->data, TRUE); \
        __glist = __glist->next)

/* signals */
enum
{
    HEADPHONE_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct thread_data {
    char *device;
    Events *self;
};

struct _EventsPrivate {
    GList *threads;
    GList *fds;
};

G_DEFINE_TYPE_WITH_CODE (
    Events,
    events,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Events)
)

static gboolean
headphone_present (gpointer user_data)
{
    Events *self = EVENTS (user_data);

    g_signal_emit(
        self,
        signals[HEADPHONE_STATE_CHANGED],
        0,
        TRUE
    );

    return FALSE;
}

static gboolean
headphone_absent (gpointer user_data)
{
    Events *self = EVENTS (user_data);

    g_signal_emit(
        self,
        signals[HEADPHONE_STATE_CHANGED],
        0,
        FALSE
    );

    return FALSE;
}

static gpointer
handle_events (gpointer user_data)
{
    struct thread_data *data = user_data;
    struct pollfd fds;
    const int input_size = sizeof(struct input_event);
    struct input_event input_data;

    fds.fd = open (data->device, O_RDONLY | O_NONBLOCK);

    if (fds.fd < 0)
        goto free;

    data->self->priv->fds = g_list_append (data->self->priv->fds, GINT_TO_POINTER (fds.fd));

    fds.events = POLLIN;

    while (TRUE) {
        poll(&fds, 1, -1);

        if(fds.revents) {
            if (read (fds.fd, &input_data, input_size) < 0)
                goto free;

            if (input_data.code == SW_HEADPHONE_INSERT) {
                if (input_data.value) {
                    g_idle_add ((GSourceFunc) headphone_present, data->self);
                } else {
                    g_idle_add ((GSourceFunc) headphone_absent, data->self);
                }
            }
        }
    }

free:
    g_free (data->device);

    return NULL;
}

static int
is_event_device (const struct dirent *dir) {
    return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

static GList*
scan_devices(Events *self)
{
    struct dirent **namelist;
    int i, ndev;
    GList *devices = NULL;

    ndev = scandir (DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
    if (ndev <= 0)
        return NULL;

    for (i = 0; i < ndev; i++) {
        char fname[4096];
        int fd = -1;
        unsigned long bit[EV_MAX][NBITS(KEY_MAX)];

        snprintf (fname, sizeof(fname),
             "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);

        fd = open (fname, O_RDONLY);
        if (fd < 0) {
            g_warning ("Can't open %s", fname);
            continue;
        }

        memset (bit, 0, sizeof(bit));
        ioctl (fd, EVIOCGBIT(0, EV_MAX), bit[0]);

        if (test_bit(EV_SW, bit[0])) {
            ioctl(fd, EVIOCGBIT(EV_SW, KEY_MAX), bit[EV_SW]);
            if (test_bit(SW_HEADPHONE_INSERT, bit[EV_SW])) {
                devices = g_list_append (
                    devices, g_strdup (fname)
                );
            }
        }

        free(namelist[i]);
        close(fd);
    }

    return devices;
}

static void
events_dispose (GObject *events)
{
    G_OBJECT_CLASS (events_parent_class)->dispose (events);
}

static void
events_finalize (GObject *events)
{
    Events *self = EVENTS (events);
    GThread *thread;
    int *fd;

    GFOREACH (self->priv->fds, fd) {
        close (*fd);
        g_free (fd);
    }
    g_list_free (self->priv->fds);

    GFOREACH (self->priv->threads, thread) {
        g_thread_join (thread);
        g_thread_unref (thread);
    }
    g_list_free (self->priv->threads);

    G_OBJECT_CLASS (events_parent_class)->finalize (events);
}

static void
events_class_init (EventsClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = events_dispose;
    object_class->finalize = events_finalize;

    signals[HEADPHONE_STATE_CHANGED] = g_signal_new (
        "headphone-state-changed",
        G_OBJECT_CLASS_TYPE (object_class),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL, NULL,
        G_TYPE_NONE,
        1,
        G_TYPE_BOOLEAN
    );
}

static void
events_init (Events *self)
{
    GList *devices = scan_devices (self);
    const char *device;

    self->priv = events_get_instance_private (self);
    self->priv->fds = NULL;
    self->priv->threads = NULL;

    GFOREACH (devices, device) {
        struct thread_data *data = g_new0(struct thread_data, 1);
        data->self = self;
        data->device = g_strdup (device);

        self->priv->threads = g_list_append (
            self->priv->threads,
            g_thread_new(
                NULL, (GThreadFunc) handle_events, data
            )
        );
    }

    g_list_free_full (devices, g_free);
}


/**
 * events_new:
 *
 * Creates a new #Events
 *
 * Returns: (transfer full): a new #Events
 *
 **/
GObject *
events_new (void)
{
    GObject *events;

    events = g_object_new (TYPE_EVENTS, NULL);

    return events;
}
