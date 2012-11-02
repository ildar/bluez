/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011-2012  Intel Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "bt.h"
#include "control.h"
#include "packet.h"

static unsigned long filter_mask = 0;

void packet_set_filter(unsigned long filter)
{
	filter_mask = filter;
}

static void print_channel_header(struct timeval *tv, uint16_t index,
							uint16_t channel)
{
	if (filter_mask & PACKET_FILTER_SHOW_INDEX) {
		switch (channel) {
		case HCI_CHANNEL_CONTROL:
			printf("{hci%d} ", index);
			break;
		case HCI_CHANNEL_MONITOR:
			printf("[hci%d] ", index);
			break;
		}
	}

	if (tv) {
		time_t t = tv->tv_sec;
		struct tm tm;

		localtime_r(&t, &tm);

		if (filter_mask & PACKET_FILTER_SHOW_DATE)
			printf("%04d-%02d-%02d ", tm.tm_year + 1900,
						tm.tm_mon + 1, tm.tm_mday);

		if (filter_mask & PACKET_FILTER_SHOW_TIME)
			printf("%02d:%02d:%02d.%06lu ", tm.tm_hour,
					tm.tm_min, tm.tm_sec, tv->tv_usec);
	}
}

static void print_header(struct timeval *tv, uint16_t index)
{
	print_channel_header(tv, index, HCI_CHANNEL_MONITOR);
}

#define print_field(fmt, args...) printf("%-12c" fmt "\n", ' ', ## args)

static const struct {
	uint8_t error;
	const char *str;
} error2str_table[] = {
	{ 0x00, "Success"						},
	{ 0x01, "Unknown HCI Command"					},
	{ 0x02, "Unknown Connection Identifier"				},
	{ 0x03, "Hardware Failure"					},
	{ 0x04, "Page Timeout"						},
	{ 0x05, "Authentication Failure"				},
	{ 0x06, "PIN or Key Missing"					},
	{ 0x07, "Memory Capacity Exceeded"				},
	{ 0x08, "Connection Timeout"					},
	{ 0x09, "Connection Limit Exceeded"				},
	{ 0x0a, "Synchronous Connection Limit to a Device Exceeded"	},
	{ 0x0b, "ACL Connection Already Exists"				},
	{ 0x0c, "Command Disallowed"					},
	{ 0x0d, "Connection Rejected due to Limited Resources"		},
	{ 0x0e, "Connection Rejected due to Security Reasons"		},
	{ 0x0f, "Connection Rejected due to Unacceptable BD_ADDR"	},
	{ 0x10, "Connection Accept Timeout Exceeded"			},
	{ 0x11, "Unsupported Feature or Parameter Value"		},
	{ 0x12, "Invalid HCI Command Parameters"			},
	{ 0x13, "Remote User Terminated Connection"			},
	{ 0x14, "Remote Device Terminated due to Low Resources"		},
	{ 0x15, "Remote Device Terminated due to Power Off"		},
	{ 0x16, "Connection Terminated By Local Host"			},
	{ 0x17, "Repeated Attempts"					},
	{ 0x18, "Pairing Not Allowed"					},
	{ 0x19, "Unknown LMP PDU"					},
	{ 0x1a, "Unsupported Remote Feature / Unsupported LMP Feature"	},
	{ 0x1b, "SCO Offset Rejected"					},
	{ 0x1c, "SCO Interval Rejected"					},
	{ 0x1d, "SCO Air Mode Rejected"					},
	{ 0x1e, "Invalid LMP Parameters"				},
	{ 0x1f, "Unspecified Error"					},
	{ 0x20, "Unsupported LMP Parameter Value"			},
	{ 0x21, "Role Change Not Allowed"				},
	{ 0x22, "LMP Response Timeout / LL Response Timeout"		},
	{ 0x23, "LMP Error Transaction Collision"			},
	{ 0x24, "LMP PDU Not Allowed"					},
	{ 0x25, "Encryption Mode Not Acceptable"			},
	{ 0x26, "Link Key cannot be Changed"				},
	{ 0x27, "Requested QoS Not Supported"				},
	{ 0x28, "Instant Passed"					},
	{ 0x29, "Pairing With Unit Key Not Supported"			},
	{ 0x2a, "Different Transaction Collision"			},
	{ 0x2b, "Reserved"						},
	{ 0x2c, "QoS Unacceptable Parameter"				},
	{ 0x2d, "QoS Rejected"						},
	{ 0x2e, "Channel Classification Not Supported"			},
	{ 0x2f, "Insufficient Security"					},
	{ 0x30, "Parameter Out Of Manadatory Range"			},
	{ 0x31, "Reserved"						},
	{ 0x32, "Role Switch Pending"					},
	{ 0x33, "Reserved"						},
	{ 0x34, "Reserved Slot Violation"				},
	{ 0x35, "Role Switch Failed"					},
	{ 0x36, "Extended Inquiry Response Too Large"			},
	{ 0x37, "Secure Simple Pairing Not Supported By Host"		},
	{ 0x38, "Host Busy - Pairing"					},
	{ 0x39, "Connection Rejected due to No Suitable Channel Found"	},
	{ 0x3a, "Controller Busy"					},
	{ 0x3b, "Unacceptable Connection Interval"			},
	{ 0x3c, "Directed Advertising Timeout"				},
	{ 0x3d, "Connection Terminated due to MIC Failure"		},
	{ 0x3e, "Connection Failed to be Established"			},
	{ 0x3f, "MAC Connection Failed"					},
	{ }
};

static void print_error(const char *label, uint8_t error)
{
	const char *str = "Unknown";
	int i;

	for (i = 0; error2str_table[i].str; i++) {
		if (error2str_table[i].error == error) {
			str = error2str_table[i].str;
			break;
		}
	}

	print_field("%s: %s (0x%2.2x)", label, str, error);
}

static void print_status(uint8_t status)
{
	print_error("Status", status);
}

static void print_reason(uint8_t reason)
{
	print_error("Reason", reason);
}

static void print_bdaddr(const uint8_t *bdaddr)
{
	print_field("Address: %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
					bdaddr[5], bdaddr[4], bdaddr[3],
					bdaddr[2], bdaddr[1], bdaddr[0]);
}

static void print_handle(uint16_t handle)
{
	print_field("Handle: %d", btohs(handle));
}

static void print_pkt_type(uint16_t pkt_type)
{
	print_field("Packet type: 0x%4.4x", btohs(pkt_type));
}

static void print_iac(const uint8_t *lap)
{
	print_field("Access code: 0x%2.2x%2.2x%2.2x", lap[2], lap[1], lap[0]);
}

static void print_dev_class(const uint8_t *dev_class)
{
	print_field("Class: 0x%2.2x%2.2x%2.2x",
			dev_class[2], dev_class[1], dev_class[0]);
}

static void print_voice_setting(uint16_t setting)
{
	print_field("Setting: 0x%4.4x", btohs(setting));
}

static void print_link_policy(uint16_t link_policy)
{
	print_field("Link policy: 0x%4.4x", btohs(link_policy));
}

