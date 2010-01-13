/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* nm-dhcp-manager.c - Handle the DHCP daemon for NetworkManager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2005 - 2010 Red Hat, Inc.
 * Copyright (C) 2006 - 2008 Novell, Inc.
 *
 */


#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "nm-dhcp-manager.h"
#include "nm-dhcp-dhclient.h"
#include "nm-dhcp-dhcpcd.h"
#include "nm-marshal.h"
#include "nm-utils.h"
#include "nm-dbus-manager.h"
#include "nm-hostname-provider.h"
#include "nm-dbus-glib-types.h"
#include "nm-glib-compat.h"

#define NM_DHCP_CLIENT_DBUS_SERVICE "org.freedesktop.nm_dhcp_client"
#define NM_DHCP_CLIENT_DBUS_IFACE   "org.freedesktop.nm_dhcp_client"

static NMDHCPManager *singleton = NULL;

typedef GSList * (*GetLeaseConfigFunc) (const char *iface, const char *uuid);

typedef struct {
	GType               client_type;
	GetLeaseConfigFunc  get_lease_config_func;

	NMDBusManager *     dbus_mgr;
	GHashTable *        clients;
	DBusGProxy *        proxy;
	NMHostnameProvider *hostname_provider;
} NMDHCPManagerPrivate;


#define NM_DHCP_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DHCP_MANAGER, NMDHCPManagerPrivate))

G_DEFINE_TYPE (NMDHCPManager, nm_dhcp_manager, G_TYPE_OBJECT)

static char *
garray_to_string (GArray *array, const char *key)
{
	GString *str;
	int i;
	unsigned char c;
	char *converted = NULL;

	g_return_val_if_fail (array != NULL, NULL);

	/* Since the DHCP options come through environment variables, they should
	 * already be UTF-8 safe, but just make sure.
	 */
	str = g_string_sized_new (array->len);
	for (i = 0; i < array->len; i++) {
		c = array->data[i];

		/* Convert NULLs to spaces and non-ASCII characters to ? */
		if (c == '\0')
			c = ' ';
		else if (c > 127)
			c = '?';
		str = g_string_append_c (str, c);
	}
	str = g_string_append_c (str, '\0');

	converted = str->str;
	if (!g_utf8_validate (converted, -1, NULL))
		nm_warning ("%s: DHCP option '%s' couldn't be converted to UTF-8", __func__, key);
	g_string_free (str, FALSE);
	return converted;
}

static NMDHCPClient *
get_client_for_pid (NMDHCPManager *manager, GPid pid)
{
	NMDHCPManagerPrivate *priv;
	GHashTableIter iter;
	gpointer value;

	g_return_val_if_fail (manager != NULL, NULL);
	g_return_val_if_fail (NM_IS_DHCP_MANAGER (manager), NULL);

	priv = NM_DHCP_MANAGER_GET_PRIVATE (manager);

	g_hash_table_iter_init (&iter, priv->clients);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		NMDHCPClient *candidate = NM_DHCP_CLIENT (value);

		if (nm_dhcp_client_get_pid (candidate) == pid)
			return candidate;
	}

	return NULL;
}

static NMDHCPClient *
get_client_for_iface (NMDHCPManager *manager,
                      const char *iface)
{
	NMDHCPManagerPrivate *priv;
	GHashTableIter iter;
	gpointer value;

	g_return_val_if_fail (manager != NULL, NULL);
	g_return_val_if_fail (NM_IS_DHCP_MANAGER (manager), NULL);
	g_return_val_if_fail (iface, NULL);

	priv = NM_DHCP_MANAGER_GET_PRIVATE (manager);

	g_hash_table_iter_init (&iter, priv->clients);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		NMDHCPClient *candidate = NM_DHCP_CLIENT (value);

		if (!strcmp (iface, nm_dhcp_client_get_iface (candidate)))
			return candidate;
	}

	return NULL;
}

