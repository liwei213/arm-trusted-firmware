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

#ifndef __MBR_H__
#define __MBR_H__

#define MBR_OFFSET			0

#define MBR_PRIMARY_ENTRY_OFFSET	0x1be
#define MBR_PRIMARY_ENTRY_SIZE		0x10
#define MBR_PRIMARY_ENTRY_NUMBER	4
#define MBR_CHS_ADDRESS_LEN		3

#define MBR_SIGNATURE_FIRST		0x55
#define MBR_SIGNATURE_SECOND		0xAA
#define MBR_SIGNATURE_OFFSET 		0x1FE

typedef struct mbr_entry {
	unsigned char		status;
	unsigned char		first_sector[MBR_CHS_ADDRESS_LEN];
	unsigned char		type;
	unsigned char		last_sector[MBR_CHS_ADDRESS_LEN];
	unsigned int		first_lba;
	unsigned int		sector_nums;
} mbr_entry_t;

#endif	/* __MBR_H__ */
