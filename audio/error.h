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

DBusHandlerResult err_invalid_args(DBusConnection *conn,
					DBusMessage *msg, const char *descr);
DBusHandlerResult err_already_connected(DBusConnection *conn, DBusMessage *msg);
DBusHandlerResult err_not_connected(DBusConnection *conn, DBusMessage * msg);
DBusHandlerResult err_not_supported(DBusConnection *conn, DBusMessage *msg);
DBusHandlerResult err_connect_failed(DBusConnection *conn,
					DBusMessage *msg, const char *err);
DBusHandlerResult err_does_not_exist(DBusConnection *conn, DBusMessage *msg);
DBusHandlerResult err_not_available(DBusConnection *conn, DBusMessage *msg);
DBusHandlerResult err_failed(DBusConnection *conn,
					DBusMessage *msg, const char *dsc);
