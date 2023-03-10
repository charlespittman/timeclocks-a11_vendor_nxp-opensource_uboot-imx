// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016-2020 Toradex
 */

#include <common.h>
#include "tdx-cfg-block.h"
#include "tdx-eeprom.h"

#include <command.h>
#include <asm/cache.h>

#if defined(CONFIG_TARGET_APALIS_IMX6) || \
	defined(CONFIG_TARGET_APALIS_IMX8) || \
	defined(CONFIG_TARGET_COLIBRI_IMX6) || \
	defined(CONFIG_TARGET_COLIBRI_IMX8X) || \
	defined(CONFIG_TARGET_VERDIN_IMX8MM) || \
	defined(CONFIG_TARGET_VERDIN_IMX8MN) || \
	defined(CONFIG_TARGET_VERDIN_IMX8MP)
#include <asm/arch/sys_proto.h>
#else
#define is_cpu_type(cpu) (0)
#endif
#if defined(CONFIG_CPU_PXA27X)
#include <asm/arch-pxa/pxa.h>
#else
#define cpu_is_pxa27x(cpu) (0)
#endif
#include <cli.h>
#include <console.h>
#include <env.h>
#include <flash.h>
#include <malloc.h>
#include <mmc.h>
#include <nand.h>
#include <asm/mach-types.h>

DECLARE_GLOBAL_DATA_PTR;

#define TAG_VALID	0xcf01
#define TAG_MAC		0x0000
#define TAG_CAR_SERIAL	0x0021
#define TAG_HW		0x0008
#define TAG_INVALID	0xffff

#define TAG_FLAG_VALID	0x1

#define TDX_EEPROM_ID_MODULE		0
#define TDX_EEPROM_ID_CARRIER		1

#if defined(CONFIG_TDX_CFG_BLOCK_IS_IN_MMC)
#define TDX_CFG_BLOCK_MAX_SIZE 512
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NAND)
#define TDX_CFG_BLOCK_MAX_SIZE 64
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NOR)
#define TDX_CFG_BLOCK_MAX_SIZE 64
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_EEPROM)
#define TDX_CFG_BLOCK_MAX_SIZE 64
#else
#error Toradex config block location not set
#endif

#ifdef CONFIG_TDX_CFG_BLOCK_EXTRA
#define TDX_CFG_BLOCK_EXTRA_MAX_SIZE 64
#endif

struct toradex_tag {
	u32 len:14;
	u32 flags:2;
	u32 id:16;
};

bool valid_cfgblock;
struct toradex_hw tdx_hw_tag;
struct toradex_eth_addr tdx_eth_addr;
u32 tdx_serial;
#ifdef CONFIG_TDX_CFG_BLOCK_EXTRA
u32 tdx_car_serial;
bool valid_cfgblock_carrier;
struct toradex_hw tdx_car_hw_tag;
#endif