static void print_inquiry_mode(uint8_t mode)
{
	const char *str;

	switch (mode) {
	case 0x00:
		str = "Standard Inquiry Result";
		break;
	case 0x01:
		str = "Inquiry Result with RSSI";
		break;
	case 0x02:
		str = "Inquiry Result with RSSI or Extended Inquiry Result";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Mode: %s (0x%2.2x)", str, mode);
}

static void print_simple_pairing_mode(uint8_t mode)
{
	const char *str;

	switch (mode) {
	case 0x00:
		str = "Disabled";
		break;
	case 0x01:
		str = "Enabled";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Mode: %s (0x%2.2x)", str, mode);
}

static void print_pscan_rep_mode(uint8_t pscan_rep_mode)
{
	const char *str;

	switch (pscan_rep_mode) {
	case 0x00:
		str = "R0";
		break;
	case 0x01:
		str = "R1";
		break;
	case 0x02:
		str = "R2";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Page scan repetition mode: %s (0x%2.2x)",
						str, pscan_rep_mode);
}

static void print_pscan_period_mode(uint8_t pscan_period_mode)
{
	const char *str;

	switch (pscan_period_mode) {
	case 0x00:
		str = "P0";
		break;
	case 0x01:
		str = "P1";
		break;
	case 0x02:
		str = "P2";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Page period mode: %s (0x%2.2x)", str, pscan_period_mode);
}

static void print_pscan_mode(uint8_t pscan_mode)
{
	const char *str;

	switch (pscan_mode) {
	case 0x00:
		str = "Mandatory";
		break;
	case 0x01:
		str = "Optional I";
		break;
	case 0x02:
		str = "Optional II";
		break;
	case 0x03:
		str = "Optional III";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Page scan mode: %s (0x%2.2x)", str, pscan_mode);
}

static void print_clock_offset(uint16_t clock_offset)
{
	print_field("Clock offset: 0x%4.4x", btohs(clock_offset));
}

static void print_link_type(uint8_t link_type)
{
	const char *str;

	switch (link_type) {
	case 0x00:
		str = "SCO";
		break;
	case 0x01:
		str = "ACL";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Link type: %s (0x%2.2x)", str, link_type);
}

static void print_encr_mode(uint8_t encr_mode)
{
	const char *str;

	switch (encr_mode) {
	case 0x00:
		str = "Disabled";
		break;
	case 0x01:
		str = "Enabled";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Encryption: %s (0x%2.2x)", str, encr_mode);
}

static void print_key_flag(uint8_t key_flag)
{
	const char *str;

	switch (key_flag) {
	case 0x00:
		str = "Semi-permanent";
		break;
	case 0x01:
		str = "Temporary";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Key flag: %s (0x%2.2x)", str, key_flag);
}

static void print_num_resp(uint8_t num_resp)
{
	print_field("Num responses: %d", num_resp);
}

static void print_timeout(uint16_t timeout)
{
	print_field("Timeout: %.3f msec (0x%4.4x)",
				btohs(timeout) * 0.625, btohs(timeout));
}

static void print_role(uint8_t role)
{
	const char *str;

	switch (role) {
	case 0x00:
		str = "Master";
		break;
	case 0x01:
		str = "Slave";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Role: %s (0x%2.2x)", str, role);
}

static void print_name(const uint8_t *name)
{
	char str[249];

	memcpy(str, name, 248);
	str[248] = '\0';

	print_field("Name: %s", str);
}

static void print_version(const char *label, uint8_t version, uint16_t revision)
{
	print_field("%s: %d - 0x%4.4x", label, version, revision);
}

static void print_hci_version(uint8_t hci_ver, uint16_t hci_rev)
{
	print_version("HCI version", hci_ver, hci_rev);
}

static void print_lmp_version(uint8_t lmp_ver, uint16_t lmp_subver)
{
	print_version("LMP version", lmp_ver, lmp_subver);
}

static void print_manufacturer(uint16_t manufacturer)
{
	print_field("Manufacturer: %d", manufacturer);
}

static void print_commands(const uint8_t *commands)
{
	char str[129];
	int i;

	for (i = 0; i < 64; i++)
		sprintf(str + (i * 2), "%2.2x", commands[i]);

	print_field("Commands: 0x%s", str);
}

static void print_features(const uint8_t *features)
{
	char str[41];
	int i;

	for (i = 0; i < 8; i++)
		sprintf(str + (i * 5), " 0x%2.2x", features[i]);

	print_field("Features:%s", str);
}

static void print_event_mask(const uint8_t *mask)
{
	char str[17];
	int i;

	for (i = 0; i < 8; i++)
		sprintf(str + (i * 2), "%2.2x", mask[i]);

	print_field("Mask: 0x%s", str);
}

static void print_fec(uint8_t fec)
{
	const char *str;

	switch (fec) {
	case 0x00:
		str = "Not required";
		break;
	case 0x01:
		str = "Required";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("FEC: %s (0x%02x)", str, fec);
}

static void print_eir(const uint8_t *eir)
{
	packet_hexdump(eir, 240);
}

void packet_hexdump(const unsigned char *buf, uint16_t len)
{
	static const char hexdigits[] = "0123456789abcdef";
	char str[68];
	uint16_t i;

	if (!len)
		return;

	for (i = 0; i < len; i++) {
		str[((i % 16) * 3) + 0] = hexdigits[buf[i] >> 4];
		str[((i % 16) * 3) + 1] = hexdigits[buf[i] & 0xf];
		str[((i % 16) * 3) + 2] = ' ';
		str[(i % 16) + 49] = isprint(buf[i]) ? buf[i] : '.';

		if ((i + 1) % 16 == 0) {
			str[47] = ' ';
			str[48] = ' ';
			str[65] = '\0';
			printf("%-12c%s\n", ' ', str);
			str[0] = ' ';
		}
	}

	if (i % 16 > 0) {
		uint16_t j;
		for (j = (i % 16); j < 16; j++) {
			str[(j * 3) + 0] = ' ';
			str[(j * 3) + 1] = ' ';
			str[(j * 3) + 2] = ' ';
			str[j + 49] = ' ';
		}
		str[47] = ' ';
		str[48] = ' ';
		str[65] = '\0';
		printf("%-12c%s\n", ' ', str);
	}
}

void packet_control(struct timeval *tv, uint16_t index, uint16_t opcode,
					const void *data, uint16_t size)
{
	print_channel_header(tv, index, HCI_CHANNEL_CONTROL);

	control_message(opcode, data, size);
}

#define MONITOR_NEW_INDEX	0
#define MONITOR_DEL_INDEX	1
#define MONITOR_COMMAND_PKT	2
#define MONITOR_EVENT_PKT	3
#define MONITOR_ACL_TX_PKT	4
#define MONITOR_ACL_RX_PKT	5
#define MONITOR_SCO_TX_PKT	6
#define MONITOR_SCO_RX_PKT	7

struct monitor_new_index {
	uint8_t  type;
	uint8_t  bus;
	bdaddr_t bdaddr;
	char     name[8];
} __attribute__((packed));

#define MONITOR_NEW_INDEX_SIZE 16

#define MONITOR_DEL_INDEX_SIZE 0

#define MAX_INDEX 16

static struct monitor_new_index index_list[MAX_INDEX];

uint32_t packet_get_flags(uint16_t opcode)
{
	switch (opcode) {
	case MONITOR_NEW_INDEX:
	case MONITOR_DEL_INDEX:
		break;
	case MONITOR_COMMAND_PKT:
		return 0x02;
	case MONITOR_EVENT_PKT:
		return 0x03;
	case MONITOR_ACL_TX_PKT:
		return 0x00;
	case MONITOR_ACL_RX_PKT:
		return 0x01;
	case MONITOR_SCO_TX_PKT:
	case MONITOR_SCO_RX_PKT:
		break;
	}

	return 0xff;
}

uint16_t packet_get_opcode(uint32_t flags)
{
	if (flags & 0x02) {
		if (flags & 0x01)
			return MONITOR_EVENT_PKT;
		else
			return MONITOR_COMMAND_PKT;
	} else {
		if (flags & 0x01)
			return MONITOR_ACL_RX_PKT;
		else
			return MONITOR_ACL_TX_PKT;
	}

	return 0xff;
}

void packet_monitor(struct timeval *tv, uint16_t index, uint16_t opcode,
					const void *data, uint16_t size)
{
	const struct monitor_new_index *ni;
	char str[18];

	switch (opcode) {
	case MONITOR_NEW_INDEX:
		ni = data;

		if (index < MAX_INDEX)
			memcpy(&index_list[index], ni, MONITOR_NEW_INDEX_SIZE);

		ba2str(&ni->bdaddr, str);
		packet_new_index(tv, index, str, ni->type, ni->bus, ni->name);
		break;
	case MONITOR_DEL_INDEX:
		if (index < MAX_INDEX)
			ba2str(&index_list[index].bdaddr, str);
		else
			ba2str(BDADDR_ANY, str);

		packet_del_index(tv, index, str);
		break;
	case MONITOR_COMMAND_PKT:
		packet_hci_command(tv, index, data, size);
		break;
	case MONITOR_EVENT_PKT:
		packet_hci_event(tv, index, data, size);
		break;
	case MONITOR_ACL_TX_PKT:
		packet_hci_acldata(tv, index, false, data, size);
		break;
	case MONITOR_ACL_RX_PKT:
		packet_hci_acldata(tv, index, true, data, size);
		break;
	case MONITOR_SCO_TX_PKT:
		packet_hci_scodata(tv, index, false, data, size);
		break;
	case MONITOR_SCO_RX_PKT:
		packet_hci_scodata(tv, index, true, data, size);
		break;
	default:
		print_header(tv, index);
		printf("* Unknown packet (code %d len %d)\n", opcode, size);
		packet_hexdump(data, size);
		break;
	}
}

static void null_cmd(const void *data, uint8_t size)
{
}

static void status_rsp(const void *data, uint8_t size)
{
	uint8_t status = *((const uint8_t *) data);

	print_status(status);
}

static void status_bdaddr_rsp(const void *data, uint8_t size)
{
	uint8_t status = *((const uint8_t *) data);

	print_status(status);
	print_bdaddr(data + 1);
}

static void inquiry_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_inquiry *cmd = data;

	print_iac(cmd->lap);
	print_field("Length: %.2fs (0x%2.2x)",
				cmd->length * 1.28, cmd->length);
	print_num_resp(cmd->num_resp);
}

static void periodic_inquiry_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_periodic_inquiry *cmd = data;

	print_field("Max period: %.2fs (0x%2.2x)",
				cmd->max_period * 1.28, cmd->max_period);
	print_field("Min period: %.2fs (0x%2.2x)",
				cmd->min_period * 1.28, cmd->min_period);
	print_iac(cmd->lap);
	print_field("Length: %.2fs (0x%2.2x)",
				cmd->length * 1.28, cmd->length);
	print_num_resp(cmd->num_resp);
}

static void create_conn_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_create_conn *cmd = data;
	const char *str;

	print_bdaddr(cmd->bdaddr);
	print_pkt_type(cmd->pkt_type);
	print_pscan_rep_mode(cmd->pscan_rep_mode);
	print_pscan_mode(cmd->pscan_mode);
	print_clock_offset(cmd->clock_offset);

	switch (cmd->role_switch) {
	case 0x00:
		str = "Stay master";
		break;
	case 0x01:
		str = "Allow slave";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Role switch: %s (0x%2.2x)", str, cmd->role_switch);
}

static void disconnect_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_disconnect *cmd = data;

	print_handle(cmd->handle);
	print_reason(cmd->reason);
}

static void add_sco_conn_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_add_sco_conn *cmd = data;

	print_handle(cmd->handle);
	print_pkt_type(cmd->pkt_type);
}

static void create_conn_cancel_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_create_conn_cancel *cmd = data;

	print_bdaddr(cmd->bdaddr);
}

static void accept_conn_request_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_accept_conn_request *cmd = data;

	print_bdaddr(cmd->bdaddr);
	print_role(cmd->role);
}

static void reject_conn_request_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_reject_conn_request *cmd = data;

	print_bdaddr(cmd->bdaddr);
	print_reason(cmd->reason);
}

static void remote_name_request_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_remote_name_request *cmd = data;

	print_bdaddr(cmd->bdaddr);
	print_pscan_rep_mode(cmd->pscan_rep_mode);
	print_pscan_mode(cmd->pscan_mode);
	print_clock_offset(cmd->clock_offset);
}

static void remote_name_request_cancel_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_remote_name_request_cancel *cmd = data;

	print_bdaddr(cmd->bdaddr);
}

