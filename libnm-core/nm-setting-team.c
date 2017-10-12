/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright 2013 Jiri Pirko <jiri@resnulli.us>
 */

#include "nm-default.h"

#include <string.h>
#include <stdlib.h>

#include "nm-setting-team.h"
#include "nm-utils.h"
#include "nm-utils-private.h"
#include "nm-connection-private.h"

/**
 * SECTION:nm-setting-team
 * @short_description: Describes connection properties for teams
 *
 * The #NMSettingTeam object is a #NMSetting subclass that describes properties
 * necessary for team connections.
 **/

G_DEFINE_TYPE_WITH_CODE (NMSettingTeam, nm_setting_team, NM_TYPE_SETTING,
                         _nm_register_setting (TEAM, NM_SETTING_PRIORITY_HW_BASE))
NM_SETTING_REGISTER_TYPE (NM_TYPE_SETTING_TEAM)

#define NM_SETTING_TEAM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_SETTING_TEAM, NMSettingTeamPrivate))

typedef struct {
	char *config;
	gint notifypeers_count;
	gint notifypeers_interval;
	gint mcastrejoin_count;
	gint mcastrejoin_interval;
	char *runner;
	char *runner_hwpolicy;
	GHashTable *runner_txhash;
	char *runner_txbalancer;
	gint runner_txbalancer_interval;
	gboolean runner_active;
	gboolean runner_fastrate;
	gint runner_sysprio;
	gint runner_minports;
	char *runner_aggselectpolicy;
} NMSettingTeamPrivate;

enum {
	PROP_0,
	PROP_CONFIG,
	PROP_NOTIFYPEERS_COUNT,
	PROP_NOTIFYPEERS_INTERVAL,
	PROP_MCASTREJOIN_COUNT,
	PROP_MCASTREJOIN_INTERVAL,
	PROP_RUNNER,
	PROP_RUNNER_HWPOLICY,
	PROP_RUNNER_TXHASH,
	PROP_RUNNER_TXBALANCER,
	PROP_RUNNER_TXBALANCER_INTERVAL,
	PROP_RUNNER_ACTIVE,
	PROP_RUNNER_FASTRATE,
	PROP_RUNNER_SYSPRIO,
	PROP_RUNNER_MINPORTS,
	PROP_RUNNER_AGGSELECTPOLICY,
	LAST_PROP
};

/**
 * nm_setting_team_new:
 *
 * Creates a new #NMSettingTeam object with default values.
 *
 * Returns: (transfer full): the new empty #NMSettingTeam object
 **/
NMSetting *
nm_setting_team_new (void)
{
	return (NMSetting *) g_object_new (NM_TYPE_SETTING_TEAM, NULL);
}

/**
 * nm_setting_team_get_config:
 * @setting: the #NMSettingTeam
 *
 * Returns: the #NMSettingTeam:config property of the setting
 **/
const char *
nm_setting_team_get_config (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), NULL);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->config;
}

/**
 * nm_setting_team_get_notifypeers_count:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:notifypeers_count property of the setting
 **/
gint
nm_setting_team_get_notifypeers_count (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->notifypeers_count;
}

/**
 * nm_setting_team_get_notifypeers_interval:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:notifypeers_interval property of the setting
 **/
gint
nm_setting_team_get_notifypeers_interval (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->notifypeers_interval;
}

/**
 * nm_setting_team_get_mcastrejoin_count:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:mcastrejoin_count property of the setting
 **/
gint
nm_setting_team_get_mcastrejoin_count (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->mcastrejoin_count;
}

/**
 * nm_setting_team_get_mcastrejoin_interval:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:mcastrejoin_interval property of the setting
 **/
gint
nm_setting_team_get_mcastrejoin_interval (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->mcastrejoin_interval;
}

/**
 * nm_setting_team_get_runner:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner property of the setting
 **/
const char *
nm_setting_team_get_runner (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), NULL);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner;
}