const char * const toradex_modules[] = {
	 [0] = "UNKNOWN MODULE",
	 [1] = "Colibri PXA270 312MHz",
	 [2] = "Colibri PXA270 520MHz",
	 [3] = "Colibri PXA320 806MHz",
	 [4] = "Colibri PXA300 208MHz",
	 [5] = "Colibri PXA310 624MHz",
	 [6] = "Colibri PXA320 806MHz IT",
	 [7] = "Colibri PXA300 208MHz XT",
	 [8] = "Colibri PXA270 312MHz",
	 [9] = "Colibri PXA270 520MHz",
	[10] = "Colibri VF50 128MB", /* not currently on sale */
	[11] = "Colibri VF61 256MB",
	[12] = "Colibri VF61 256MB IT",
	[13] = "Colibri VF50 128MB IT",
	[14] = "Colibri iMX6 Solo 256MB",
	[15] = "Colibri iMX6 DualLite 512MB",
	[16] = "Colibri iMX6 Solo 256MB IT",
	[17] = "Colibri iMX6 DualLite 512MB IT",
	[18] = "UNKNOWN MODULE",
	[19] = "UNKNOWN MODULE",
	[20] = "Colibri T20 256MB",
	[21] = "Colibri T20 512MB",
	[22] = "Colibri T20 512MB IT",
	[23] = "Colibri T30 1GB",
	[24] = "Colibri T20 256MB IT",
	[25] = "Apalis T30 2GB",
	[26] = "Apalis T30 1GB",
	[27] = "Apalis iMX6 Quad 1GB",
	[28] = "Apalis iMX6 Quad 2GB IT",
	[29] = "Apalis iMX6 Dual 512MB",
	[30] = "Colibri T30 1GB IT",
	[31] = "Apalis T30 1GB IT",
	[32] = "Colibri iMX7 Solo 256MB",
	[33] = "Colibri iMX7 Dual 512MB",
	[34] = "Apalis TK1 2GB",
	[35] = "Apalis iMX6 Dual 1GB IT",
	[36] = "Colibri iMX6ULL 256MB",
	[37] = "Apalis iMX8 QuadMax 4GB Wi-Fi / BT IT",
	[38] = "Colibri iMX8 QuadXPlus 2GB Wi-Fi / BT IT",
	[39] = "Colibri iMX7 Dual 1GB (eMMC)",
	[40] = "Colibri iMX6ULL 512MB Wi-Fi / BT IT",
	[41] = "Colibri iMX7 Dual 512MB EPDC",
	[42] = "Apalis TK1 4GB",
	[43] = "Colibri T20 512MB IT SETEK",
	[44] = "Colibri iMX6ULL 512MB IT",
	[45] = "Colibri iMX6ULL 512MB Wi-Fi / Bluetooth",
	[46] = "Apalis iMX8 QuadXPlus 2GB Wi-Fi / BT IT",
	[47] = "Apalis iMX8 QuadMax 4GB IT",
	[48] = "Apalis iMX8 QuadPlus 2GB Wi-Fi / BT",
	[49] = "Apalis iMX8 QuadPlus 2GB",
	[50] = "Colibri iMX8 QuadXPlus 2GB IT",
	[51] = "Colibri iMX8 DualX 1GB Wi-Fi / Bluetooth",
	[52] = "Colibri iMX8 DualX 1GB",
	[53] = "Apalis iMX8 QuadXPlus 2GB ECC IT",
	[54] = "Apalis iMX8 DualXPlus 1GB",
	[55] = "Verdin iMX8M Mini Quad 2GB Wi-Fi / BT IT",
	[56] = "Verdin iMX8M Nano Quad 1GB Wi-Fi / BT", /* not currently on sale */
	[57] = "Verdin iMX8M Mini DualLite 1GB",
	[58] = "Verdin iMX8M Plus Quad 4GB Wi-Fi / BT IT",
	[59] = "Verdin iMX8M Mini Quad 2GB IT",
	[60] = "Verdin iMX8M Mini DualLite 1GB WB IT",
	[61] = "Verdin iMX8M Plus Quad 2GB",
	[62] = "Colibri iMX6ULL 1GB IT (eMMC)",
	[63] = "Verdin iMX8M Plus Quad 4GB IT",
	[64] = "Verdin iMX8M Plus Quad 2GB Wi-Fi / BT IT",
	[65] = "Verdin iMX8M Plus QuadLite 1GB IT",
	[66] = "Verdin iMX8M Plus Quad 8GB Wi-Fi / BT",
};

const char * const toradex_carrier_boards[] = {
	[0] = "UNKNOWN CARRIER BOARD",
	[155] = "Dahlia",
	[156] = "Verdin Development Board",
};

const char * const toradex_display_adapters[] = {
	[0] = "UNKNOWN DISPLAY ADAPTER",
	[157] = "Verdin DSI to HDMI Adapter",
	[159] = "Verdin DSI to LVDS Adapter",
};

#ifdef CONFIG_TDX_CFG_BLOCK_IS_IN_MMC
static int tdx_cfg_block_mmc_storage(u8 *config_block, int write)
{
	struct mmc *mmc;
	int dev = CONFIG_TDX_CFG_BLOCK_DEV;
	int offset = CONFIG_TDX_CFG_BLOCK_OFFSET;
	uint part = CONFIG_TDX_CFG_BLOCK_PART;
	uint blk_start;
	int ret = 0;

	/* Read production parameter config block from eMMC */
	mmc = find_mmc_device(dev);
	if (!mmc) {
		puts("No MMC card found\n");
		ret = -ENODEV;
		goto out;
	}
	if (mmc_init(mmc)) {
		puts("MMC init failed\n");
		return -EINVAL;
	}
	if (part != mmc_get_blk_desc(mmc)->hwpart) {
		if (blk_select_hwpart_devnum(IF_TYPE_MMC, dev, part)) {
			puts("MMC partition switch failed\n");
			ret = -ENODEV;
			goto out;
		}
	}
	if (offset < 0)
		offset += mmc->capacity;
	blk_start = ALIGN(offset, mmc->write_bl_len) / mmc->write_bl_len;

	if (!write) {
		/* Careful reads a whole block of 512 bytes into config_block */
		if (blk_dread(mmc_get_blk_desc(mmc), blk_start, 1,
			      (unsigned char *)config_block) != 1) {
			ret = -EIO;
			goto out;
		}
	} else {
		/* Just writing one 512 byte block */
		if (blk_dwrite(mmc_get_blk_desc(mmc), blk_start, 1,
			       (unsigned char *)config_block) != 1) {
			ret = -EIO;
			goto out;
		}
	}

out:
	/* Switch back to regular eMMC user partition */
	blk_select_hwpart_devnum(IF_TYPE_MMC, 0, 0);

	return ret;
}
#endif