static void read_remote_features_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_read_remote_features *cmd = data;

	print_handle(cmd->handle);
}

static void read_remote_ext_features_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_read_remote_ext_features *cmd = data;

	print_handle(cmd->handle);
	print_field("Page: %d", cmd->page);
}

static void read_remote_version_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_read_remote_version *cmd = data;

	print_handle(cmd->handle);
}

static void read_default_link_policy_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_default_link_policy *rsp = data;

	print_status(rsp->status);
	print_link_policy(rsp->policy);
}

static void write_default_link_policy_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_default_link_policy *cmd = data;

	print_link_policy(cmd->policy);
}

static void set_event_mask_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_set_event_mask *cmd = data;

	print_event_mask(cmd->mask);
}

static void set_event_filter_cmd(const void *data, uint8_t size)
{
	uint8_t type = *((const uint8_t *) data);

	print_field("Type: 0x%2.2x", type);

	packet_hexdump(data + 1, size - 1);
}

static void delete_stored_link_key_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_delete_stored_link_key *cmd = data;

	print_bdaddr(cmd->bdaddr);
	print_field("Delete all: 0x%2.2x", cmd->delete_all);
}

static void delete_stored_link_key_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_delete_stored_link_key *rsp = data;

	print_status(rsp->status);
	print_field("Num keys: %d", btohs(rsp->num_keys));
}

static void write_local_name_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_local_name *cmd = data;

	print_name(cmd->name);
}

static void read_local_name_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_local_name *rsp = data;

	print_status(rsp->status);
	print_name(rsp->name);
}

static void read_conn_accept_timeout_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_conn_accept_timeout *rsp = data;

	print_status(rsp->status);
	print_timeout(rsp->timeout);
}

static void write_conn_accept_timeout_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_conn_accept_timeout *cmd = data;

	print_timeout(cmd->timeout);
}

static void read_class_of_dev_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_class_of_dev *rsp = data;

	print_status(rsp->status);
	print_dev_class(rsp->dev_class);
}

static void write_class_of_dev_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_class_of_dev *cmd = data;

	print_dev_class(cmd->dev_class);
}

static void read_voice_setting_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_voice_setting *rsp = data;

	print_status(rsp->status);
	print_voice_setting(rsp->setting);
}