static char *
get_option (GHashTable *hash, const char *key)
{
	GValue *value;

	value = g_hash_table_lookup (hash, key);
	if (value == NULL)
		return NULL;

	if (G_VALUE_TYPE (value) != DBUS_TYPE_G_UCHAR_ARRAY) {
		g_warning ("Unexpected key %s value type was not "
		           "DBUS_TYPE_G_UCHAR_ARRAY",
		           (char *) key);
		return NULL;
	}

	return garray_to_string ((GArray *) g_value_get_boxed (value), key);
}

static void
nm_dhcp_manager_handle_event (DBusGProxy *proxy,
                              GHashTable *options,
                              gpointer user_data)
{
	NMDHCPManager *manager;
	NMDHCPManagerPrivate *priv;
	NMDHCPClient *client;
	char *iface = NULL;
	char *pid_str = NULL;
	char *reason = NULL;
	unsigned long temp;

	manager = NM_DHCP_MANAGER (user_data);
	priv = NM_DHCP_MANAGER_GET_PRIVATE (manager);

	iface = get_option (options, "interface");
	if (iface == NULL) {
		nm_warning ("DHCP event didn't have associated interface.");
		goto out;
	}

	pid_str = get_option (options, "pid");
	if (pid_str == NULL) {
		nm_warning ("DHCP event didn't have associated PID.");
		goto out;
	}

	temp = strtoul (pid_str, NULL, 10);
	if ((temp == ULONG_MAX) && (errno == ERANGE)) {
		nm_warning ("Couldn't convert PID");
		goto out;
	}

	client = get_client_for_pid (manager, (GPid) temp);
	if (client == NULL) {
		nm_warning ("Unhandled DHCP event for interface %s", iface);
		goto out;
	}

	if (strcmp (iface, nm_dhcp_client_get_iface (client))) {
		nm_warning ("Received DHCP event from unexpected interface '%s' (expected '%s')",
		            iface, nm_dhcp_client_get_iface (client));
		goto out;
	}

	reason = get_option (options, "reason");
	if (reason == NULL) {
		nm_warning ("DHCP event didn't have a reason");
		goto out;
	}

	nm_dhcp_client_new_options (client, options, reason);

out:
	g_free (iface);
	g_free (pid_str);
	g_free (reason);
}

NMDHCPManager *
nm_dhcp_manager_new (const char *client, GError **error)
{
	NMDHCPManagerPrivate *priv;
	DBusGConnection *g_connection;

	g_warn_if_fail (singleton == NULL);
	g_return_val_if_fail (client != NULL, NULL);

	singleton = g_object_new (NM_TYPE_DHCP_MANAGER, NULL);
	priv = NM_DHCP_MANAGER_GET_PRIVATE (singleton);

	/* Figure out which DHCP client to use */
	if (!strcmp (client, "dhclient") && strlen (DHCLIENT_PATH)) {
		priv->client_type = NM_TYPE_DHCP_DHCLIENT;
		priv->get_lease_config_func = nm_dhcp_dhclient_get_lease_config;
	} else if (!strcmp (client, "dhcpcd") && strlen (DHCPCD_PATH)) {
		priv->client_type = NM_TYPE_DHCP_DHCPCD;
		priv->get_lease_config_func = nm_dhcp_dhcpcd_get_lease_config;
	} else {
		g_set_error (error, 0, 0, "unknown or missing DHCP client '%s'", client);
		goto error;
	}

	priv->clients = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                                       NULL,
	                                       (GDestroyNotify) g_object_unref);
	if (!priv->clients) {
		g_set_error_literal (error, 0, 0, "not enough memory to initialize DHCP manager");
		goto error;
	}

	priv->dbus_mgr = nm_dbus_manager_get ();
	g_connection = nm_dbus_manager_get_connection (priv->dbus_mgr);
	priv->proxy = dbus_g_proxy_new_for_name (g_connection,
	                                         NM_DHCP_CLIENT_DBUS_SERVICE,
	                                         "/",
	                                         NM_DHCP_CLIENT_DBUS_IFACE);
	if (!priv->proxy) {
		g_set_error_literal (error, 0, 0, "not enough memory to initialize DHCP manager proxy");
		goto error;
	}

	dbus_g_proxy_add_signal (priv->proxy,
	                         "Event",
	                         DBUS_TYPE_G_MAP_OF_VARIANT,
	                         G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (priv->proxy, "Event",
	                             G_CALLBACK (nm_dhcp_manager_handle_event),
	                             singleton,
	                             NULL);

	return singleton;

error:
	g_object_unref (singleton);
	singleton = NULL;
	return singleton;
}