/**
 * nm_setting_team_get_runner_hwpolicy:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_hwpolicy property of the setting
 **/
const char *
nm_setting_team_get_runner_hwpolicy (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), NULL);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_hwpolicy;
}

/**
 * nm_setting_team_get_runner_txbalancer:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_txbalancer property of the setting
 **/
const char *
nm_setting_team_get_runner_txbalancer (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), NULL);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_txbalancer;
}

/**
 * nm_setting_team_get_runner_txbalancer_interval:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_txbalancer_interval property of the setting
 **/
gint
nm_setting_team_get_runner_txbalancer_interval (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_txbalancer_interval;
}

/**
 * nm_setting_team_get_runner_active:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_active property of the setting
 **/
gboolean
nm_setting_team_get_runner_active (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), FALSE);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_active;
}

/**
 * nm_setting_team_get_runner_fastrate:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_fastrate property of the setting
 **/
gboolean
nm_setting_team_get_runner_fastrate (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), FALSE);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_fastrate;
}

/**
 * nm_setting_team_get_runner_sysprio:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_sysprio property of the setting
 **/
gint
nm_setting_team_get_runner_sysprio (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_sysprio;
}

/**
 * nm_setting_team_get_runner_minports:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_minports property of the setting
 **/
gint
nm_setting_team_get_runner_minports (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), 0);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_minports;
}

/**
 * nm_setting_team_get_runner_aggselectpolicy:
 * @setting: the #NMSettingTeam
 *
 * Returns: the ##NMSettingTeam:runner_aggselectpolicy property of the setting
 **/
const char *
nm_setting_team_get_runner_aggselectpolicy (NMSettingTeam *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_TEAM (setting), NULL);

	return NM_SETTING_TEAM_GET_PRIVATE (setting)->runner_aggselectpolicy;
}

static gboolean
verify (NMSetting *setting, NMConnection *connection, GError **error)
{
	NMSettingTeamPrivate *priv = NM_SETTING_TEAM_GET_PRIVATE (setting);

	if (!_nm_connection_verify_required_interface_name (connection, error))
		return FALSE;

	if (priv->config) {
		if (strlen (priv->config) > 1*1024*1024) {
			g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_PROPERTY,
			             _("team config exceeds size limit"));
			g_prefix_error (error,
			                "%s.%s: ",
			                NM_SETTING_TEAM_SETTING_NAME,
			                NM_SETTING_TEAM_CONFIG);
			return FALSE;
		}

		if (!nm_utils_is_json_object (priv->config, error)) {
			g_prefix_error (error,
			                "%s.%s: ",
			                NM_SETTING_TEAM_SETTING_NAME,
			                NM_SETTING_TEAM_CONFIG);
			/* We treat an empty string as no config for compatibility. */
			return *priv->config ? FALSE : NM_SETTING_VERIFY_NORMALIZABLE;
		}
	}

	if (   priv->runner
	    && g_ascii_strcasecmp (priv->runner, NM_SETTING_TEAM_RUNNER_BROADCAST)
	    && g_ascii_strcasecmp (priv->runner, NM_SETTING_TEAM_RUNNER_ROUNDROBIN)
	    && g_ascii_strcasecmp (priv->runner, NM_SETTING_TEAM_RUNNER_ACTIVEBACKUP)
	    && g_ascii_strcasecmp (priv->runner, NM_SETTING_TEAM_RUNNER_LOADBALANCE)
	    && g_ascii_strcasecmp (priv->runner, NM_SETTING_TEAM_RUNNER_LACP)) {
		g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_SETTING,
		                     _("invalid runner \"%s\""), priv->runner);

		g_prefix_error (error, "%s.%s: ", nm_setting_get_name (setting), NM_SETTING_TEAM_RUNNER);
		return FALSE;
	}

	/* NOTE: normalizable/normalizable-errors must appear at the end with decreasing severity.
	 * Take care to properly order statements with priv->config above. */

	return TRUE;
}