#ifdef CONFIG_TDX_CFG_BLOCK_IS_IN_NAND
static int read_tdx_cfg_block_from_nand(unsigned char *config_block)
{
	size_t size = TDX_CFG_BLOCK_MAX_SIZE;
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!mtd)
		return -ENODEV;

	/* Read production parameter config block from NAND page */
	return nand_read_skip_bad(mtd, CONFIG_TDX_CFG_BLOCK_OFFSET,
				  &size, NULL, TDX_CFG_BLOCK_MAX_SIZE,
				  config_block);
}

static int write_tdx_cfg_block_to_nand(unsigned char *config_block)
{
	size_t size = TDX_CFG_BLOCK_MAX_SIZE;

	/* Write production parameter config block to NAND page */
	return nand_write_skip_bad(get_nand_dev_by_index(0),
				   CONFIG_TDX_CFG_BLOCK_OFFSET,
				   &size, NULL, TDX_CFG_BLOCK_MAX_SIZE,
				   config_block, WITH_WR_VERIFY);
}
#endif

#ifdef CONFIG_TDX_CFG_BLOCK_IS_IN_NOR
static int read_tdx_cfg_block_from_nor(unsigned char *config_block)
{
	/* Read production parameter config block from NOR flash */
	memcpy(config_block, (void *)CONFIG_TDX_CFG_BLOCK_OFFSET,
	       TDX_CFG_BLOCK_MAX_SIZE);
	return 0;
}

static int write_tdx_cfg_block_to_nor(unsigned char *config_block)
{
	/* Write production parameter config block to NOR flash */
	return flash_write((void *)config_block, CONFIG_TDX_CFG_BLOCK_OFFSET,
			   TDX_CFG_BLOCK_MAX_SIZE);
}
#endif

#ifdef CONFIG_TDX_CFG_BLOCK_IS_IN_EEPROM
static int read_tdx_cfg_block_from_eeprom(unsigned char *config_block)
{
	return read_tdx_eeprom_data(TDX_EEPROM_ID_MODULE, 0x0, config_block,
				    TDX_CFG_BLOCK_MAX_SIZE);
}

static int write_tdx_cfg_block_to_eeprom(unsigned char *config_block)
{
	return write_tdx_eeprom_data(TDX_EEPROM_ID_MODULE, 0x0, config_block,
				     TDX_CFG_BLOCK_MAX_SIZE);
}
#endif

int read_tdx_cfg_block(void)
{
	int ret = 0;
	u8 *config_block = NULL;
	struct toradex_tag *tag;
	size_t size = TDX_CFG_BLOCK_MAX_SIZE;
	int offset;

	/* Allocate RAM area for config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return -ENOMEM;
	}

	memset(config_block, 0, size);

#if defined(CONFIG_TDX_CFG_BLOCK_IS_IN_MMC)
	ret = tdx_cfg_block_mmc_storage(config_block, 0);
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NAND)
	ret = read_tdx_cfg_block_from_nand(config_block);
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NOR)
	ret = read_tdx_cfg_block_from_nor(config_block);
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_EEPROM)
	ret = read_tdx_cfg_block_from_eeprom(config_block);
#else
	ret = -EINVAL;
#endif
	if (ret)
		goto out;

	/* Expect a valid tag first */
	tag = (struct toradex_tag *)config_block;
	if (tag->flags != TAG_FLAG_VALID || tag->id != TAG_VALID) {
		valid_cfgblock = false;
		ret = -EINVAL;
		goto out;
	}
	valid_cfgblock = true;
	offset = 4;

	/*
	 * check if there is enough space for storing tag and value of the
	 * biggest element
	 */
	while (offset + sizeof(struct toradex_tag) +
	       sizeof(struct toradex_hw) < TDX_CFG_BLOCK_MAX_SIZE) {
		tag = (struct toradex_tag *)(config_block + offset);
		offset += 4;
		if (tag->id == TAG_INVALID)
			break;

		if (tag->flags == TAG_FLAG_VALID) {
			switch (tag->id) {
			case TAG_MAC:
				memcpy(&tdx_eth_addr, config_block + offset,
				       6);

				/* NIC part of MAC address is serial number */
				tdx_serial = ntohl(tdx_eth_addr.nic) >> 8;
				break;
			case TAG_HW:
				memcpy(&tdx_hw_tag, config_block + offset, 8);
				break;
			}
		}

		/* Get to next tag according to current tags length */
		offset += tag->len * 4;
	}

	/* Cap product id to avoid issues with a yet unknown one */
	if (tdx_hw_tag.prodid >= (sizeof(toradex_modules) /
				  sizeof(toradex_modules[0])))
		tdx_hw_tag.prodid = 0;

out:
	free(config_block);
	return ret;
}

