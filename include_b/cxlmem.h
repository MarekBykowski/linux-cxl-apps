/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2020-2021 Intel Corporation. */
#ifndef __CXL_MEM_H__
#define __CXL_MEM_H__
#include "../include/linux/cxl_mem.h"

#define BIT(x) (1UL << (x))

enum cxl_opcode {
	CXL_MBOX_OP_INVALID		= 0x0000,
	CXL_MBOX_OP_RAW			= CXL_MBOX_OP_INVALID,
	CXL_MBOX_OP_GET_FW_INFO		= 0x0200,
	CXL_MBOX_OP_ACTIVATE_FW		= 0x0202,
	CXL_MBOX_OP_GET_SUPPORTED_LOGS	= 0x0400,
	CXL_MBOX_OP_GET_LOG		= 0x0401,
	CXL_MBOX_OP_IDENTIFY		= 0x4000,
	CXL_MBOX_OP_GET_PARTITION_INFO	= 0x4100,
	CXL_MBOX_OP_SET_PARTITION_INFO	= 0x4101,
	CXL_MBOX_OP_GET_LSA		= 0x4102,
	CXL_MBOX_OP_SET_LSA		= 0x4103,
	CXL_MBOX_OP_GET_HEALTH_INFO	= 0x4200,
	CXL_MBOX_OP_GET_ALERT_CONFIG	= 0x4201,
	CXL_MBOX_OP_SET_ALERT_CONFIG	= 0x4202,
	CXL_MBOX_OP_GET_SHUTDOWN_STATE	= 0x4203,
	CXL_MBOX_OP_SET_SHUTDOWN_STATE	= 0x4204,
	CXL_MBOX_OP_GET_POISON		= 0x4300,
	CXL_MBOX_OP_INJECT_POISON	= 0x4301,
	CXL_MBOX_OP_CLEAR_POISON	= 0x4302,
	CXL_MBOX_OP_GET_SCAN_MEDIA_CAPS	= 0x4303,
	CXL_MBOX_OP_SCAN_MEDIA		= 0x4304,
	CXL_MBOX_OP_GET_SCAN_MEDIA	= 0x4305,
	CXL_MBOX_OP_GET_SECURITY_STATE	= 0x4500,
	CXL_MBOX_OP_SET_PASSPHRASE	= 0x4501,
	CXL_MBOX_OP_DISABLE_PASSPHRASE	= 0x4502,
	CXL_MBOX_OP_UNLOCK		= 0x4503,
	CXL_MBOX_OP_FREEZE_SECURITY	= 0x4504,
	CXL_MBOX_OP_PASSPHRASE_SECURE_ERASE	= 0x4505,
	CXL_MBOX_OP_MAX			= 0x10000
};

/**
 * struct cxl_mem_command - Driver representation of a memory device command
 * @info: Command information as it exists for the UAPI
 * @opcode: The actual bits used for the mailbox protocol
 * @flags: Set of flags effecting driver behavior.
 *
 *  * %CXL_CMD_FLAG_FORCE_ENABLE: In cases of error, commands with this flag
 *    will be enabled by the driver regardless of what hardware may have
 *    advertised.
 *
 * The cxl_mem_command is the driver's internal representation of commands that
 * are supported by the driver. Some of these commands may not be supported by
 * the hardware. The driver will use @info to validate the fields passed in by
 * the user then submit the @opcode to the hardware.
 *
 * See struct cxl_command_info.
 */
struct cxl_mem_command {
	struct cxl_command_info info;
	enum cxl_opcode opcode;
	u32 flags;
#define CXL_CMD_FLAG_NONE 0
#define CXL_CMD_FLAG_FORCE_ENABLE BIT(0)
};

#endif