static void write_voice_setting_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_voice_setting *cmd = data;

	print_voice_setting(cmd->setting);
}

static void read_inquiry_mode_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_inquiry_mode *rsp = data;

	print_status(rsp->status);
	print_inquiry_mode(rsp->mode);
}

static void write_inquiry_mode_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_inquiry_mode *cmd = data;

	print_inquiry_mode(cmd->mode);
}

static void read_ext_inquiry_response_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_ext_inquiry_response *rsp = data;

	print_status(rsp->status);
	print_fec(rsp->fec);
	print_eir(rsp->data);
}

static void write_ext_inquiry_response_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_ext_inquiry_response *cmd = data;

	print_fec(cmd->fec);
	print_eir(cmd->data);
}

static void read_simple_pairing_mode_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_simple_pairing_mode *rsp = data;

	print_status(rsp->status);
	print_simple_pairing_mode(rsp->mode);
}

static void write_simple_pairing_mode_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_simple_pairing_mode *cmd = data;

	print_simple_pairing_mode(cmd->mode);
}

static void read_inquiry_resp_tx_power_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_inquiry_resp_tx_power *rsp = data;

	print_status(rsp->status);
	print_field("TX power: %d dBm", rsp->level);
}

static void read_le_host_supported_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_le_host_supported *rsp = data;

	print_status(rsp->status);
	print_field("Supported: 0x%2.2x", rsp->supported);
	print_field("Simultaneous: 0x%2.2x", rsp->simultaneous);
}

static void write_le_host_supported_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_write_le_host_supported *cmd = data;

	print_field("Supported: 0x%2.2x", cmd->supported);
	print_field("Simultaneous: 0x%2.2x", cmd->simultaneous);
}

static void read_local_version_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_local_version *rsp = data;

	print_status(rsp->status);
	print_hci_version(rsp->hci_ver, rsp->hci_rev);
	print_lmp_version(rsp->lmp_ver, rsp->lmp_subver);
	print_manufacturer(rsp->manufacturer);
}

static void read_local_commands_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_local_commands *rsp = data;

	print_status(rsp->status);
	print_commands(rsp->commands);
}

static void read_local_features_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_local_features *rsp = data;

	print_status(rsp->status);
	print_features(rsp->features);
}

static void read_local_ext_features_cmd(const void *data, uint8_t size)
{
	const struct bt_hci_cmd_read_local_ext_features *cmd = data;

	print_field("Page: %d", cmd->page);
}

static void read_local_ext_features_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_local_ext_features *rsp = data;

	print_status(rsp->status);
	print_field("Page: %d/%d", rsp->page, rsp->max_page);
	print_features(rsp->features);
}

static void read_buffer_size_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_buffer_size *rsp = data;

	print_status(rsp->status);
	print_field("ACL MTU: %-4d ACL max packet: %d",
				btohs(rsp->acl_mtu), btohs(rsp->acl_max_pkt));
	print_field("SCO MTU: %-4d SCO max packet: %d",
				rsp->sco_mtu, btohs(rsp->sco_max_pkt));
}

static void read_country_code_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_country_code *rsp = data;
	const char *str;

	print_status(rsp->status);

	switch (rsp->code) {
	case 0x00:
		str = "North America, Europe*, Japan";
		break;
	case 0x01:
		str = "France";
		break;
	default:
		str = "Reserved";
		break;
	}

	print_field("Country code: %s (0x%2.2x)", str, rsp->code);
}

static void read_bd_addr_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_bd_addr *rsp = data;

	print_status(rsp->status);
	print_bdaddr(rsp->bdaddr);
}

static void read_data_block_size_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_read_data_block_size *rsp = data;

	print_status(rsp->status);
	print_field("Max ACL length: %d", btohs(rsp->max_acl_len));
	print_field("Block length: %d", btohs(rsp->block_len));
	print_field("Num blocks: %d", btohs(rsp->num_blocks));
}

static void le_read_buffer_size_rsp(const void *data, uint8_t size)
{
	const struct bt_hci_rsp_le_read_buffer_size *rsp = data;

	print_status(rsp->status);
	print_field("Data packet length: %d", btohs(rsp->le_mtu));
	print_field("Num data packets: %d", rsp->le_max_pkt);
}

struct opcode_data {
	uint16_t opcode;
	const char *str;
	void (*cmd_func) (const void *data, uint8_t size);
	uint8_t cmd_size;
	bool cmd_fixed;
	void (*rsp_func) (const void *data, uint8_t size);
	uint8_t rsp_size;
	bool rsp_fixed;
};