#define STATE_ID_TAG "state-id"
#define TIMEOUT_ID_TAG "timeout-id"

static void
remove_client (NMDHCPManager *self, NMDHCPClient *client)
{
	NMDHCPManagerPrivate *priv = NM_DHCP_MANAGER_GET_PRIVATE (self);
	guint id;

	id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (client), STATE_ID_TAG));
	if (id)
		g_signal_handler_disconnect (client, id);

	id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (client), TIMEOUT_ID_TAG));
	if (id)
		g_signal_handler_disconnect (client, id);

	/* Stopping the client is left up to the controlling device
	 * explicitly since we may want to quit NetworkManager but not terminate
	 * the DHCP client.
	 */

	g_hash_table_remove (priv->clients, client);
}

static void
client_state_changed (NMDHCPClient *client, NMDHCPState new_state, gpointer user_data)
{
	if (new_state == DHC_ABEND || new_state == DHC_END)
		remove_client (NM_DHCP_MANAGER (user_data), client);
}

static void
client_timeout (NMDHCPClient *client, gpointer user_data)
{
	remove_client (NM_DHCP_MANAGER (user_data), client);
}

static void
add_client (NMDHCPManager *self, NMDHCPClient *client)
{
	NMDHCPManagerPrivate *priv = NM_DHCP_MANAGER_GET_PRIVATE (self);
	guint id;

	id = g_signal_connect (client, "state-changed", G_CALLBACK (client_state_changed), self);
	g_object_set_data (G_OBJECT (client), STATE_ID_TAG, GUINT_TO_POINTER (id));

	id = g_signal_connect (client, "timeout", G_CALLBACK (client_timeout), self);
	g_object_set_data (G_OBJECT (client), TIMEOUT_ID_TAG, GUINT_TO_POINTER (id));

	g_hash_table_insert (priv->clients, client, g_object_ref (client));
}

/* Caller owns a reference to the NMDHCPClient on return */
NMDHCPClient *
nm_dhcp_manager_start_client (NMDHCPManager *self,
                              const char *iface,
                              const char *uuid,
                              NMSettingIP4Config *s_ip4,
                              guint32 timeout,
                              guint8 *dhcp_anycast_addr)
{
	NMDHCPManagerPrivate *priv;
	NMDHCPClient *client;
	NMSettingIP4Config *setting = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (NM_IS_DHCP_MANAGER (self), NULL);
	g_return_val_if_fail (iface != NULL, NULL);

	priv = NM_DHCP_MANAGER_GET_PRIVATE (self);

	/* Kill any old client instance */
	client = get_client_for_iface (self, iface);
	if (client) {
		nm_dhcp_client_stop (client);
		remove_client (self, client);
	}

	/* And make a new one */
	client = g_object_new (NM_TYPE_DHCP_DHCLIENT,
	                       NM_DHCP_CLIENT_INTERFACE, iface,
	                       NULL);
	g_return_val_if_fail (client != NULL, NULL);
	add_client (self, client);

	if (   s_ip4
	    && nm_setting_ip4_config_get_dhcp_send_hostname (s_ip4)
		&& (nm_setting_ip4_config_get_dhcp_hostname (s_ip4) == NULL)
		&& priv->hostname_provider != NULL) {

		/* We're asked to send the hostname to DHCP server, the hostname
		 * isn't specified, and a hostname provider is registered: use that
		 */
		setting = NM_SETTING_IP4_CONFIG (nm_setting_duplicate (NM_SETTING (s_ip4)));
		g_object_set (G_OBJECT (setting),
					  NM_SETTING_IP4_CONFIG_DHCP_HOSTNAME,
					  nm_hostname_provider_get_hostname (priv->hostname_provider),
					  NULL);
	}

	success = nm_dhcp_client_start (client, uuid, setting, timeout, dhcp_anycast_addr);

	if (setting)
		g_object_unref (setting);

	if (!success) {
		remove_client (self, client);
		g_object_unref (client);
		client = NULL;
	}

	return client;
}