static int get_cfgblock_interactive(void)
{
	char message[CONFIG_SYS_CBSIZE];
	char *soc;
	char it = 'n';
	char wb = 'n';
	int len = 0;

	/* Unknown module by default */
	tdx_hw_tag.prodid = 0;

	if (cpu_is_pxa27x())
		sprintf(message, "Is the module the 312 MHz version? [y/N] ");
	else
		sprintf(message, "Is the module an IT version? [y/N] ");

	len = cli_readline(message);
	it = console_buffer[0];

#if defined(CONFIG_TARGET_APALIS_IMX8) || \
		defined(CONFIG_TARGET_COLIBRI_IMX6ULL) || \
		defined(CONFIG_TARGET_COLIBRI_IMX8X) || \
		defined(CONFIG_TARGET_VERDIN_IMX8MM) || \
		defined(CONFIG_TARGET_VERDIN_IMX8MP)
	sprintf(message, "Does the module have Wi-Fi / Bluetooth? [y/N] ");
	len = cli_readline(message);
	wb = console_buffer[0];
#endif

	soc = env_get("soc");
	if (!strcmp("mx6", soc)) {
#ifdef CONFIG_TARGET_APALIS_IMX6
		if (it == 'y' || it == 'Y') {
			if (is_cpu_type(MXC_CPU_MX6Q))
				tdx_hw_tag.prodid = APALIS_IMX6Q_IT;
			else
				tdx_hw_tag.prodid = APALIS_IMX6D_IT;
		} else {
			if (is_cpu_type(MXC_CPU_MX6Q))
				tdx_hw_tag.prodid = APALIS_IMX6Q;
			else
				tdx_hw_tag.prodid = APALIS_IMX6D;
		}
#elif CONFIG_TARGET_COLIBRI_IMX6
		if (it == 'y' || it == 'Y') {
			if (is_cpu_type(MXC_CPU_MX6DL))
				tdx_hw_tag.prodid = COLIBRI_IMX6DL_IT;
			else if (is_cpu_type(MXC_CPU_MX6SOLO))
				tdx_hw_tag.prodid = COLIBRI_IMX6S_IT;
		} else {
			if (is_cpu_type(MXC_CPU_MX6DL))
				tdx_hw_tag.prodid = COLIBRI_IMX6DL;
			else if (is_cpu_type(MXC_CPU_MX6SOLO))
				tdx_hw_tag.prodid = COLIBRI_IMX6S;
		}
#elif CONFIG_TARGET_COLIBRI_IMX6ULL
		if (it == 'y' || it == 'Y') {
			if (wb == 'y' || wb == 'Y')
				tdx_hw_tag.prodid = COLIBRI_IMX6ULL_WIFI_BT_IT;
			else
				if (gd->ram_size == 0x20000000)
					tdx_hw_tag.prodid = COLIBRI_IMX6ULL_IT;
				else
					tdx_hw_tag.prodid = COLIBRI_IMX6ULL_IT_EMMC;
		} else {
			if (wb == 'y' || wb == 'Y')
				tdx_hw_tag.prodid = COLIBRI_IMX6ULL_WIFI_BT;
			else
				tdx_hw_tag.prodid = COLIBRI_IMX6ULL;
		}
#endif
	} else if (!strcmp("imx7d", soc))
		if (gd->ram_size == 0x20000000)
			tdx_hw_tag.prodid = COLIBRI_IMX7D;
		else
			tdx_hw_tag.prodid = COLIBRI_IMX7D_EMMC;
	else if (!strcmp("imx7s", soc))
		tdx_hw_tag.prodid = COLIBRI_IMX7S;
	else if (is_cpu_type(MXC_CPU_IMX8QM)) {
		if (it == 'y' || it == 'Y') {
			if (wb == 'y' || wb == 'Y')
				tdx_hw_tag.prodid = APALIS_IMX8QM_WIFI_BT_IT;
			else
				tdx_hw_tag.prodid = APALIS_IMX8QM_IT;
		} else {
			if (wb == 'y' || wb == 'Y')
				tdx_hw_tag.prodid = APALIS_IMX8QP_WIFI_BT;
			else
				tdx_hw_tag.prodid = APALIS_IMX8QP;
		}
	} else if (is_cpu_type(MXC_CPU_IMX8QXP)) {
#ifdef CONFIG_TARGET_COLIBRI_IMX8X
		if (it == 'y' || it == 'Y') {
			if (wb == 'y' || wb == 'Y')
				tdx_hw_tag.prodid = COLIBRI_IMX8QXP_WIFI_BT_IT;
			else
				tdx_hw_tag.prodid = COLIBRI_IMX8QXP_IT;
		} else {
			if (wb == 'y' || wb == 'Y')
				tdx_hw_tag.prodid = COLIBRI_IMX8DX_WIFI_BT;
			else
				tdx_hw_tag.prodid = COLIBRI_IMX8DX;
		}
#endif
	} else if (is_cpu_type(MXC_CPU_IMX8MMDL)) {
		if (wb == 'y' || wb == 'Y')
			tdx_hw_tag.prodid = VERDIN_IMX8MMDL_WIFI_BT_IT;
		else
			tdx_hw_tag.prodid = VERDIN_IMX8MMDL;
	} else if (is_cpu_type(MXC_CPU_IMX8MM)) {
		if (wb == 'y' || wb == 'Y')
			tdx_hw_tag.prodid = VERDIN_IMX8MMQ_WIFI_BT_IT;
		else
			tdx_hw_tag.prodid = VERDIN_IMX8MMQ_IT;
	} else if (is_cpu_type(MXC_CPU_IMX8MN)) {
		tdx_hw_tag.prodid = VERDIN_IMX8MNQ_WIFI_BT;
	} else if (is_cpu_type(MXC_CPU_IMX8MPL)) {
		tdx_hw_tag.prodid = VERDIN_IMX8MPQL_IT;
	} else if (is_cpu_type(MXC_CPU_IMX8MP)) {
		if (wb == 'y' || wb == 'Y')
			if (gd->ram_size == 0x80000000)
				tdx_hw_tag.prodid = VERDIN_IMX8MPQ_2GB_WIFI_BT_IT;
			else if (gd->ram_size == 0x200000000)
				tdx_hw_tag.prodid = VERDIN_IMX8MPQ_8GB_WIFI_BT;
			else
				tdx_hw_tag.prodid = VERDIN_IMX8MPQ_WIFI_BT_IT;
		else
			if (it == 'y' || it == 'Y')
				tdx_hw_tag.prodid = VERDIN_IMX8MPQ_IT;
			else
				tdx_hw_tag.prodid = VERDIN_IMX8MPQ;
	} else if (!strcmp("tegra20", soc)) {
		if (it == 'y' || it == 'Y')
			if (gd->ram_size == 0x10000000)
				tdx_hw_tag.prodid = COLIBRI_T20_256MB_IT;
			else
				tdx_hw_tag.prodid = COLIBRI_T20_512MB_IT;
		else
			if (gd->ram_size == 0x10000000)
				tdx_hw_tag.prodid = COLIBRI_T20_256MB;
			else
				tdx_hw_tag.prodid = COLIBRI_T20_512MB;
	} else if (cpu_is_pxa27x()) {
		if (it == 'y' || it == 'Y')
			tdx_hw_tag.prodid = COLIBRI_PXA270_312MHZ;
		else
			tdx_hw_tag.prodid = COLIBRI_PXA270_520MHZ;
	}
#ifdef CONFIG_MACH_TYPE
	else if (!strcmp("tegra30", soc)) {
		if (CONFIG_MACH_TYPE == MACH_TYPE_APALIS_T30) {
			if (it == 'y' || it == 'Y')
				tdx_hw_tag.prodid = APALIS_T30_IT;
			else
				if (gd->ram_size == 0x40000000)
					tdx_hw_tag.prodid = APALIS_T30_1GB;
				else
					tdx_hw_tag.prodid = APALIS_T30_2GB;
		} else {
			if (it == 'y' || it == 'Y')
				tdx_hw_tag.prodid = COLIBRI_T30_IT;
			else
				tdx_hw_tag.prodid = COLIBRI_T30;
		}
	}
#endif /* CONFIG_MACH_TYPE */
	else if (!strcmp("tegra124", soc)) {
		tdx_hw_tag.prodid = APALIS_TK1_2GB;
	} else if (!strcmp("vf500", soc)) {
		if (it == 'y' || it == 'Y')
			tdx_hw_tag.prodid = COLIBRI_VF50_IT;
		else
			tdx_hw_tag.prodid = COLIBRI_VF50;
	} else if (!strcmp("vf610", soc)) {
		if (it == 'y' || it == 'Y')
			tdx_hw_tag.prodid = COLIBRI_VF61_IT;
		else
			tdx_hw_tag.prodid = COLIBRI_VF61;
	}

	if (!tdx_hw_tag.prodid) {
		printf("Module type not detectable due to unknown SoC\n");
		return -1;
	}

	while (len < 4) {
		sprintf(message, "Enter the module version (e.g. V1.1B): V");
		len = cli_readline(message);
	}

	tdx_hw_tag.ver_major = console_buffer[0] - '0';
	tdx_hw_tag.ver_minor = console_buffer[2] - '0';
	tdx_hw_tag.ver_assembly = console_buffer[3] - 'A';

	if (cpu_is_pxa27x() && tdx_hw_tag.ver_major == 1)
		tdx_hw_tag.prodid -= (COLIBRI_PXA270_312MHZ -
				       COLIBRI_PXA270_V1_312MHZ);

	while (len < 8) {
		sprintf(message, "Enter module serial number: ");
		len = cli_readline(message);
	}

	tdx_serial = simple_strtoul(console_buffer, NULL, 10);

	return 0;
}

