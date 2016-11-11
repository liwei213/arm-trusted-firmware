/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <bl_common.h>
#include <console.h>
#include <debug.h>
#include <dw_ufs.h>
#include <errno.h>
#include <hi3660.h>
#include <hi3660_mailbox.h>
#include <mailbox.h>
#include <mmio.h>
#include <partition/partition.h>
#include <platform.h>
#include <sp804_delay_timer.h>
#include <string.h>
#include <ufs.h>

#include "hikey960_def.h"
#include "hikey960_private.h"

/*
 * The next 2 constants identify the extents of the code & RO data region.
 * These addresses are used by the MMU setup code and therefore they must be
 * page-aligned.  It is the responsibility of the linker script to ensure that
 * __RO_START__ and __RO_END__ linker symbols refer to page-aligned addresses.
 */
#define BL2U_RO_BASE (unsigned long)(&__RO_START__)
#define BL2U_RO_LIMIT (unsigned long)(&__RO_END__)

/*
 * The next 2 constants identify the extents of the coherent memory region.
 * These addresses are used by the MMU setup code and therefore they must be
 * page-aligned.  It is the responsibility of the linker script to ensure that
 * __COHERENT_RAM_START__ and __COHERENT_RAM_END__ linker symbols refer to
 * page-aligned addresses.
 */
#define BL2U_COHERENT_RAM_BASE (unsigned long)(&__COHERENT_RAM_START__)
#define BL2U_COHERENT_RAM_LIMIT (unsigned long)(&__COHERENT_RAM_END__)

#define IPC_A53_LPM3_CHANNEL 		28
#define IPC_LPM3_A53_CHANNEL 		27

/*
 * Version 1
 */
#define IDX_START 			0
#define IDX_VERSION 			1
#define IDX_CMD 			2
#define IDX_DATA 			4

#define IPC_START_FRAME 		0xA7

/* CMD: A53 --> LPM3 */
#define IPC_CMD_USB_DOWNLOAD 		0x51 	/* no data */
#define IPC_CMD_RESPONSE_SN 		0x52 	/* [4-19]: SN (16 bytes) */

/* CMD: LPM3 --> A53 */
#define IPC_CMD_READ_SN 		0x71 	/* no data */
#define IPC_CMD_GENERATE_SN 		0x72 	/* no data */
#define IPC_CMD_GET_IMAGE 		0x73 	/* [4-7]: addr, [8-11]: size */

void bl2u_early_platform_setup(struct meminfo *mem_layout,
			       void *plat_info)
{
	/* Initialize the console to provide early debug support */
	console_init(CONSOLE_BASE, PL011_UART_CLK_IN_HZ, PL011_BAUDRATE);
}

void bl2u_plat_arch_setup(void)
{
	hikey960_init_mmu_el1(BL2U_RO_LIMIT,
			      BL31_LIMIT - BL2U_RO_LIMIT,
			      BL2U_RO_BASE,
			      BL2U_RO_LIMIT,
			      BL2U_COHERENT_RAM_BASE,
			      BL2U_COHERENT_RAM_LIMIT);
}

static void hikey960_timer_init(void)
{
	unsigned int data;

	/* unreset TIMER9 */
	mmio_write_32(CRG_REG_BASE + CRG_PERRSTDIS1_OFFSET, PERI_TIMER9_BIT);
	do {
		data = mmio_read_32(CRG_REG_BASE + CRG_PERRSTSTAT1_OFFSET);
	} while (data & PERI_TIMER9_BIT);
	/* enable TIMER9 */
	mmio_write_32(CRG_REG_BASE + CRG_PEREN1_OFFSET, PERI_TIMER9_BIT);
	do {
		data = mmio_read_32(CRG_REG_BASE + CRG_PERSTAT1_OFFSET);
	} while ((data & PERI_TIMER9_BIT) == 0);
	sp804_timer_init(TIMER9_REG_BASE, 15625, 512); 		/* 32.768KHz */
}

static void hikey960_ipc_init(void)
{
	mbox_params_t mbox_params;
	unsigned char tx_buf[MBX_MAX_DATA_LEN];
	int result;

	memset(&mbox_params, 0, sizeof(mbox_params));
	mbox_params.chans = MBX_MAX_CHANNELS;
	result = hi3660_mbox_init(&mbox_params);
	assert(result == 0);

	result = mbox_request_channel(IPC_A53_LPM3_CHANNEL, MAILBOX_DIR_TX);
	assert(result == 0);
	result = mbox_request_channel(IPC_LPM3_A53_CHANNEL, MAILBOX_DIR_RX);
	assert(result == 0);

	/*
	 * Notify LPM3 to receive images from USB.
	 */
	memset((void *)tx_buf, 0, MBX_MAX_DATA_LEN);
	tx_buf[IDX_START] = IPC_START_FRAME;
	tx_buf[IDX_VERSION] = 1;
	tx_buf[IDX_CMD] = IPC_CMD_USB_DOWNLOAD;
	tx_buf[IDX_DATA] = 1;
	result = mbox_send_message(IPC_A53_LPM3_CHANNEL, tx_buf, 4);
	assert(result == 0);
	/* Wait IPC Ack */
	for (;;);
}

//#define DEBUG_RESET_UFS

void bl2u_platform_setup(void)
{
	dw_ufs_params_t dw_ufs_params;
#ifdef DEBUG_RESET_UFS
	unsigned int data;
#endif

	hikey960_timer_init();
#ifdef DEBUG_RESET_UFS
	/* reset UFS */
	mmio_write_32(CRG_REG_BASE + CRG_PERRSTEN3_OFFSET, PERI_UFS_BIT);
	do {
		data = mmio_read_32(CRG_REG_BASE + CRG_PERRSTSTAT3_OFFSET);
	} while ((data & PERI_UFS_BIT) == 0);
	mmio_write_32(CRG_REG_BASE + CRG_PERRSTDIS3_OFFSET, PERI_UFS_BIT);
	do {
		data = mmio_read_32(CRG_REG_BASE + CRG_PERRSTSTAT3_OFFSET);
	} while (data & PERI_UFS_BIT);
#endif
	memset(&dw_ufs_params, 0, sizeof(dw_ufs_params));
	dw_ufs_params.reg_base = UFS_REG_BASE;
	dw_ufs_params.desc_base = HIKEY960_UFS_DESC_BASE;
	dw_ufs_params.desc_size = HIKEY960_UFS_DESC_SIZE;
	dw_ufs_params.flags = UFS_FLAGS_SKIPINIT;
	dw_ufs_init(&dw_ufs_params);
	hikey960_io_setup();
	/* this buffer is temporarily used to get partition information */
	partition_init(BL2U_IMAGE_ID, HIKEY960_UFS_DATA_BASE + 0x8000, 0x1000);

	hikey960_ipc_init();
}
