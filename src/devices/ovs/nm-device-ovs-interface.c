/* NetworkManager -- Network link manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * Copyright 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include "nm-device-ovs-interface.h"
#include "nm-ovsdb.h"

#include "devices/nm-device-private.h"
#include "nm-active-connection.h"
#include "nm-setting-connection.h"
#include "nm-setting-ovs-interface.h"
#include "nm-setting-ovs-port.h"

#include "introspection/org.freedesktop.NetworkManager.Device.OvsInterface.h"

#include "devices/nm-device-logging.h"
_LOG_DECLARE_SELF(NMDeviceOvsInterface);

/*****************************************************************************/

struct _NMDeviceOvsInterface {
	NMDevice parent;
};

struct _NMDeviceOvsInterfaceClass {
	NMDeviceClass parent;
};

G_DEFINE_TYPE (NMDeviceOvsInterface, nm_device_ovs_interface, NM_TYPE_DEVICE)

/*****************************************************************************/

static const char *
get_type_description (NMDevice *device)
{
	return "ovs-interface";
}

static gboolean
create_and_realize (NMDevice *device,
                    NMConnection *connection,
                    NMDevice *parent,
                    const NMPlatformLink **out_plink,
                    GError **error)
{
	/* The actual backing resources will be created once an interface is
	 * added to a port of ours, since there can be neither an empty port nor
	 * an empty bridge. */

	return TRUE;
}

static NMDeviceCapabilities
get_generic_capabilities (NMDevice *device)
{
	return NM_DEVICE_CAP_CARRIER_DETECT | NM_DEVICE_CAP_IS_SOFTWARE;
}

static gboolean
check_connection_compatible (NMDevice *device, NMConnection *connection)
{
	NMSettingConnection *s_con;
	NMSettingOvsInterface *s_ovs_iface;

	if (!NM_DEVICE_CLASS (nm_device_ovs_interface_parent_class)->check_connection_compatible (device, connection))
		return FALSE;

	s_ovs_iface = nm_connection_get_setting_ovs_interface (connection);
	if (!s_ovs_iface)
		return FALSE;
	if (g_strcmp0 (nm_setting_ovs_interface_get_interface_type (s_ovs_iface),
	               "internal") != 0) {
		return FALSE;
	}

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);
	if (g_strcmp0 (nm_setting_connection_get_connection_type (s_con),
	               NM_SETTING_OVS_INTERFACE_SETTING_NAME) != 0) {
		return FALSE;
	}

	return TRUE;
}

static void
link_changed (NMDevice *device,
              const NMPlatformLink *pllink)
{
	if (nm_device_get_state (device) == NM_DEVICE_STATE_IP_CONFIG)
		nm_device_activate_schedule_stage3_ip_config_start (device);
}

static NMActStageReturn
act_stage3_ip4_config_start (NMDevice *device,
                             NMIP4Config **out_config,
                             NMDeviceStateReason *out_failure_reason)
{
	if (!nm_device_get_ip_ifindex (device))
		return NM_ACT_STAGE_RETURN_POSTPONE;

	return NM_DEVICE_CLASS (nm_device_ovs_interface_parent_class)->act_stage3_ip4_config_start (device, out_config, out_failure_reason);
}

static NMActStageReturn
act_stage3_ip6_config_start (NMDevice *device,
                             NMIP6Config **out_config,
                             NMDeviceStateReason *out_failure_reason)
{
	if (!nm_device_get_ip_ifindex (device))
		return NM_ACT_STAGE_RETURN_POSTPONE;

	return NM_DEVICE_CLASS (nm_device_ovs_interface_parent_class)->act_stage3_ip6_config_start (device, out_config, out_failure_reason);
}


/*****************************************************************************/

static void
nm_device_ovs_interface_init (NMDeviceOvsInterface *self)
{
}

static void
nm_device_ovs_interface_class_init (NMDeviceOvsInterfaceClass *klass)
{
	NMDeviceClass *device_class = NM_DEVICE_CLASS (klass);

	NM_DEVICE_CLASS_DECLARE_TYPES (klass, NULL, NM_LINK_TYPE_OPENVSWITCH);

	device_class->get_type_description = get_type_description;
	device_class->create_and_realize = create_and_realize;
	device_class->get_generic_capabilities = get_generic_capabilities;
	device_class->check_connection_compatible = check_connection_compatible;
	device_class->link_changed = link_changed;
	device_class->act_stage3_ip4_config_start = act_stage3_ip4_config_start;
	device_class->act_stage3_ip6_config_start = act_stage3_ip6_config_start;

	nm_exported_object_class_add_interface (NM_EXPORTED_OBJECT_CLASS (klass),
	                                        NMDBUS_TYPE_DEVICE_OVS_INTERFACE_SKELETON,
	                                        NULL);
}