static int get_cfgblock_barcode(char *barcode, struct toradex_hw *tag,
				u32 *serial)
{
	char revision[3] = {barcode[6], barcode[7], '\0'};

	if (strlen(barcode) < 16) {
		printf("Argument too short, barcode is 16 chars long\n");
		return -1;
	}

	/* Get hardware information from the first 8 digits */
	tag->ver_major = barcode[4] - '0';
	tag->ver_minor = barcode[5] - '0';
	tag->ver_assembly = simple_strtoul(revision, NULL, 10);

	barcode[4] = '\0';
	tag->prodid = simple_strtoul(barcode, NULL, 10);

	/* Parse second part of the barcode (serial number */
	barcode += 8;
	*serial = simple_strtoul(barcode, NULL, 10);

	return 0;
}

static int write_tag(u8 *config_block, int *offset, int tag_id,
		     u8 *tag_data, size_t tag_data_size)
{
	struct toradex_tag *tag;

	if (!offset || !config_block)
		return -EINVAL;

	tag = (struct toradex_tag *)(config_block + *offset);
	tag->id = tag_id;
	tag->flags = TAG_FLAG_VALID;
	/* len is provided as number of 32bit values after the tag */
	tag->len = (tag_data_size + sizeof(u32) - 1) / sizeof(u32);
	*offset += sizeof(struct toradex_tag);
	if (tag_data && tag_data_size) {
		memcpy(config_block + *offset, tag_data,
		       tag_data_size);
		*offset += tag_data_size;
	}

	return 0;
}

