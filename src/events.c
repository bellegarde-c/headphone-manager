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
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <gudev/gudev.h>

#include "config.h"
#include "events.h"
#include "utils.h"

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#define POLL_MAX 10

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x)  ((x) % BITS_PER_LONG)
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

/* signals */
enum
{
    HEADPHONE_STATE_CHANGED,
    MEDIA_KEY_PRESSED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct poll_data {
    struct pollfd fds[POLL_MAX+1];
    char *device[POLL_MAX+1];
};

struct _EventsPrivate {
    GHashTable *events;
    struct poll_data *pd;
    guint watched_fds;
    GThread *thread;
    GMutex mutex;
    gboolean closing;
    GUdevClient *udev_client;
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

    g_debug ("Headphone connected");

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

    g_debug ("Headphone removed");

    return FALSE;
}

static gboolean
key_pressed (gpointer user_data)
{
    Events *self = EVENTS (user_data);

    g_signal_emit(
        self,
        signals[MEDIA_KEY_PRESSED],
        0
    );

    g_debug ("Media key pressed signal");

    return FALSE;
}

static void
events_add_device (Events *self, const char *device)
{
    guint64 refresh = 1;
    guint next_device;

    g_mutex_lock (&self->priv->mutex);
    next_device = self->priv->watched_fds;

    if (next_device <= POLL_MAX) {
        g_debug ("Adding %s as device %d", device, next_device);
        self->priv->pd->fds[next_device].fd = open (device, O_RDONLY | O_NONBLOCK);

        if (self->priv->pd->fds[next_device].fd < 0) {
            g_warning ("Unable to open %s. Device will not be watched.", device);
        } else {
            self->priv->pd->fds[next_device].events = POLLIN;
            self->priv->pd->device[next_device] = g_strdup (device);
            self->priv->watched_fds++;
        }
    } else {
        g_warning ("Reached maximum polled device number: %d", POLL_MAX);
    }

    g_mutex_unlock (&self->priv->mutex);

    /* Signal a change to the thread */
    write(self->priv->pd->fds[0].fd, &refresh, sizeof(refresh));
}

static void
events_remove_device_by_id (Events   *self,
                            guint     id,
                            gboolean  update)
{
    struct poll_data *new_pd;
    int old_counter;
    g_debug ("Removing fd for device %d", id);
    close (self->priv->pd->fds[id].fd);
    g_free (self->priv->pd->device[id]);

    if (update) {
        /* Refresh the poll_data without the device we removed */
        new_pd = g_new0 (struct poll_data, 1);
        old_counter = 0;
        for (int i=0; i < self->priv->watched_fds; i++) {
            if (i == id) {
                old_counter++;
                continue;
            }

            new_pd->fds[i] = self->priv->pd->fds[old_counter];
            new_pd->device[i] = self->priv->pd->device[old_counter];
            old_counter++;
        }
        g_free (self->priv->pd);
        self->priv->pd = new_pd;
    }

    self->priv->watched_fds--;
}

static void
events_remove_device (Events *self, const char *device)
{
    gboolean freed = FALSE;
    guint64 refresh = 1;

    g_mutex_lock (&self->priv->mutex);
    for (int i=1; i < self->priv->watched_fds; i++) {
        if (g_strcmp0 (device, self->priv->pd->device[i]) == 0) {
            g_debug ("Removing device %s", device);
            events_remove_device_by_id (self, i, TRUE);
            freed = TRUE;
            break;
        }
    }

    g_mutex_unlock (&self->priv->mutex);
    if (freed) {
        write(self->priv->pd->fds[0].fd, &refresh, sizeof(refresh));
    }
}

static void
events_cleanup (Events *self)
{
    guint64 refresh = 1;
    g_mutex_lock (&self->priv->mutex);
    for (int i=1; i < self->priv->watched_fds; i++) {
        events_remove_device_by_id (self, i, FALSE);
    }

    self->priv->closing = TRUE;
    g_mutex_unlock (&self->priv->mutex);
    write(self->priv->pd->fds[0].fd, &refresh, sizeof(refresh));
}

static gpointer
handle_events (gpointer user_data)
{
    Events *self = user_data;
    const int input_size = sizeof(struct input_event);
    struct input_event input_data;

    while (poll(self->priv->pd->fds, self->priv->watched_fds, -1) > 0) {
        if (self->priv->pd->fds[0].revents & POLLIN) {
            /* Signal eventfd */
            guint64 refresh;
            eventfd_read (self->priv->pd->fds[0].fd, &refresh);

            if (self->priv->closing) {
                close (self->priv->pd->fds[0].fd);
                return NULL;
            } else {
                continue;
            }
        }

        for (int i=1; i < self->priv->watched_fds; i++) {
            if (self->priv->pd->fds[i].revents & POLLIN) {
                if (read (self->priv->pd->fds[i].fd, &input_data, input_size) < 0) {
                    events_remove_device (self, self->priv->pd->device[i]);
                    break;
                }

                if (input_data.code == SW_HEADPHONE_INSERT) {
                    if (input_data.value) {
                        g_idle_add ((GSourceFunc) headphone_present, self);
                    } else {
                        g_idle_add ((GSourceFunc) headphone_absent, self);
                    }
                } else if (input_data.code == KEY_MEDIA || input_data.code == KEY_PLAYPAUSE) {
                    if (input_data.value) {
                        g_debug ("%s: key pressed: %d", self->priv->pd->device[i], input_data.code);
                        g_idle_add ((GSourceFunc) key_pressed, self);
                    }
                }
            } else if (self->priv->pd->fds[i].revents & POLLNVAL || self->priv->pd->fds[i].revents & POLLERR) {
                events_remove_device (self, self->priv->pd->device[i]);
                break;
            }
        }
    }

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
            if (test_bit (SW_HEADPHONE_INSERT, bit[EV_SW])) {
                devices = g_list_append (
                    devices, g_strdup (fname)
                );
            }
        } else if (test_bit (EV_KEY, bit[0])) {
            ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), bit[EV_KEY]);
            if (test_bit (KEY_MEDIA, bit[EV_KEY]) || test_bit (KEY_PLAYPAUSE, bit[EV_KEY])) {
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
on_uevent (GUdevClient *udev_client,
           const gchar *action,
           GUdevDevice *udev_device,
           gpointer     user_data)
{
    Events *self = EVENTS (user_data);
    const gchar *devname = g_udev_device_get_property (udev_device, "DEVNAME");

    if (devname && g_str_has_prefix (devname, "/dev/input/event")) {
        if (g_strcmp0(action, "add") == 0) {
            events_add_device (self, devname);
            g_idle_add ((GSourceFunc) headphone_present, self);
        } else if (g_strcmp0(action, "remove") == 0) {
            events_remove_device (self, devname);
            g_idle_add ((GSourceFunc) headphone_absent, self);
        }
    }
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

    g_clear_object (&self->priv->udev_client);

    events_cleanup (self);
    g_thread_join (self->priv->thread);
    g_thread_unref (self->priv->thread);

    g_free (self->priv->pd);
    g_mutex_clear (&self->priv->mutex);

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

    signals[MEDIA_KEY_PRESSED] = g_signal_new (
        "media-key-pressed",
        G_OBJECT_CLASS_TYPE (object_class),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL, NULL,
        G_TYPE_NONE,
        0
    );

}

static void
events_init (Events *self)
{
    static const gchar *udev_subsystems[] = {"input", NULL};
    GList *devices = scan_devices (self);
    const char *device;

    self->priv = events_get_instance_private (self);
    g_mutex_init (&self->priv->mutex);
    self->priv->pd = g_new0 (struct poll_data, 1);
    self->priv->pd->fds[0].fd = eventfd (0, EFD_NONBLOCK);
    self->priv->pd->fds[0].events = POLLIN;
    self->priv->watched_fds = 1;
    self->priv->closing = FALSE;
    self->priv->udev_client = g_udev_client_new (udev_subsystems);
    self->priv->thread = g_thread_new (NULL, (GThreadFunc) handle_events, self);

    GFOREACH (devices, device) {
        events_add_device (self, device);
    }

    g_signal_connect (self->priv->udev_client, "uevent",
                      G_CALLBACK (on_uevent), self);
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