static const struct opcode_data opcode_table[] = {
	{ 0x0000, "NOP" },
	/* OGF 1 - Link Control */
	{ 0x0401, "Inquiry",
				inquiry_cmd, 5, true },
	{ 0x0402, "Inquiry Cancel",
				null_cmd, 0, true,
				status_rsp, 1, true },
	{ 0x0403, "Periodic Inquiry Mode",
				periodic_inquiry_cmd, 9, true,
				status_rsp, 1, true },
	{ 0x0404, "Exit Periodic Inquiry Mode",
				null_cmd, 0, true,
				status_rsp, 1, true },
	{ 0x0405, "Create Connection",
				create_conn_cmd, 13, true },
	{ 0x0406, "Disconnect",
				disconnect_cmd, 3, true },
	{ 0x0407, "Add SCO Connection",
				add_sco_conn_cmd, 4, true },
	{ 0x0408, "Create Connection Cancel",
				create_conn_cancel_cmd, 6, true,
				status_bdaddr_rsp, 7, true },
	{ 0x0409, "Accept Connection Request",
				accept_conn_request_cmd, 7, true },
	{ 0x040a, "Reject Connection Request",
				reject_conn_request_cmd, 7, true },
	{ 0x040b, "Link Key Request Reply"		},
	{ 0x040c, "Link Key Request Negative Reply"	},
	{ 0x040d, "PIN Code Request Reply"		},
	{ 0x040e, "PIN Code Request Negative Reply"	},
	{ 0x040f, "Change Connection Packet Type"	},
	/* reserved command */
	{ 0x0411, "Authentication Requested"		},
	/* reserved command */
	{ 0x0413, "Set Connection Encryption"		},
	/* reserved command */
	{ 0x0415, "Change Connection Link Key"		},
	/* reserved command */
	{ 0x0417, "Master Link Key"			},
	/* reserved command */
	{ 0x0419, "Remote Name Request",
				remote_name_request_cmd, 10, true },
	{ 0x041a, "Remote Name Request Cancel",
				remote_name_request_cancel_cmd, 6, true,
				status_bdaddr_rsp, 7, true },
	{ 0x041b, "Read Remote Supported Features",
				read_remote_features_cmd, 2, true },
	{ 0x041c, "Read Remote Extended Features",
				read_remote_ext_features_cmd, 3, true },
	{ 0x041d, "Read Remote Version Information",
				read_remote_version_cmd, 2, true },
	/* reserved command */
	{ 0x041f, "Read Clock Offset"			},
	{ 0x0420, "Read LMP Handle"			},
	/* reserved commands */
	{ 0x0428, "Setup Synchronous Connection"	},
	{ 0x0429, "Accept Synchronous Connection"	},
	{ 0x042a, "Reject Synchronous Connection"	},
	{ 0x042b, "IO Capability Request Reply"		},
	{ 0x042c, "User Confirmation Request Reply"	},
	{ 0x042d, "User Confirmation Request Neg Reply"	},
	{ 0x042e, "User Passkey Request Reply"		},
	{ 0x042f, "User Passkey Request Negative Reply"	},
	{ 0x0430, "Remote OOB Data Request Reply"	},
	/* reserved commands */
	{ 0x0433, "Remote OOB Data Request Neg Reply"	},
	{ 0x0434, "IO Capability Request Negative Reply"},
	{ 0x0435, "Create Physical Link"		},
	{ 0x0436, "Accept Physical Link"		},
	{ 0x0437, "Disconnect Physical Link"		},
	{ 0x0438, "Create Logical Link"			},
	{ 0x0439, "Accept Logical Link"			},
	{ 0x043a, "Disconnect Logical Link"		},
	{ 0x043b, "Logical Link Cancel"			},
	{ 0x043c, "Flow Specifcation Modify"		},

	/* OGF 2 - Link Policy */
	{ 0x0801, "Holde Mode"				},
	/* reserved command */
	{ 0x0803, "Sniff Mode"				},
	{ 0x0804, "Exit Sniff Mode"			},
	{ 0x0805, "Park State"				},
	{ 0x0806, "Exit Park State"			},
	{ 0x0807, "QoS Setup"				},
	/* reserved command */
	{ 0x0809, "Role Discovery"			},
	/* reserved command */
	{ 0x080b, "Switch Role"				},
	{ 0x080c, "Read Link Policy Settings"		},
	{ 0x080d, "Write Link Policy Settings"		},
	{ 0x080e, "Read Default Link Policy Settings",
				null_cmd, 0, true,
				read_default_link_policy_rsp, 3, true },
	{ 0x080f, "Write Default Link Policy Settings",
				write_default_link_policy_cmd, 2, true,
				status_rsp, 1, true },
	{ 0x0810, "Flow Specification"			},
	{ 0x0811, "Sniff Subrating"			},

	/* OGF 3 - Host Control */
	{ 0x0c01, "Set Event Mask",
				set_event_mask_cmd, 8, true,
				status_rsp, 1, true },
	/* reserved command */
	{ 0x0c03, "Reset",
				null_cmd, 0, true,
				status_rsp, 1, true },
	/* reserved command */
	{ 0x0c05, "Set Event Filter",
				set_event_filter_cmd, 1, false,
				status_rsp, 1, true },
	/* reserved commands */
	{ 0x0c08, "Flush"				},
	{ 0x0c09, "Read PIN Type"			},
	{ 0x0c0a, "Write PIN Type"			},
	{ 0x0c0b, "Create New Unit Key"			},
	/* reserved command */
	{ 0x0c0d, "Read Stored Link Key"		},
	/* reserved commands */
	{ 0x0c11, "Write Stored Link Key"		},
	{ 0x0c12, "Delete Stored Link Key",
				delete_stored_link_key_cmd, 7, true,
				delete_stored_link_key_rsp, 3, true },
	{ 0x0c13, "Write Local Name",
				write_local_name_cmd, 248, true,
				status_rsp, 1, true },
	{ 0x0c14, "Read Local Name",
				null_cmd, 0, true,
				read_local_name_rsp, 249, true },
	{ 0x0c15, "Read Connection Accept Timeout",
				null_cmd, 0, true,
				read_conn_accept_timeout_rsp, 3, true },
	{ 0x0c16, "Write Connection Accept Timeout",
				write_conn_accept_timeout_cmd, 2, true,
				status_rsp, 1, true },
	{ 0x0c17, "Read Page Timeout"			},
	{ 0x0c18, "Write Page Timeout"			},
	{ 0x0c19, "Read Scan Enable"			},
	{ 0x0c1a, "Write Scan Enable"			},
	{ 0x0c1b, "Read Page Scan Activity"		},
	{ 0x0c1c, "Write Page Scan Activity"		},
	{ 0x0c1d, "Read Inquiry Scan Activity"		},
	{ 0x0c1e, "Write Inquiry Scan Activity"		},
	{ 0x0c1f, "Read Authentication Enable"		},
	{ 0x0c20, "Write Authentication Enable"		},
	{ 0x0c21, "Read Encryption Mode"		},
	{ 0x0c22, "Write Encryption Mode"		},
	{ 0x0c23, "Read Class of Device",
				null_cmd, 0, true,
				read_class_of_dev_rsp, 4, true },
	{ 0x0c24, "Write Class of Device",
				write_class_of_dev_cmd, 3, true,
				status_rsp, 1, true },
	{ 0x0c25, "Read Voice Setting",
				null_cmd, 0, true,
				read_voice_setting_rsp, 3, true },
	{ 0x0c26, "Write Voice Setting",
				write_voice_setting_cmd, 2, true,
				status_rsp, 1, true },
	{ 0x0c27, "Read Automatic Flush Timeout"	},
	{ 0x0c28, "Write Automatic Flush Timeout"	},
	{ 0x0c29, "Read Num Broadcast Retransmissions"	},
	{ 0x0c2a, "Write Num Broadcast Retransmissions"	},
	{ 0x0c2b, "Read Hold Mode Activity"		},
	{ 0x0c2c, "Write Hold Mode Activity"		},
	{ 0x0c2d, "Read Transmit Power Level"		},
	{ 0x0c2e, "Read Sync Flow Control Enable"	},
	{ 0x0c2f, "Write Sync Flow Control Enable"	},
	/* reserved command */
	{ 0x0c31, "Set Host Controller To Host Flow"	},
	/* reserved command */
	{ 0x0c33, "Host Buffer Size"			},
	/* reserved command */
	{ 0x0c35, "Host Number of Completed Packets"	},
	{ 0x0c36, "Read Link Supervision Timeout"	},
	{ 0x0c37, "Write Link Supervision Timeout"	},
	{ 0x0c38, "Read Number of Supported IAC"	},
	{ 0x0c39, "Read Current IAC LAP"		},
	{ 0x0c3a, "Write Current IAC LAP"		},
	{ 0x0c3b, "Read Page Scan Period Mode"		},
	{ 0x0c3c, "Write Page Scan Period Mode"		},
	{ 0x0c3d, "Read Page Scan Mode"			},
	{ 0x0c3e, "Write Page Scan Mode"		},
	{ 0x0c3f, "Set AFH Host Channel Classification"	},
	/* reserved commands */
	{ 0x0c42, "Read Inquiry Scan Type"		},
	{ 0x0c43, "Write Inquiry Scan Type"		},
	{ 0x0c44, "Read Inquiry Mode",
				null_cmd, 0, true,
				read_inquiry_mode_rsp, 2, true },
	{ 0x0c45, "Write Inquiry Mode",
				write_inquiry_mode_cmd, 1, true,
				status_rsp, 1, true },
	{ 0x0c46, "Read Page Scan Type"			},
	{ 0x0c47, "Write Page Scan Type"		},
	{ 0x0c48, "Read AFH Channel Assessment Mode"	},
	{ 0x0c49, "Write AFH Channel Assessment Mode"	},
	/* reserved commands */
	{ 0x0c51, "Read Extended Inquiry Response",
				null_cmd, 0, true,
				read_ext_inquiry_response_rsp, 242, true },
	{ 0x0c52, "Write Extended Inquiry Response",
				write_ext_inquiry_response_cmd, 241, true,
				status_rsp, 1, true },
	{ 0x0c53, "Refresh Encryption Key"		},
	/* reserved command */
	{ 0x0c55, "Read Simple Pairing Mode",
				null_cmd, 0, true,
				read_simple_pairing_mode_rsp, 2, true },
	{ 0x0c56, "Write Simple Pairing Mode",
				write_simple_pairing_mode_cmd, 1, true,
				status_rsp, 1, true },
	{ 0x0c57, "Read Local OOB Data"			},
	{ 0x0c58, "Read Inquiry Response TX Power Level",
				null_cmd, 0, true,
				read_inquiry_resp_tx_power_rsp, 2, true },
	{ 0x0c59, "Write Inquiry Transmit Power Level"	},
	{ 0x0c5a, "Read Default Erroneous Reporting"	},
	{ 0x0c5b, "Write Default Erroneous Reporting"	},
	/* reserved commands */
	{ 0x0c5f, "Enhanced Flush"			},
	/* reserved command */
	{ 0x0c61, "Read Logical Link Accept Timeout"	},
	{ 0x0c62, "Write Logical Link Accept Timeout"	},
	{ 0x0c63, "Set Event Mask Page 2"		},
	{ 0x0c64, "Read Location Data"			},
	{ 0x0c65, "Write Location Data"			},
	{ 0x0c66, "Read Flow Control Mode"		},
	{ 0x0c67, "Write Flow Control Mode"		},
	{ 0x0c68, "Read Enhanced Transmit Power Level"	},
	{ 0x0c69, "Read Best Effort Flush Timeout"	},
	{ 0x0c6a, "Write Best Effort Flush Timeout"	},
	{ 0x0c6b, "Short Range Mode"			},
	{ 0x0c6c, "Read LE Host Supported",
				null_cmd, 0, true,
				read_le_host_supported_rsp, 3, true },
	{ 0x0c6d, "Write LE Host Supported",
				write_le_host_supported_cmd, 2, true,
				status_rsp, 1, true },

	/* OGF 4 - Information Parameter */
	{ 0x1001, "Read Local Version Information",
				null_cmd, 0, true,
				read_local_version_rsp, 9, true },
	{ 0x1002, "Read Local Supported Commands",
				null_cmd, 0, true,
				read_local_commands_rsp, 65, true },
	{ 0x1003, "Read Local Supported Features",
				null_cmd, 0, true,
				read_local_features_rsp, 9, true },
	{ 0x1004, "Read Local Extended Features",
				read_local_ext_features_cmd, 1, true,
				read_local_ext_features_rsp, 11, true },
	{ 0x1005, "Read Buffer Size",
				null_cmd, 0, true,
				read_buffer_size_rsp, 8, true },
	/* reserved command */
	{ 0x1007, "Read Country Code",
				null_cmd, 0, true,
				read_country_code_rsp, 2, true },
	/* reserved command */
	{ 0x1009, "Read BD ADDR",
				null_cmd, 0, true,
				read_bd_addr_rsp, 7, true },
	{ 0x100a, "Read Data Block Size",
				null_cmd, 0, true,
				read_data_block_size_rsp, 7, true },

	/* OGF 5 - Status Parameter */
	{ 0x1401, "Read Failed Contact Counter"		},
	{ 0x1402, "Reset Failed Contact Counter"	},
	{ 0x1403, "Read Link Quality"			},
	/* reserved command */
	{ 0x1405, "Read RSSI"				},
	{ 0x1406, "Read AFH Channel Map"		},
	{ 0x1407, "Read Clock"				},
	{ 0x1408, "Read Encryption Key Size"		},
	{ 0x1409, "Read Local AMP Info"			},
	{ 0x140a, "Read Local AMP ASSOC"		},
	{ 0x140b, "Write Remote AMP ASSOC"		},

	/* OGF 8 - LE Control */
	{ 0x2001, "LE Set Event Mask"			},
	{ 0x2002, "LE Read Buffer Size",
				null_cmd, 0, true,
				le_read_buffer_size_rsp, 4, true },
	{ 0x2003, "LE Read Local Supported Features"	},
	/* reserved command */
	{ 0x2005, "LE Set Random Address"		},
	{ 0x2006, "LE Set Advertising Parameters"	},
	{ 0x2007, "LE Read Advertising Channel TX Power"},
	{ 0x2008, "LE Set Advertising Data"		},
	{ 0x2009, "LE Set Scan Response Data"		},
	{ 0x200a, "LE Set Advertise Enable"		},
	{ 0x200b, "LE Set Scan Parameters"		},
	{ 0x200c, "LE Set Scan Enable"			},
	{ 0x200d, "LE Create Connection"		},
	{ 0x200e, "LE Create Connection Cancel"		},
	{ 0x200f, "LE Read White List Size"		},
	{ 0x2010, "LE Clear White List"			},
	{ 0x2011, "LE Add Device To White List"		},
	{ 0x2012, "LE Remove Device From White List"	},
	{ 0x2013, "LE Connection Update"		},
	{ 0x2014, "LE Set Host Channel Classification"	},
	{ 0x2015, "LE Read Channel Map"			},
	{ 0x2016, "LE Read Remote Used Features"	},
	{ 0x2017, "LE Encrypt"				},
	{ 0x2018, "LE Rand"				},
	{ 0x2019, "LE Start Encryption"			},
	{ 0x201a, "LE Long Term Key Request Reply"	},
	{ 0x201b, "LE Long Term Key Request Neg Reply"	},
	{ 0x201c, "LE Read Supported States"		},
	{ 0x201d, "LE Receiver Test"			},
	{ 0x201e, "LE Transmitter Test"			},
	{ 0x201f, "LE Test End"				},
	{ }
};