#ifdef CONFIG_TDX_CFG_BLOCK_EXTRA
int read_tdx_cfg_block_carrier(void)
{
	int ret = 0;
	u8 *config_block = NULL;
	struct toradex_tag *tag;
	size_t size = TDX_CFG_BLOCK_EXTRA_MAX_SIZE;
	int offset;

	/* Allocate RAM area for carrier config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return -ENOMEM;
	}

	memset(config_block, 0, size);

	ret = read_tdx_eeprom_data(TDX_EEPROM_ID_CARRIER, 0x0, config_block,
				   size);
	if (ret)
		return ret;

	/* Expect a valid tag first */
	tag = (struct toradex_tag *)config_block;
	if (tag->flags != TAG_FLAG_VALID || tag->id != TAG_VALID) {
		valid_cfgblock_carrier = false;
		ret = -EINVAL;
		goto out;
	}
	valid_cfgblock_carrier = true;
	offset = 4;

	while (offset + sizeof(struct toradex_tag) +
	       sizeof(struct toradex_hw) < TDX_CFG_BLOCK_MAX_SIZE) {
		tag = (struct toradex_tag *)(config_block + offset);
		offset += 4;
		if (tag->id == TAG_INVALID)
			break;

		if (tag->flags == TAG_FLAG_VALID) {
			switch (tag->id) {
			case TAG_CAR_SERIAL:
				memcpy(&tdx_car_serial, config_block + offset,
				       sizeof(tdx_car_serial));
				break;
			case TAG_HW:
				memcpy(&tdx_car_hw_tag, config_block +
				       offset, 8);
				break;
			}
		}

		/* Get to next tag according to current tags length */
		offset += tag->len * 4;
	}
out:
	free(config_block);
	return ret;
}

int check_pid8_sanity(char *pid8)
{
	char s_carrierid_verdin_dev[5];
	char s_carrierid_dahlia[5];

	sprintf(s_carrierid_verdin_dev, "0%d", VERDIN_DEVELOPMENT_BOARD);
	sprintf(s_carrierid_dahlia, "0%d", DAHLIA);

	/* sane value check, first 4 chars which represent carrier id */
	if (!strncmp(pid8, s_carrierid_verdin_dev, 4))
		return 0;

	if (!strncmp(pid8, s_carrierid_dahlia, 4))
		return 0;

	return -EINVAL;
}

