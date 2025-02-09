/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <stdio.h>
#include <stdarg.h>

#include <gio/gio.h>

#include "bluetooth.h"
#include "utils.h"

#define BLUEZ_DBUS_NAME               "org.bluez"
#define BLUEZ_DBUS_PATH               "/org/bluez/hci0"
#define BLUEZ_DBUS_ADAPTER_INTERFACE  "org.bluez.Adapter1"
#define BLUEZ_DBUS_DEVICE_INTERFACE   "org.bluez.Device1"

/* signals */
enum
{
    AUDIO_DEVICE_CONNECTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _BluetoothPrivate {
    GDBusObjectManager *object_manager;
    GDBusProxy *bluez_proxy;

    GList *connections;

    gboolean powered;
    gboolean powersaving;
};

G_DEFINE_TYPE_WITH_CODE (
    Bluetooth,
    bluetooth,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (Bluetooth)
)

static void
on_bluez_object_added (GDBusObjectManager *self,
                       GDBusObject        *object,
                       gpointer            user_data);
static void
on_bluez_proxy_properties (GDBusProxy  *proxy,
                           GVariant    *changed_properties,
                           char       **invalidated_properties,
                           gpointer     user_data);

static void
check_existing_connections (Bluetooth *self)
{
    GList *connections, *c;

    connections = g_dbus_object_manager_get_objects(
        G_DBUS_OBJECT_MANAGER (self->priv->object_manager)
    );

    for (c = connections; c; c = g_list_next(c)) {
        on_bluez_object_added(self->priv->object_manager, c->data, self);
    }
    g_list_free_full (connections, (GDestroyNotify) g_object_unref);
}

static void
check_for_audio_device (Bluetooth  *self,
                        GDBusProxy *proxy)
{
    g_autoptr (GVariant) value = NULL;
    const char *path = g_dbus_proxy_get_object_path (proxy);
    const char *icon_name;

    value = g_dbus_proxy_get_cached_property (proxy, "Icon");

    g_return_if_fail (value != NULL);

    icon_name = g_variant_get_string (value, NULL);

    if (g_strcmp0 (icon_name, "audio-headset")) {
        g_message ("Connected audio device: %s", path);
        g_signal_emit(
            self,
            signals[AUDIO_DEVICE_CONNECTED],
            0,
            FALSE
        );
    }
}

static void
on_bluez_object_added (GDBusObjectManager *object_manager,
                       GDBusObject        *object,
                       gpointer            user_data)
{
    Bluetooth *self = BLUETOOTH (user_data);
    const char *path = g_dbus_object_get_object_path (object);
    g_autoptr (GDBusProxy) proxy = NULL;
    g_autoptr (GRegex) regex = g_regex_new (
        ".*dev_([0-9A-Fa-f]{2}_){5}([0-9A-Fa-f]{2})$",
        G_REGEX_DEFAULT,
        G_REGEX_MATCH_DEFAULT,
        NULL
    );
    g_autoptr (GVariant) value = NULL;
    g_autoptr (GError) error = NULL;

    if (!g_regex_match (regex, path, G_REGEX_MATCH_DEFAULT, NULL)) {
        return;
    }

    proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        0,
        NULL,
        BLUEZ_DBUS_NAME,
        path,
        BLUEZ_DBUS_DEVICE_INTERFACE,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't get Bluez object: %s", error->message);
        return;
    }

    g_signal_connect (
        proxy,
        "g-properties-changed",
        G_CALLBACK (on_bluez_proxy_properties),
        self
    );

    self->priv->connections = g_list_append (
        self->priv->connections, g_steal_pointer (&proxy)
    );
}

static void
on_bluez_object_removed (GDBusObjectManager *object_manager,
                         GDBusObject        *object,
                         gpointer            user_data)
{
    Bluetooth *self = BLUETOOTH (user_data);
    const char *object_path = g_dbus_object_get_object_path (object);
    GDBusProxy *proxy;

    GFOREACH (self->priv->connections, proxy) {
        const char *proxy_path = g_dbus_proxy_get_object_path (proxy);
        if (g_strcmp0 (proxy_path, object_path) == 0) {
            self->priv->connections = g_list_remove (
                self->priv->connections, proxy
            );
            g_clear_object (&proxy);
            break;
        }
    }
}