static gboolean
compare_property (NMSetting *setting,
                  NMSetting *other,
                  const GParamSpec *prop_spec,
                  NMSettingCompareFlags flags)
{
	NMSettingClass *parent_class;

	/* If we are trying to match a connection in order to assume it (and thus
	 * @flags contains INFERRABLE), use the "relaxed" matching for team
	 * configuration. Otherwise, for all other purposes (including connection
	 * comparison before an update), resort to the default string comparison.
	 */
	if (   NM_FLAGS_HAS (flags, NM_SETTING_COMPARE_FLAG_INFERRABLE)
	    && nm_streq0 (prop_spec->name, NM_SETTING_TEAM_CONFIG)) {
		return _nm_utils_team_config_equal (NM_SETTING_TEAM_GET_PRIVATE (setting)->config,
		                                    NM_SETTING_TEAM_GET_PRIVATE (other)->config,
		                                    FALSE);
	}

	/* Otherwise chain up to parent to handle generic compare */
	parent_class = NM_SETTING_CLASS (nm_setting_team_parent_class);
	return parent_class->compare_property (setting, other, prop_spec, flags);
}

static void
nm_setting_team_init (NMSettingTeam *setting)
{
}

static void
finalize (GObject *object)
{
	NMSettingTeamPrivate *priv = NM_SETTING_TEAM_GET_PRIVATE (object);

	g_free (priv->config);
	g_free (priv->runner);
	g_free (priv->runner_hwpolicy);
	g_free (priv->runner_txbalancer);
	g_free (priv->runner_aggselectpolicy);

	G_OBJECT_CLASS (nm_setting_team_parent_class)->finalize (object);
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	NMSettingTeamPrivate *priv = NM_SETTING_TEAM_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONFIG:
		g_free (priv->config);
		priv->config = g_value_dup_string (value);
		break;
	case PROP_NOTIFYPEERS_COUNT:
		break;
	case PROP_NOTIFYPEERS_INTERVAL:
		break;
	case PROP_MCASTREJOIN_COUNT:
		break;
	case PROP_MCASTREJOIN_INTERVAL:
		break;
	case PROP_RUNNER:
		g_free (priv->runner);
		priv->runner = g_value_dup_string (value);
		break;
	case PROP_RUNNER_HWPOLICY:
		break;
	case PROP_RUNNER_TXHASH:
		break;
	case PROP_RUNNER_TXBALANCER:
		break;
	case PROP_RUNNER_TXBALANCER_INTERVAL:
		break;
	case PROP_RUNNER_ACTIVE:
		break;
	case PROP_RUNNER_FASTRATE:
		break;
	case PROP_RUNNER_SYSPRIO:
		break;
	case PROP_RUNNER_MINPORTS:
		break;
	case PROP_RUNNER_AGGSELECTPOLICY:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	NMSettingTeam *setting = NM_SETTING_TEAM (object);
	NMSettingTeamPrivate *priv = NM_SETTING_TEAM_GET_PRIVATE (setting);

	switch (prop_id) {
	case PROP_CONFIG:
		g_value_set_string (value, nm_setting_team_get_config (setting));
		break;
	case PROP_NOTIFYPEERS_COUNT:
		g_value_set_int (value, priv->notifypeers_count);
		break;
	case PROP_NOTIFYPEERS_INTERVAL:
		g_value_set_int (value, priv->notifypeers_interval);
		break;
	case PROP_MCASTREJOIN_COUNT:
		g_value_set_int (value, priv->mcastrejoin_count);
		break;
	case PROP_MCASTREJOIN_INTERVAL:
		g_value_set_int (value, priv->mcastrejoin_interval);
		break;
	case PROP_RUNNER:
		g_value_set_string (value, nm_setting_team_get_runner (setting));
		break;
	case PROP_RUNNER_HWPOLICY:
		g_value_set_string (value, nm_setting_team_get_runner_hwpolicy (setting));
		break;
	case PROP_RUNNER_TXHASH:
		g_value_take_boxed (value, _nm_utils_copy_strdict (priv->runner_txhash));
		break;
	case PROP_RUNNER_TXBALANCER:
		g_value_set_string (value, nm_setting_team_get_runner_txbalancer (setting));
		break;
	case PROP_RUNNER_TXBALANCER_INTERVAL:
		g_value_set_int (value, priv->runner_txbalancer_interval);
		break;
	case PROP_RUNNER_ACTIVE:
		g_value_set_boolean (value, nm_setting_team_get_runner_active (setting));
		break;
	case PROP_RUNNER_FASTRATE:
		g_value_set_boolean (value, nm_setting_team_get_runner_fastrate (setting));
		break;
	case PROP_RUNNER_SYSPRIO:
		g_value_set_int (value, priv->runner_sysprio);
		break;
	case PROP_RUNNER_MINPORTS:
		g_value_set_int (value, priv->runner_minports);
		break;
	case PROP_RUNNER_AGGSELECTPOLICY:
		g_value_set_string (value, nm_setting_team_get_runner_aggselectpolicy (setting));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_setting_team_class_init (NMSettingTeamClass *setting_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (setting_class);
	NMSettingClass *parent_class = NM_SETTING_CLASS (setting_class);

	g_type_class_add_private (setting_class, sizeof (NMSettingTeamPrivate));

	/* virtual methods */
	object_class->set_property     = set_property;
	object_class->get_property     = get_property;
	object_class->finalize         = finalize;
	parent_class->compare_property = compare_property;
	parent_class->verify           = verify;

	/* Properties */
	/**
	 * NMSettingTeam:config:
	 *
	 * The JSON configuration for the team network interface.  The property
	 * should contain raw JSON configuration data suitable for teamd, because
	 * the value is passed directly to teamd. If not specified, the default
	 * configuration is used.  See man teamd.conf for the format details.
	 **/
	/* ---ifcfg-rh---
	 * property: config
	 * variable: TEAM_CONFIG
	 * description: Team configuration in JSON. See man teamd.conf for details.
	 * ---end---
	 */
	g_object_class_install_property
		(object_class, PROP_CONFIG,
		 g_param_spec_string (NM_SETTING_TEAM_CONFIG, "", "",
		                      NULL,
		                      G_PARAM_READWRITE |
		                      NM_SETTING_PARAM_INFERRABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:notifypeers-count:
	 *
	 * Corresponds to the teamd notify_peers.count.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_NOTIFYPEERS_COUNT,
		 g_param_spec_int (NM_SETTING_TEAM_NOTIFYPEERS_COUNT, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:notifypeers-interval:
	 *
	 * Corresponds to the teamd notify_peers.interval.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_NOTIFYPEERS_INTERVAL,
		 g_param_spec_int (NM_SETTING_TEAM_NOTIFYPEERS_INTERVAL, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:mcastrejoin-count:
	 *
	 * Corresponds to the teamd mcast_rejoin.count.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_MCASTREJOIN_COUNT,
		 g_param_spec_int (NM_SETTING_TEAM_MCASTREJOIN_COUNT, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:mcastrejoin-interval:
	 *
	 * Corresponds to the teamd mcast_rejoin.interval.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_MCASTREJOIN_INTERVAL,
		 g_param_spec_int (NM_SETTING_TEAM_MCASTREJOIN_INTERVAL, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner:
	 *
	 * Corresponds to the teamd runner.name.
	 * Permitted values are: "roundrobin", "broadcast", "activebackup",
	 * "loadbalance", "lacp".
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER,
		 g_param_spec_string (NM_SETTING_TEAM_RUNNER, "", "",
		                      "roundrobin",
		                      G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-hwpolicy:
	 *
	 * Corresponds to the teamd runner.hwaddr_policy.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_HWPOLICY,
		 g_param_spec_string (NM_SETTING_TEAM_RUNNER_HWPOLICY, "", "",
		                      NULL,
		                      G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-txhash:
	 *
	 * Corresponds to the teamd runner.tx_hash.
	 *
	 * Since: 1.10
	 *
	 * Type: GHashTable(utf8,utf8)
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_TXHASH,
		 g_param_spec_boxed (NM_SETTING_TEAM_RUNNER_TXHASH, "", "",
		                     G_TYPE_HASH_TABLE,
	                             G_PARAM_READWRITE |
		                     NM_SETTING_PARAM_INFERRABLE |
	                             G_PARAM_STATIC_STRINGS));
	_nm_setting_class_transform_property (parent_class, NM_SETTING_TEAM_RUNNER_TXHASH,
	                                      G_VARIANT_TYPE ("a{ss}"),
	                                      _nm_utils_strdict_to_dbus,
	                                      _nm_utils_strdict_from_dbus);

	/**
	 * NMSettingTeam:runner-txbalancer:
	 *
	 * Corresponds to the teamd runner.tx_balancer.name.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_TXBALANCER,
		 g_param_spec_string (NM_SETTING_TEAM_RUNNER_TXBALANCER, "", "",
		                      NULL,
		                      G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-txbalancer-interval:
	 *
	 * Corresponds to the teamd runner.tx_balancer.interval.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_TXBALANCER_INTERVAL,
		 g_param_spec_int (NM_SETTING_TEAM_RUNNER_TXBALANCER_INTERVAL, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-active:
	 *
	 * Corresponds to the teamd runner.active.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_ACTIVE,
		 g_param_spec_boolean (NM_SETTING_TEAM_RUNNER_ACTIVE, "", "",
		                       FALSE,
		                       G_PARAM_READWRITE |
		                       G_PARAM_CONSTRUCT |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-fastrate:
	 *
	 * Corresponds to the teamd runner.fast_rate.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_FASTRATE,
		 g_param_spec_boolean (NM_SETTING_TEAM_RUNNER_FASTRATE, "", "",
		                       FALSE,
		                       G_PARAM_READWRITE |
		                       G_PARAM_CONSTRUCT |
		                       G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-sysprio:
	 *
	 * Corresponds to the teamd runner.sys_prio.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_SYSPRIO,
		 g_param_spec_int (NM_SETTING_TEAM_RUNNER_SYSPRIO, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-minports:
	 *
	 * Corresponds to the teamd runner.min_ports.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_MINPORTS,
		 g_param_spec_int (NM_SETTING_TEAM_RUNNER_MINPORTS, "", "",
		                   G_MININT32, G_MAXINT32, 0,
		                   G_PARAM_READWRITE |
		                   G_PARAM_CONSTRUCT |
		                   G_PARAM_STATIC_STRINGS));

	/**
	 * NMSettingTeam:runner-aggselectpolicy:
	 *
	 * Corresponds to the teamd runner.agg_select_policy.
	 *
	 * Since: 1.10
	 **/
	g_object_class_install_property
		(object_class, PROP_RUNNER_AGGSELECTPOLICY,
		 g_param_spec_string (NM_SETTING_TEAM_RUNNER_AGGSELECTPOLICY, "", "",
		                      NULL,
		                      G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT |
		                      G_PARAM_STATIC_STRINGS));

	/* ---dbus---
	 * property: interface-name
	 * format: string
	 * description: Deprecated in favor of connection.interface-name, but can
	 *   be used for backward-compatibility with older daemons, to set the
	 *   team's interface name.
	 * ---end---
	 */
	_nm_setting_class_add_dbus_only_property (parent_class, "interface-name",
	                                          G_VARIANT_TYPE_STRING,
	                                          _nm_setting_get_deprecated_virtual_interface_name,
	                                          NULL);
}