int try_migrate_tdx_cfg_block_carrier(void)
{
	char pid8[8];
	int offset = 0;
	int ret = CMD_RET_SUCCESS;
	size_t size = TDX_CFG_BLOCK_EXTRA_MAX_SIZE;
	u8 *config_block;

	memset(pid8, 0x0, 8);
	ret = read_tdx_eeprom_data(TDX_EEPROM_ID_CARRIER, 0x0, (u8 *)pid8, 8);
	if (ret)
		return ret;

	if (check_pid8_sanity(pid8))
		return -EINVAL;

	/* Allocate RAM area for config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return CMD_RET_FAILURE;
	}

	memset(config_block, 0xff, size);
	/* we try parse PID8 concatenating zeroed serial number */
	tdx_car_hw_tag.ver_major = pid8[4] - '0';
	tdx_car_hw_tag.ver_minor = pid8[5] - '0';
	tdx_car_hw_tag.ver_assembly = pid8[7] - '0';

	pid8[4] = '\0';
	tdx_car_hw_tag.prodid = simple_strtoul(pid8, NULL, 10);

	/* Valid Tag */
	write_tag(config_block, &offset, TAG_VALID, NULL, 0);

	/* Product Tag */
	write_tag(config_block, &offset, TAG_HW, (u8 *)&tdx_car_hw_tag,
		  sizeof(tdx_car_hw_tag));

	/* Serial Tag */
	write_tag(config_block, &offset, TAG_CAR_SERIAL, (u8 *)&tdx_car_serial,
		  sizeof(tdx_car_serial));

	memset(config_block + offset, 0, 32 - offset);
	ret = write_tdx_eeprom_data(TDX_EEPROM_ID_CARRIER, 0x0, config_block,
				    size);
	if (ret) {
		printf("Failed to write Toradex Extra config block: %d\n",
		       ret);
		ret = CMD_RET_FAILURE;
		goto out;
	}

	printf("Successfully migrated to Toradex Config Block from PID8\n");

out:
	free(config_block);
	return ret;
}

static int get_cfgblock_carrier_interactive(void)
{
	char message[CONFIG_SYS_CBSIZE];
	int len;

	printf("Supported carrier boards:\n");
	printf("CARRIER BOARD NAME\t\t [ID]\n");
	for (int i = 0; i < sizeof(toradex_carrier_boards) /
			    sizeof(toradex_carrier_boards[0]); i++)
		if (toradex_carrier_boards[i])
			printf("%s \t\t [%d]\n", toradex_carrier_boards[i], i);

	sprintf(message, "Choose your carrier board (provide ID): ");
	len = cli_readline(message);
	tdx_car_hw_tag.prodid = simple_strtoul(console_buffer, NULL, 10);

	do {
		sprintf(message, "Enter carrier board version (e.g. V1.1B): V");
		len = cli_readline(message);
	} while (len < 4);

	tdx_car_hw_tag.ver_major = console_buffer[0] - '0';
	tdx_car_hw_tag.ver_minor = console_buffer[2] - '0';
	tdx_car_hw_tag.ver_assembly = console_buffer[3] - 'A';

	while (len < 8) {
		sprintf(message, "Enter carrier board serial number: ");
		len = cli_readline(message);
	}

	tdx_car_serial = simple_strtoul(console_buffer, NULL, 10);

	return 0;
}

static int do_cfgblock_carrier_create(cmd_tbl_t *cmdtp, int flag, int argc,
				      char * const argv[])
{
	u8 *config_block;
	size_t size = TDX_CFG_BLOCK_EXTRA_MAX_SIZE;
	int offset = 0;
	int ret = CMD_RET_SUCCESS;
	int err;
	int force_overwrite = 0;

	if (argc >= 3) {
		if (argv[2][0] == '-' && argv[2][1] == 'y')
			force_overwrite = 1;
	}

	/* Allocate RAM area for config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return CMD_RET_FAILURE;
	}

	memset(config_block, 0xff, size);
	read_tdx_cfg_block_carrier();
	if (valid_cfgblock_carrier && !force_overwrite) {
		char message[CONFIG_SYS_CBSIZE];

		sprintf(message, "A valid Toradex Carrier config block is present, still recreate? [y/N] ");

		if (!cli_readline(message))
			goto out;

		if (console_buffer[0] != 'y' &&
		    console_buffer[0] != 'Y')
			goto out;
	}

	if (argc < 3 || (force_overwrite && argc < 4)) {
		err = get_cfgblock_carrier_interactive();
	} else {
		if (force_overwrite)
			err = get_cfgblock_barcode(argv[3], &tdx_car_hw_tag,
						   &tdx_car_serial);
		else
			err = get_cfgblock_barcode(argv[2], &tdx_car_hw_tag,
						   &tdx_car_serial);
	}

	if (err) {
		ret = CMD_RET_FAILURE;
		goto out;
	}

	/* Valid Tag */
	write_tag(config_block, &offset, TAG_VALID, NULL, 0);

	/* Product Tag */
	write_tag(config_block, &offset, TAG_HW, (u8 *)&tdx_car_hw_tag,
		  sizeof(tdx_car_hw_tag));

	/* Serial Tag */
	write_tag(config_block, &offset, TAG_CAR_SERIAL, (u8 *)&tdx_car_serial,
		  sizeof(tdx_car_serial));

	memset(config_block + offset, 0, 32 - offset);
	err = write_tdx_eeprom_data(TDX_EEPROM_ID_CARRIER, 0x0, config_block,
				    size);
	if (err) {
		printf("Failed to write Toradex Extra config block: %d\n",
		       ret);
		ret = CMD_RET_FAILURE;
		goto out;
	}

	printf("Toradex Extra config block successfully written\n");

out:
	free(config_block);
	return ret;
}

#endif /* CONFIG_TDX_CFG_BLOCK_EXTRA */

