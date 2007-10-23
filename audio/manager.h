/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2007  Nokia Corporation
 *  Copyright (C) 2004-2007  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define MAX_PATH_LENGTH 64 /* D-Bus path */
#define AUDIO_MANAGER_PATH "/org/bluez/audio"
#define AUDIO_MANAGER_INTERFACE "org.bluez.audio.Manager"

struct enabled_interfaces {
	gboolean headset;
	gboolean gateway;
	gboolean sink;
	gboolean source;
	gboolean control;
	gboolean target;
};

typedef void (*create_dev_cb_t) (struct device *dev, void *user_data);

int audio_init(DBusConnection *conn, struct enabled_interfaces *enabled,
		gboolean no_hfp, gboolean sco_hci, int source_count);

void audio_exit(void);

uint32_t add_service_record(DBusConnection *conn, sdp_buf_t *buf);
int remove_service_record(DBusConnection *conn, uint32_t rec_id);

struct device *manager_find_device(bdaddr_t *bda, const char *interface,
					gboolean connected);

struct device *manager_device_connected(bdaddr_t *bda, const char *uuid);

gboolean manager_create_device(bdaddr_t *bda, create_dev_cb_t cb,
				void *user_data);

gboolean manager_authorize(bdaddr_t *dba, const char *uuid,
				DBusPendingCallNotifyFunction cb,
				void *user_data,
				DBusPendingCall **pending);
void manager_cancel_authorize(bdaddr_t *dba, const char *uuid,
				DBusPendingCall *pending);