static void
on_bluez_proxy_properties (GDBusProxy  *proxy,
                           GVariant    *changed_properties,
                           char       **invalidated_properties,
                           gpointer     user_data)
{
    Bluetooth *self = BLUETOOTH (user_data);
    GVariant *value;
    char *property;
    GVariantIter i;

    g_variant_iter_init (&i, changed_properties);
    while (g_variant_iter_next (&i, "{&sv}", &property, &value)) {
        if (g_strcmp0 (property, "Powered") == 0) {
            if (!self->priv->powersaving)
                g_variant_get (value, "b", &self->priv->powered);
        } else if (g_strcmp0 (property, "Connected") == 0) {
            gboolean connected = g_variant_get_boolean (value);

            if (connected) {
                check_for_audio_device (self, proxy);
            }
        }
        g_variant_unref (value);
    }
}

static void
bluetooth_dispose (GObject *bluetooth)
{
    Bluetooth *self = BLUETOOTH (bluetooth);

    g_clear_object (&self->priv->object_manager);
    g_clear_object (&self->priv->bluez_proxy);

    G_OBJECT_CLASS (bluetooth_parent_class)->dispose (bluetooth);
}

static void
bluetooth_finalize (GObject *bluetooth)
{
    G_OBJECT_CLASS (bluetooth_parent_class)->finalize (bluetooth);
}

static void
bluetooth_class_init (BluetoothClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = bluetooth_dispose;
    object_class->finalize = bluetooth_finalize;

    signals[AUDIO_DEVICE_CONNECTED] = g_signal_new (
        "audio-device-connected",
        G_OBJECT_CLASS_TYPE (object_class),
        G_SIGNAL_RUN_LAST,
        0,
        NULL, NULL, NULL,
        G_TYPE_NONE,
        0
    );
}

static void
bluetooth_init (Bluetooth *self)
{
    g_autoptr (GVariant) value = NULL;
    g_autoptr (GError) error = NULL;

    self->priv = bluetooth_get_instance_private (self);

    self->priv->powered = FALSE;
    self->priv->powersaving = FALSE;
    self->priv->connections = NULL;

    self->priv->bluez_proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        0,
        NULL,
        BLUEZ_DBUS_NAME,
        BLUEZ_DBUS_PATH,
        BLUEZ_DBUS_ADAPTER_INTERFACE,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't contact Bluez: %s", error->message);
        return;
    }

    g_signal_connect (
        self->priv->bluez_proxy,
        "g-properties-changed",
        G_CALLBACK (on_bluez_proxy_properties),
        self
    );

    value = g_dbus_proxy_get_cached_property (
        self->priv->bluez_proxy, "Powered"
    );
    g_variant_get (value, "b", &self->priv->powered);

    self->priv->object_manager = g_dbus_object_manager_client_new_sync (
        g_dbus_proxy_get_connection (self->priv->bluez_proxy),
        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
        BLUEZ_DBUS_NAME,
        "/",
        NULL,
        NULL,
        NULL,
        NULL,
        &error
    );

    if (error != NULL) {
        g_warning ("Can't get object manager: %s", error->message);
        return;
    }

    g_signal_connect(
        self->priv->object_manager,
        "object-added",
        G_CALLBACK(on_bluez_object_added),
        self
    );
    g_signal_connect(
        self->priv->object_manager,
        "object-removed",
        G_CALLBACK(on_bluez_object_removed),
        self
    );
    check_existing_connections (self);
}

/**
 * bluetooth_new:
 *
 * Creates a new #Bluetooth
 *
 * Returns: (transfer full): a new #Bluetooth
 *
 **/
GObject *
bluetooth_new (void)
{
    GObject *bluetooth;

    bluetooth = g_object_new (TYPE_BLUETOOTH, NULL);

    return bluetooth;
}