static void
hostname_provider_destroyed (gpointer data, GObject *destroyed_object)
{
	NM_DHCP_MANAGER_GET_PRIVATE (data)->hostname_provider = NULL;
}

void
nm_dhcp_manager_set_hostname_provider (NMDHCPManager *manager,
									   NMHostnameProvider *provider)
{
	NMDHCPManagerPrivate *priv;

	g_return_if_fail (NM_IS_DHCP_MANAGER (manager));

	priv = NM_DHCP_MANAGER_GET_PRIVATE (manager);

	if (priv->hostname_provider) {
		g_object_weak_unref (G_OBJECT (priv->hostname_provider), hostname_provider_destroyed, manager);
		priv->hostname_provider = NULL;
	}

	if (provider) {
		priv->hostname_provider = provider;
		g_object_weak_ref (G_OBJECT (provider), hostname_provider_destroyed, manager);
	}
}

GSList *
nm_dhcp_manager_get_lease_config (NMDHCPManager *self,
                                  const char *iface,
                                  const char *uuid)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (NM_IS_DHCP_MANAGER (self), NULL);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (uuid != NULL, NULL);

	return NM_DHCP_MANAGER_GET_PRIVATE (self)->get_lease_config_func (iface, uuid);
}

NMIP4Config *
nm_dhcp_manager_test_ip4_options_to_config (const char *iface,
                                            GHashTable *options,
                                            const char *reason)
{
	NMDHCPClient *client;
	NMIP4Config *config;

	client = (NMDHCPClient *) g_object_new (NM_TYPE_DHCP_DHCLIENT,
	                                        NM_DHCP_CLIENT_INTERFACE, iface,
	                                        NULL);
	g_return_val_if_fail (client != NULL, NULL);
	nm_dhcp_client_new_options (client, options, reason);
	config = nm_dhcp_client_get_ip4_config (client, TRUE);
	g_object_unref (client);

	return config;
}

/***************************************************/

NMDHCPManager *
nm_dhcp_manager_get (void)
{
	g_warn_if_fail (singleton != NULL);
	return g_object_ref (singleton);
}

static void
nm_dhcp_manager_init (NMDHCPManager *manager)
{
}

static void
dispose (GObject *object)
{
	NMDHCPManagerPrivate *priv = NM_DHCP_MANAGER_GET_PRIVATE (object);
	GList *values, *iter;

	if (priv->clients) {
		values = g_hash_table_get_values (priv->clients);
		for (iter = values; iter; iter = g_list_next (iter))
			remove_client (NM_DHCP_MANAGER (object), NM_DHCP_CLIENT (iter->data));
		g_list_free (values);
	}

	G_OBJECT_CLASS (nm_dhcp_manager_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	NMDHCPManagerPrivate *priv = NM_DHCP_MANAGER_GET_PRIVATE (object);

	if (priv->hostname_provider) {
		g_object_weak_unref (G_OBJECT (priv->hostname_provider), hostname_provider_destroyed, object);
		priv->hostname_provider = NULL;
	}

	if (priv->clients)
		g_hash_table_destroy (priv->clients);
	if (priv->proxy)
		g_object_unref (priv->proxy);
	if (priv->dbus_mgr)
		g_object_unref (priv->dbus_mgr);

	G_OBJECT_CLASS (nm_dhcp_manager_parent_class)->finalize (object);
}

static void
nm_dhcp_manager_class_init (NMDHCPManagerClass *manager_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (manager_class);

	g_type_class_add_private (manager_class, sizeof (NMDHCPManagerPrivate));

	/* virtual methods */
	object_class->finalize = finalize;
	object_class->dispose = dispose;
}

