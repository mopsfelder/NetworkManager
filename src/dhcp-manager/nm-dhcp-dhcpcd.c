/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* nm-dhcp-dhcpcd.c - dhcpcd specific hooks for NetworkManager
 *
 * Copyright (C) 2008 Roy Marples
 * Copyright (C) 2010 Dan Williams <dcbw@redhat.com>
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
 */


#include <glib.h>
#include <glib/gi18n.h>
#include <dbus/dbus.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nm-dhcp-dhcpcd.h"
#include "nm-utils.h"

G_DEFINE_TYPE (NMDHCPDhcpcd, nm_dhcp_dhcpcd, NM_TYPE_DHCP_DHCPCD)

#define NM_DHCP_DHCPCD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DHCP_DHCPCD, NMDHCPDhcpcdPrivate))

#define ACTION_SCRIPT_PATH	LIBEXECDIR "/nm-dhcp-client.action"

typedef struct {
	char *pid_file;
} NMDHCPDhcpcdPrivate;


GSList *
nm_dhcp_dhcpcd_get_lease_config (const char *iface, const char *uuid)
{
	return NULL;
}

static void
dhcpcd_child_setup (gpointer user_data G_GNUC_UNUSED)
{
	/* We are in the child process at this point */
	pid_t pid = getpid ();
	setpgid (pid, pid);
}

static GPid
real_ip4_start (NMDHCPClient *client,
                const char *uuid,
                NMSettingIP4Config *s_ip4,
                guint8 *dhcp_anycast_addr)
{
	NMDHCPDhcpcdPrivate *priv = NM_DHCP_DHCPCD_GET_PRIVATE (client);
	GPtrArray *argv = NULL;
	GPid pid = 0;
	GError *error = NULL;
	char *pid_contents = NULL, *binary_name;
	const char *iface;

	g_return_val_if_fail (priv->pid_file == NULL, -1);

	iface = nm_dhcp_client_get_iface (client);

	priv->pid_file = g_strdup_printf (LOCALSTATEDIR "/run/dhcpcd-%s.pid", iface);
	if (!priv->pid_file) {
		nm_warning ("%s: not enough memory for dhcpcd options.", iface);
		return -1;
	}

	if (!g_file_test (DHCPCD_PATH, G_FILE_TEST_EXISTS)) {
		nm_warning (DHCPCD_PATH " does not exist.");
		return -1;
	}

	/* Kill any existing dhclient from the pidfile */
	binary_name = g_path_get_basename (DHCPCD_PATH);
	nm_dhcp_client_stop_existing (priv->pid_file, binary_name);
	g_free (binary_name);

	argv = g_ptr_array_new ();
	g_ptr_array_add (argv, (gpointer) DHCPCD_PATH);

	g_ptr_array_add (argv, (gpointer) "-B");	/* Don't background on lease (disable fork()) */

	g_ptr_array_add (argv, (gpointer) "-K");	/* Disable built-in carrier detection */

	g_ptr_array_add (argv, (gpointer) "-L");	/* Disable built-in IPv4LL since we use avahi-autoipd */

	g_ptr_array_add (argv, (gpointer) "-c");	/* Set script file */
	g_ptr_array_add (argv, (gpointer) ACTION_SCRIPT_PATH );

	g_ptr_array_add (argv, (gpointer) iface);
	g_ptr_array_add (argv, NULL);

	if (!g_spawn_async (NULL, (char **) argv->pdata, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
	                    &dhcpcd_child_setup, NULL, &pid, &error)) {
		nm_warning ("dhcpcd failed to start.  error: '%s'", error->message);
		g_error_free (error);
	} else
		nm_info ("dhcpcd started with pid %d", pid);

	g_free (pid_contents);
	g_ptr_array_free (argv, TRUE);
	return pid;
}