static void status_evt(const void *data, uint8_t size)
{
	uint8_t status = *((uint8_t *) data);

	print_status(status);
}

static void inquiry_result_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_inquiry_result *evt = data;

	print_num_resp(evt->num_resp);
	print_bdaddr(evt->bdaddr);
	print_pscan_rep_mode(evt->pscan_rep_mode);
	print_pscan_period_mode(evt->pscan_period_mode);
	print_pscan_mode(evt->pscan_mode);
	print_dev_class(evt->dev_class);
	print_clock_offset(evt->clock_offset);

	if (size > sizeof(*evt))
		packet_hexdump(data + sizeof(*evt), size - sizeof(*evt));
}

static void conn_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_conn_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_bdaddr(evt->bdaddr);
	print_link_type(evt->link_type);
	print_encr_mode(evt->encr_mode);
}

static void conn_request_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_conn_request *evt = data;

	print_bdaddr(evt->bdaddr);
	print_dev_class(evt->dev_class);
	print_link_type(evt->link_type);
}

static void disconnect_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_disconnect_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_reason(evt->reason);
}

static void auth_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_auth_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
}

static void remote_name_request_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_remote_name_request_complete *evt = data;

	print_status(evt->status);
	print_bdaddr(evt->bdaddr);
	print_name(evt->name);
}

static void encrypt_change_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_encrypt_change *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_encr_mode(evt->encr_mode);
}

static void change_conn_link_key_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_change_conn_link_key_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
}

static void master_link_key_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_master_link_key_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_key_flag(evt->key_flag);
}

static void remote_features_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_remote_features_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_features(evt->features);
}