static int do_cfgblock_create(cmd_tbl_t *cmdtp, int flag, int argc,
			      char * const argv[])
{
	u8 *config_block;
	size_t size = TDX_CFG_BLOCK_MAX_SIZE;
	int offset = 0;
	int ret = CMD_RET_SUCCESS;
	int err;
	int force_overwrite = 0;

	if (argc >= 3) {
#ifdef CONFIG_TDX_CFG_BLOCK_EXTRA
		if (!strcmp(argv[2], "carrier"))
			return do_cfgblock_carrier_create(cmdtp, flag,
							  --argc, ++argv);
#endif /* CONFIG_TDX_CFG_BLOCK_EXTRA */
		if (argv[2][0] == '-' && argv[2][1] == 'y')
			force_overwrite = 1;
	}

	/* Allocate RAM area for config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return CMD_RET_FAILURE;
	}

	memset(config_block, 0xff, size);

	read_tdx_cfg_block();
	if (valid_cfgblock) {
#if defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NAND)
		/*
		 * On NAND devices, recreation is only allowed if the page is
		 * empty (config block invalid...)
		 */
		printf("NAND erase block %d need to be erased before creating a Toradex config block\n",
		       CONFIG_TDX_CFG_BLOCK_OFFSET /
		       get_nand_dev_by_index(0)->erasesize);
		goto out;
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NOR)
		/*
		 * On NOR devices, recreation is only allowed if the sector is
		 * empty and write protection is off (config block invalid...)
		 */
		printf("NOR sector at offset 0x%02x need to be erased and unprotected before creating a Toradex config block\n",
		       CONFIG_TDX_CFG_BLOCK_OFFSET);
		goto out;
#else
		if (!force_overwrite) {
			char message[CONFIG_SYS_CBSIZE];

			sprintf(message,
				"A valid Toradex config block is present, still recreate? [y/N] ");

			if (!cli_readline(message))
				goto out;

			if (console_buffer[0] != 'y' &&
			    console_buffer[0] != 'Y')
				goto out;
		}
#endif
	}

	/* Parse new Toradex config block data... */
	if (argc < 3 || (force_overwrite && argc < 4)) {
		err = get_cfgblock_interactive();
	} else {
		if (force_overwrite)
			err = get_cfgblock_barcode(argv[3], &tdx_hw_tag,
						   &tdx_serial);
		else
			err = get_cfgblock_barcode(argv[2], &tdx_hw_tag,
						   &tdx_serial);
	}
	if (err) {
		ret = CMD_RET_FAILURE;
		goto out;
	}

	/* Convert serial number to MAC address (the storage format) */
	tdx_eth_addr.oui = htonl(0x00142dUL << 8);
	tdx_eth_addr.nic = htonl(tdx_serial << 8);

	/* Valid Tag */
	write_tag(config_block, &offset, TAG_VALID, NULL, 0);

	/* Product Tag */
	write_tag(config_block, &offset, TAG_HW, (u8 *)&tdx_hw_tag,
		  sizeof(tdx_hw_tag));

	/* MAC Tag */
	write_tag(config_block, &offset, TAG_MAC, (u8 *)&tdx_eth_addr,
		  sizeof(tdx_eth_addr));

	memset(config_block + offset, 0, 32 - offset);
#if defined(CONFIG_TDX_CFG_BLOCK_IS_IN_MMC)
	err = tdx_cfg_block_mmc_storage(config_block, 1);
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NAND)
	err = write_tdx_cfg_block_to_nand(config_block);
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_NOR)
	err = write_tdx_cfg_block_to_nor(config_block);
#elif defined(CONFIG_TDX_CFG_BLOCK_IS_IN_EEPROM)
	err = write_tdx_cfg_block_to_eeprom(config_block);
#else
	err = -EINVAL;
#endif
	if (err) {
		printf("Failed to write Toradex config block: %d\n", ret);
		ret = CMD_RET_FAILURE;
		goto out;
	}

	printf("Toradex config block successfully written\n");

out:
	free(config_block);
	return ret;
}

static int do_cfgblock(cmd_tbl_t *cmdtp, int flag, int argc,
		       char * const argv[])
{
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "create")) {
		return do_cfgblock_create(cmdtp, flag, argc, argv);
	} else if (!strcmp(argv[1], "reload")) {
		ret = read_tdx_cfg_block();
		if (ret) {
			printf("Failed to reload Toradex config block: %d\n",
			       ret);
			return CMD_RET_FAILURE;
		}
		return CMD_RET_SUCCESS;
	}

	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	cfgblock, 5, 0, do_cfgblock,
	"Toradex config block handling commands",
	"create [-y] [barcode] - (Re-)create Toradex config block\n"
	"create carrier [-y] [barcode] - (Re-)create Toradex Carrier config block\n"
	"cfgblock reload - Reload Toradex config block from flash"
);