static void
real_stop (NMDHCPClient *client)
{
	NMDHCPDhcpcdPrivate *priv = NM_DHCP_DHCPCD_GET_PRIVATE (client);

	/* Chain up to parent */
	NM_DHCP_CLIENT_CLASS (nm_dhcp_dhcpcd_parent_class)->stop (client);

	if (priv->pid_file)
		remove (priv->pid_file);
}

static gboolean
real_ip4_process_classless_routes (NMDHCPClient *client,
                                   GHashTable *options,
                                   NMIP4Config *ip4_config,
                                   guint32 *gwaddr)
{
	const char *str;
	char **routes, **r;
	gboolean have_routes = FALSE;

	/* Classless static routes over-ride any static routes and routers
	 * provided. We should also check for MS classless static routes as
	 * they implemented the draft RFC using their own code.
	 */
	str = g_hash_table_lookup (options, "new_classless_static_routes");
	if (!str)
		str = g_hash_table_lookup (options, "new_ms_classless_static_routes");

	if (!str || !strlen (str))
		return FALSE;

	routes = g_strsplit (str, " ", 0);
	if (g_strv_length (routes) == 0)
		goto out;

	if ((g_strv_length (routes) % 2) != 0) {
		nm_info ("  classless static routes provided, but invalid");
		goto out;
	}

	for (r = routes; *r; r += 2) {
		char *slash;
		NMIP4Route *route;
		int rt_cidr = 32;
		struct in_addr rt_addr;
		struct in_addr rt_route;

		slash = strchr(*r, '/');
		if (slash) {
			*slash = '\0';
			errno = 0;
			rt_cidr = strtol (slash + 1, NULL, 10);
			if ((errno == EINVAL) || (errno == ERANGE)) {
				nm_warning ("DHCP provided invalid classless static route cidr: '%s'", slash + 1);
				continue;
			}
		}
		if (inet_pton (AF_INET, *r, &rt_addr) <= 0) {
			nm_warning ("DHCP provided invalid classless static route address: '%s'", *r);
			continue;
		}
		if (inet_pton (AF_INET, *(r + 1), &rt_route) <= 0) {
			nm_warning ("DHCP provided invalid classless static route gateway: '%s'", *(r + 1));
			continue;
		}

		have_routes = TRUE;
		if (rt_cidr == 0 && rt_addr.s_addr == 0) {
			/* FIXME: how to handle multiple routers? */
			*gwaddr = rt_addr.s_addr;
		} else {
			route = nm_ip4_route_new ();
			nm_ip4_route_set_dest (route, (guint32) rt_addr.s_addr);
			nm_ip4_route_set_prefix (route, rt_cidr);
			nm_ip4_route_set_next_hop (route, (guint32) rt_route.s_addr);


			nm_ip4_config_take_route (ip4_config, route);
			nm_info ("  classless static route %s/%d gw %s", *r, rt_cidr, *(r + 1));
		}
	}

out:
	g_strfreev (routes);
	return have_routes;
}

/***************************************************/

static void
nm_dhcp_dhcpcd_init (NMDHCPDhcpcd *self)
{
}

static void
dispose (GObject *object)
{
	NMDHCPDhcpcdPrivate *priv = NM_DHCP_DHCPCD_GET_PRIVATE (object);

	g_free (priv->pid_file);

	G_OBJECT_CLASS (nm_dhcp_dhcpcd_parent_class)->dispose (object);
}

static void
nm_dhcp_dhcpcd_class_init (NMDHCPDhcpcdClass *dhcpcd_class)
{
	NMDHCPClientClass *client_class = NM_DHCP_CLIENT_CLASS (dhcpcd_class);
	GObjectClass *object_class = G_OBJECT_CLASS (dhcpcd_class);

	g_type_class_add_private (dhcpcd_class, sizeof (NMDHCPDhcpcdPrivate));

	/* virtual methods */
	object_class->dispose = dispose;

	client_class->ip4_start = real_ip4_start;
	client_class->stop = real_stop;
	client_class->ip4_process_classless_routes = real_ip4_process_classless_routes;
}