static void remote_version_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_remote_version_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_lmp_version(evt->lmp_ver, evt->lmp_subver);
	print_manufacturer(evt->manufacturer);
}

static void qos_setup_complete_evt(const void *data, uint8_t size)
{
	uint8_t status = *((uint8_t *) data);

	print_status(status);

	packet_hexdump(data + 1, size - 1);
}

static void cmd_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_cmd_complete *evt = data;
	uint16_t opcode = btohs(evt->opcode);
	uint16_t ogf = cmd_opcode_ogf(opcode);
	uint16_t ocf = cmd_opcode_ocf(opcode);
	const struct opcode_data *opcode_data = NULL;
	int i;

	for (i = 0; opcode_table[i].str; i++) {
		if (opcode_table[i].opcode == opcode) {
			opcode_data = &opcode_table[i];
			break;
		}
	}

	print_field("%s (0x%2.2x|0x%4.4x) ncmd %d",
				opcode_data ? opcode_data->str : "Unknown",
							ogf, ocf, evt->ncmd);

	if (!opcode_data->rsp_func) {
		packet_hexdump(data + 3, size - 3);
		return;
	}

	if (opcode_data->rsp_fixed) {
		if (size - 3 != opcode_data->rsp_size) {
			print_field("invalid packet size");
			packet_hexdump(data + 3, size - 3);
			return;
		}
	} else {
		if (size - 3 < opcode_data->rsp_size) {
			print_field("too short packet");
			packet_hexdump(data + 3, size - 3);
			return;
		}
	}

	opcode_data->rsp_func(data + 3, size - 3);
}

static void cmd_status_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_cmd_status *evt = data;
	uint16_t opcode = btohs(evt->opcode);
	uint16_t ogf = cmd_opcode_ogf(opcode);
	uint16_t ocf = cmd_opcode_ocf(opcode);
	const struct opcode_data *opcode_data = NULL;
	int i;

	for (i = 0; opcode_table[i].str; i++) {
		if (opcode_table[i].opcode == opcode) {
			opcode_data = &opcode_table[i];
			break;
		}
	}

	print_field("%s (0x%2.2x|0x%4.4x) ncmd %d",
				opcode_data ? opcode_data->str : "Unknown",
							ogf, ocf, evt->ncmd);

	print_status(evt->status);
}

static void hardware_error_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_hardware_error *evt = data;

	print_field("Code: 0x%2.2x", evt->code);
}

static void flush_occurred_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_flush_occurred *evt = data;

	print_handle(evt->handle);
}

static void role_change_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_role_change *evt = data;

	print_status(evt->status);
	print_bdaddr(evt->bdaddr);
	print_role(evt->role);
}

static void num_completed_packets_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_num_completed_packets *evt = data;

	print_field("Num handles: %d", evt->num_handles);
	print_handle(evt->handle);
	print_field("Count: %d", btohs(evt->count));

	if (size > sizeof(*evt))
		packet_hexdump(data + sizeof(*evt), size - sizeof(*evt));
}

static void max_slots_change_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_max_slots_change *evt = data;

	print_handle(evt->handle);
	print_field("Max slots: %d", evt->max_slots);
}

static void remote_ext_features_complete_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_remote_ext_features_complete *evt = data;

	print_status(evt->status);
	print_handle(evt->handle);
	print_field("Page: %d/%d", evt->page, evt->max_page);
	print_features(evt->features);
}

static void pscan_rep_mode_change_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_pscan_rep_mode_change *evt = data;

	print_bdaddr(evt->bdaddr);
	print_pscan_rep_mode(evt->pscan_rep_mode);
}

static void remote_host_features_notify_evt(const void *data, uint8_t size)
{
	const struct bt_hci_evt_remote_host_features_notify *evt = data;

	print_bdaddr(evt->bdaddr);
	print_features(evt->features);
}

struct subevent_data {
	uint8_t subevent;
	const char *str;
};

static const struct subevent_data subevent_table[] = {
	{ }
};

static void le_meta_event_evt(const void *data, uint8_t size)
{
	uint8_t subevent = *((const uint8_t *) data);
	const struct subevent_data *subevent_data = NULL;
	int i;

	for (i = 0; subevent_table[i].str; i++) {
		if (subevent_table[i].subevent == subevent) {
			subevent_data = &subevent_table[i];
			break;
		}
	}

	print_field("Subevent: %s (0x%2.2x)",
		subevent_data ? subevent_data->str : "Unknown", subevent);

	if (!subevent_data) {
		packet_hexdump(data + 1, size - 1);
		return;
	}
}

struct event_data {
	uint8_t event;
	const char *str;
	void (*func) (const void *data, uint8_t size);
	uint8_t size;
	bool fixed;
};

static const struct event_data event_table[] = {
	{ 0x01, "Inquiry Complete",
				status_evt, 1, true },
	{ 0x02, "Inquiry Result",
				inquiry_result_evt, 1, false },
	{ 0x03, "Connect Complete",
				conn_complete_evt, 11, true },
	{ 0x04, "Connect Request",
				conn_request_evt, 10, true },
	{ 0x05, "Disconnect Complete",
				disconnect_complete_evt, 4, true },
	{ 0x06, "Auth Complete",
				auth_complete_evt, 3, true },
	{ 0x07, "Remote Name Req Complete",
				remote_name_request_complete_evt, 255, true },
	{ 0x08, "Encryption Change",
				encrypt_change_evt, 4, true },
	{ 0x09, "Change Connection Link Key Complete",
				change_conn_link_key_complete_evt, 3, true },
	{ 0x0a, "Master Link Key Complete",
				master_link_key_complete_evt, 4, true },
	{ 0x0b, "Read Remote Supported Features",
				remote_features_complete_evt, 11, true },
	{ 0x0c, "Read Remote Version Complete",
				remote_version_complete_evt, 8, true },
	{ 0x0d, "QoS Setup Complete",
				qos_setup_complete_evt, 21, true },
	{ 0x0e, "Command Complete",
				cmd_complete_evt, 3, false },
	{ 0x0f, "Command Status",
				cmd_status_evt, 4, true },
	{ 0x10, "Hardware Error",
				hardware_error_evt, 1, true },
	{ 0x11, "Flush Occurred",
				flush_occurred_evt, 2, true },
	{ 0x12, "Role Change",
				role_change_evt, 8, true },
	{ 0x13, "Number of Completed Packets",
				num_completed_packets_evt, 1, false },
	{ 0x14, "Mode Change"				},
	{ 0x15, "Return Link Keys"			},
	{ 0x16, "PIN Code Request"			},
	{ 0x17, "Link Key Request"			},
	{ 0x18, "Link Key Notification"			},
	{ 0x19, "Loopback Command"			},
	{ 0x1a, "Data Buffer Overflow"			},
	{ 0x1b, "Max Slots Change",
				max_slots_change_evt, 3, true },
	{ 0x1c, "Read Clock Offset Complete"		},
	{ 0x1d, "Connection Packet Type Changed"	},
	{ 0x1e, "QoS Violation"				},
	{ 0x1f, "Page Scan Mode Change"			},
	{ 0x20, "Page Scan Repetition Mode Change",
				pscan_rep_mode_change_evt, 7, true },
	{ 0x21, "Flow Specification Complete"		},
	{ 0x22, "Inquiry Result with RSSI"		},
	{ 0x23, "Read Remote Extended Features",
				remote_ext_features_complete_evt, 13, true },
	/* reserved events */
	{ 0x2c, "Synchronous Connect Complete"		},
	{ 0x2d, "Synchronous Connect Changed"		},
	{ 0x2e, "Sniff Subrate"				},
	{ 0x2f, "Extended Inquiry Result"		},
	{ 0x30, "Encryption Key Refresh Complete"	},
	{ 0x31, "IO Capability Request"			},
	{ 0x32, "IO Capability Response"		},
	{ 0x33, "User Confirmation Request"		},
	{ 0x34, "User Passkey Request"			},
	{ 0x35, "Remote OOB Data Request"		},
	{ 0x36, "Simple Pairing Complete"		},
	/* reserved event */
	{ 0x38, "Link Supervision Timeout Change"	},
	{ 0x39, "Enhanced Flush Complete"		},
	/* reserved event */
	{ 0x3b, "User Passkey Notification"		},
	{ 0x3c, "Keypress Notification"			},
	{ 0x3d, "Remote Host Supported Features",
				remote_host_features_notify_evt, 14, true },
	{ 0x3e, "LE Meta Event",
				le_meta_event_evt, 1, false },
	/* reserved event */
	{ 0x40, "Physical Link Complete"		},
	{ 0x41, "Channel Selected"			},
	{ 0x42, "Disconn Physical Link Complete"	},
	{ 0x43, "Physical Link Loss Early Warning"	},
	{ 0x44, "Physical Link Recovery"		},
	{ 0x45, "Logical Link Complete"			},
	{ 0x46, "Disconn Logical Link Complete"		},
	{ 0x47, "Flow Spec Modify Complete"		},
	{ 0x48, "Number Of Completed Data Blocks"	},
	{ 0x49, "AMP Start Test"			},
	{ 0x4a, "AMP Test End"				},
	{ 0x4b, "AMP Receiver Report"			},
	{ 0x4c, "Short Range Mode Change Complete"	},
	{ 0x4d, "AMP Status Change"			},
	{ 0xfe, "Testing"				},
	{ 0xff, "Vendor"				},
	{ }
};

void packet_new_index(struct timeval *tv, uint16_t index, const char *label,
				uint8_t type, uint8_t bus, const char *name)
{
	print_header(tv, index);

	printf("= New Index: %s (%s,%s,%s)\n", label,
				hci_typetostr(type), hci_bustostr(bus), name);
}

void packet_del_index(struct timeval *tv, uint16_t index, const char *label)
{
	print_header(tv, index);

	printf("= Delete Index: %s\n", label);
}

void packet_hci_command(struct timeval *tv, uint16_t index,
					const void *data, uint16_t size)
{
	const hci_command_hdr *hdr = data;
	uint16_t opcode = btohs(hdr->opcode);
	uint16_t ogf = cmd_opcode_ogf(opcode);
	uint16_t ocf = cmd_opcode_ocf(opcode);
	const struct opcode_data *opcode_data = NULL;
	int i;

	print_header(tv, index);

	if (size < HCI_COMMAND_HDR_SIZE) {
		printf("* Malformed HCI Command packet\n");
		return;
	}

	data += HCI_COMMAND_HDR_SIZE;
	size -= HCI_COMMAND_HDR_SIZE;

	if (size != hdr->plen) {
		printf("* Invalid HCI Command packet size\n");
		return;
	}

	for (i = 0; opcode_table[i].str; i++) {
		if (opcode_table[i].opcode == opcode) {
			opcode_data = &opcode_table[i];
			break;
		}
	}

	printf("< HCI Command: %s (0x%2.2x|0x%4.4x) plen %d\n",
				opcode_data ? opcode_data->str : "Unknown",
							ogf, ocf, hdr->plen);

	if (!opcode_data || !opcode_data->cmd_func) {
		packet_hexdump(data, size);
		return;
	}

	if (opcode_data->cmd_fixed) {
		if (hdr->plen != opcode_data->cmd_size) {
			print_field("invalid packet size");
			packet_hexdump(data, size);
			return;
		}
	} else {
		if (hdr->plen < opcode_data->cmd_size) {
			print_field("too short packet");
			packet_hexdump(data, size);
			return;
		}
	}

	opcode_data->cmd_func(data, hdr->plen);
}

void packet_hci_event(struct timeval *tv, uint16_t index,
					const void *data, uint16_t size)
{
	const hci_event_hdr *hdr = data;
	const struct event_data *event_data = NULL;
	int i;

	print_header(tv, index);

	if (size < HCI_EVENT_HDR_SIZE) {
		printf("* Malformed HCI Event packet\n");
		return;
	}

	data += HCI_EVENT_HDR_SIZE;
	size -= HCI_EVENT_HDR_SIZE;

	if (size != hdr->plen) {
		printf("* Invalid HCI Event packet size\n");
		return;
	}

	for (i = 0; event_table[i].str; i++) {
		if (event_table[i].event == hdr->evt) {
			event_data = &event_table[i];
			break;
		}
	}

	printf("> HCI Event: %s (0x%2.2x) plen %d\n",
				event_data ? event_data->str : "Unknown",
							hdr->evt, hdr->plen);

	if (!event_data || !event_data->func) {
		packet_hexdump(data, size);
		return;
	}

	if (event_data->fixed) {
		if (hdr->plen != event_data->size) {
			print_field("invalid packet size");
			packet_hexdump(data, size);
			return;
		}
	} else {
		if (hdr->plen < event_data->size) {
			print_field("too short packet");
			packet_hexdump(data, size);
			return;
		}
	}

	event_data->func(data, hdr->plen);
}

void packet_hci_acldata(struct timeval *tv, uint16_t index, bool in,
					const void *data, uint16_t size)
{
	const hci_acl_hdr *hdr = data;
	uint16_t handle = btohs(hdr->handle);
	uint16_t dlen = btohs(hdr->dlen);
	uint8_t flags = acl_flags(handle);

	print_header(tv, index);

	if (size < HCI_ACL_HDR_SIZE) {
		printf("* Malformed ACL Data %s packet\n", in ? "RX" : "TX");
		return;
	}

	printf("%c ACL Data: handle %d flags 0x%2.2x dlen %d\n",
			in ? '>' : '<', acl_handle(handle), flags, dlen);

	data += HCI_ACL_HDR_SIZE;
	size -= HCI_ACL_HDR_SIZE;

	if (filter_mask & PACKET_FILTER_SHOW_ACL_DATA)
		packet_hexdump(data, size);
}

void packet_hci_scodata(struct timeval *tv, uint16_t index, bool in,
					const void *data, uint16_t size)
{
	const hci_sco_hdr *hdr = data;
	uint16_t handle = btohs(hdr->handle);
	uint8_t flags = acl_flags(handle);

	print_header(tv, index);

	if (size < HCI_SCO_HDR_SIZE) {
		printf("* Malformed SCO Data %s packet\n", in ? "RX" : "TX");
		return;
	}

	printf("%c SCO Data: handle %d flags 0x%2.2x dlen %d\n",
			in ? '>' : '<', acl_handle(handle), flags, hdr->dlen);

	data += HCI_SCO_HDR_SIZE;
	size -= HCI_SCO_HDR_SIZE;

	if (filter_mask & PACKET_FILTER_SHOW_SCO_DATA)
		packet_hexdump(data, size);
}
