// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  it87.c - Part of lm_sensors, Linux kernel modules for hardware
 *           monitoring.
 *
 *  The IT8705F is an LPC-based Super I/O part that contains UARTs, a
 *  parallel port, an IR port, a MIDI port, a floppy controller, etc., in
 *  addition to an Environment Controller (Enhanced Hardware Monitor and
 *  Fan Controller)
 *
 *  This driver supports only the Environment Controller in the IT8705F and
 *  similar parts.  The other devices are supported by different drivers.
 *
 *  Supports: IT8603E  Super I/O chip w/LPC interface
 *            IT8606E  Super I/O chip w/LPC interface
 *            IT8607E  Super I/O chip w/LPC interface
 *            IT8613E  Super I/O chip w/LPC interface
 *            IT8620E  Super I/O chip w/LPC interface
 *            IT8622E  Super I/O chip w/LPC interface
 *            IT8623E  Super I/O chip w/LPC interface
 *            IT8625E  Super I/O chip w/LPC interface
 *            IT8628E  Super I/O chip w/LPC interface
 *            IT8655E  Super I/O chip w/LPC interface
 *            IT8665E  Super I/O chip w/LPC interface
 *            IT8686E  Super I/O chip w/LPC interface
 *            IT8688E  Super I/O chip w/LPC interface
 *            IT8689E  Super I/O chip w/LPC interface
 *            IT8696E  Super I/O chip w/LPC interface
 *            IT8698E  Super I/O chip w/LPC interface
 *            IT8705F  Super I/O chip w/LPC interface
 *            IT8712F  Super I/O chip w/LPC interface
 *            IT8716F  Super I/O chip w/LPC interface
 *            IT8718F  Super I/O chip w/LPC interface
 *            IT8720F  Super I/O chip w/LPC interface
 *            IT8721F  Super I/O chip w/LPC interface
 *            IT8726F  Super I/O chip w/LPC interface
 *            IT8728F  Super I/O chip w/LPC interface
 *            IT8732F  Super I/O chip w/LPC interface
 *            IT8736F  Super I/O chip w/LPC interface
 *            IT8738E  Super I/O chip w/LPC interface
 *            IT8758E  Super I/O chip w/LPC interface
 *            IT8771E  Super I/O chip w/LPC interface
 *            IT8772E  Super I/O chip w/LPC interface
 *            IT8781F  Super I/O chip w/LPC interface
 *            IT8782F  Super I/O chip w/LPC interface
 *            IT8783E/F Super I/O chip w/LPC interface
 *            IT8785E  Super I/O chip w/LPC interface
 *            IT8786E  Super I/O chip w/LPC interface
 *            IT8790E  Super I/O chip w/LPC interface
 *            IT8792E  Super I/O chip w/LPC interface
 *            IT87952E  Super I/O chip w/LPC interface
 *            Sis950   A clone of the IT8705F
 *
 *  Copyright (C) 2001 Chris Gauthron
 *  Copyright (C) 2005-2010 Jean Delvare <jdelvare@suse.de>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon-vid.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/dmi.h>
#include <linux/pci.h>
#include <asm/processor.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include "compat.h"

#ifndef IT87_DRIVER_VERSION
#define IT87_DRIVER_VERSION  "<not provided>"
#endif

#define DRVNAME "it87"

enum chips { it87, it8712, it8716, it8718, it8720, it8721, it8728, it8732,
	     it8736, it8738,
	     it8771, it8772, it8781, it8782, it8783, it8785, it8786, it8790,
	     it8792, it8603, it8606, it8607, it8613, it8620, it8622, it8625,
	     it8628, it8655, it8665, it8686, it8688, it8689, it87952, it8696,
	     it8698 };

static struct platform_device *it87_pdev[2];

#define	REG_2E	0x2e	/* The register to read/write */
#define	REG_4E	0x4e	/* Secondary register to read/write */

#define	DEV	  0x07	/* Register: Logical device select */
#define	PME	  0x04	/* The device with the fan registers in it */
#define H2RAM 0x0f   /* The device with the H2RAM registers in it */

/* The device with the IT8718F/IT8720F VID value in it */
#define	GPIO	0x07

/* Hybrid window access definitions */
#define H2RAM_LOW_BOUND 0x800   /* Lower boundary for H2RAM window */
#define H2RAM_HI_BOUND  0xfff   /* Upper boundary for H2RAM window*/

/* Normal MMIO window upper boundary */
#define MMIO_HI_BOUND   0x3ff   /* Normal MMIO upper boundary */

/* Defines ECIO port access for ECIO */
#define EXT_ECIO_EXTENT     5 /* Defines Port Range Reserved for ECIO*/
#define ECIO_DATA       0x3f0 /* Data port for ECIO */
#define ECIO_CMD_STAT   0x3f4 /* Command and status port for ECIO */
#define ECIO_CMD_READ   0xb0  /* Command for reading data */
#define ECIO_CMD_WRITE  0xb1  /* Command for writing data */
#define ECIO_CMD_OBF    0x01  /* Status bit mask for output buffer is full */
#define ECIO_CMD_IBF    0x02  /* Status bit mask for input buffer is full */
#define ECIO_Burst_MASK 0x10  /* Status bit mask for burst tranfers */

/* Timeouts and retries */
#define ECIO_STEP_TIMEOUT   (HZ)  /* ~1 second per wait */

/* Global ECIO lock: serialize all EC-IO access */
static DEFINE_MUTEX(it87_ecio_lock);

/* Defines vendor ID's for PCI to ISA bridges */
#define IT87_H2_VENDOR_AMD     0x1022
#define IT87_H2_VENDOR_INTEL   0x8086

#define	DEVID	0x20	/* Register: Device ID */
#define	DEVREV	0x22	/* Register: Device Revision */

static inline void __superio_enter(int ioreg)
{
	outb(0x87, ioreg);
	outb(0x01, ioreg);
	outb(0x55, ioreg);
	outb(ioreg == REG_4E ? 0xaa : 0x55, ioreg);
}

static inline int superio_inb(int ioreg, int reg)
{
	int val;

	outb(reg, ioreg);
	val = inb(ioreg + 1);

	return val;
}

static inline void superio_outb(int ioreg, int reg, int val)
{
	outb(reg, ioreg);
	outb(val, ioreg + 1);
}

static int superio_inw(int ioreg, int reg)
{
	return (superio_inb(ioreg, reg) << 8) | superio_inb(ioreg, reg + 1);
}

static inline void superio_select(int ioreg, int ldn)
{
	outb(DEV, ioreg);
	outb(ldn, ioreg + 1);
}

static inline int superio_enter(int ioreg, bool noentry)
{
	/*
	 * Try to reserve ioreg and ioreg + 1 for exclusive access.
	 */
	if (!request_muxed_region(ioreg, 2, DRVNAME))
		return -EBUSY;

	if (!noentry)
		__superio_enter(ioreg);
	return 0;
}

static inline void superio_exit(int ioreg, bool noexit)
{
	if (!noexit) {
		outb(0x02, ioreg);
		outb(0x02, ioreg + 1);
	}
	release_region(ioreg, 2);
}

/* PCI Read Routine */
static inline int pci_reg_read(struct pci_dev *d, u16 off, u32 *v)
{
    return pci_read_config_dword(d, off, v);
}

/* PCI Write Routine */
static inline int pci_reg_write(struct pci_dev *d, u16 off, u32 v)
{
    return pci_write_config_dword(d, off, v);
}

/* Logical device 4 registers */
#define IT8712F_DEVID 0x8712
#define IT8705F_DEVID 0x8705
#define IT8716F_DEVID 0x8716
#define IT8718F_DEVID 0x8718
#define IT8720F_DEVID 0x8720
#define IT8721F_DEVID 0x8721
#define IT8726F_DEVID 0x8726
#define IT8728F_DEVID 0x8728
#define IT8732F_DEVID 0x8732
#define IT8736F_DEVID 0x8736
#define IT8738E_DEVID 0x8738
#define IT8792E_DEVID 0x8733
#define IT8771E_DEVID 0x8771
#define IT8772E_DEVID 0x8772
#define IT8781F_DEVID 0x8781
#define IT8782F_DEVID 0x8782
#define IT8783E_DEVID 0x8783
#define IT8785E_DEVID 0x8785
#define IT8786E_DEVID 0x8786
#define IT8790E_DEVID 0x8790
#define IT8603E_DEVID 0x8603
#define IT8606E_DEVID 0x8606
#define IT8607E_DEVID 0x8607
#define IT8613E_DEVID 0x8613
#define IT8620E_DEVID 0x8620
#define IT8622E_DEVID 0x8622
#define IT8623E_DEVID 0x8623
#define IT8625E_DEVID 0x8625
#define IT8628E_DEVID 0x8628
#define IT8655E_DEVID 0x8655
#define IT8665E_DEVID 0x8665
#define IT8686E_DEVID 0x8686
#define IT8688E_DEVID 0x8688
#define IT8689E_DEVID 0x8689
#define IT87952E_DEVID 0x8695
#define IT8696E_DEVID 0x8696
#define IT8698E_DEVID 0x8698

/* Logical device 4 (Environmental Monitor) registers */
#define IT87_ACT_REG  0x30
#define IT87_BASE_REG 0x60
#define IT87_SPECIAL_CFG_REG	0xf3	/* special configuration register */

/* Global configuration registers (IT8712F and later) */
#define IT87_EC_HWM_MIO_REG	0x24	/* MMIO configuration register */
#define IT87_SIO_GPIO1_REG	0x25
#define IT87_SIO_GPIO2_REG	0x26
#define IT87_SIO_GPIO3_REG	0x27
#define IT87_SIO_GPIO4_REG	0x28
#define IT87_SIO_GPIO5_REG	0x29
#define IT87_SIO_GPIO9_REG	0xd3
#define IT87_SIO_PINX1_REG	0x2a	/* Pin selection */
#define IT87_SIO_PINX2_REG	0x2c	/* Pin selection */
#define IT87_SIO_PINX4_REG	0x2d	/* Pin selection */

/* Logical device 7 (GPIO) registers (IT8712F and later) */
#define IT87_SIO_SPI_REG	0xef	/* SPI function pin select */
#define IT87_SIO_VID_REG	0xfc	/* VID value */
#define IT87_SIO_BEEP_PIN_REG	0xf6	/* Beep pin mapping */

/* Logical device F (SMFI/H2RAM) registers (IT8790E, IT8792E, IT87952E) */
#define IT87_SMFI_ENABLE	0x30  /* SMFI enable Register (H2RAM support) */
#define IT87_SMFI_BASE_LOW  0xf5  /* SMFI address low address & feat support */
#define IT87_SMFI_BASE_HI   0xf6  /* SMFI address high byte */
#define IT87_SMFI_BASE_EX   0xfc  /* SMFI register for 24 bit SMFI addresses */

/* Force chip IDs to specified values. Should only be used for testing */
static unsigned short force_id[2];
static unsigned int force_id_cnt;

/* ACPI resource conflicts are ignored if this parameter is set to 1 */
static bool ignore_resource_conflict;

/* If set the driver uses MMIO to access the chip if supported */
static bool mmio;

/* Update battery voltage after every reading if true */
static bool update_vbat;

/* Not all BIOSes properly configure the PWM registers */
static bool fix_pwm_polarity;

/* Many IT87 constants specified below */

/* Length of ISA address segment */
#define IT87_EXTENT 8

/* Length of ISA address segment for Environmental Controller */
#define IT87_EC_EXTENT 2

/* Offset of EC registers from ISA base address */
#define IT87_EC_OFFSET 5

/* Where are the ISA address/data registers relative to the EC base address */
#define IT87_ADDR_REG_OFFSET 0
#define IT87_DATA_REG_OFFSET 1

/*----- The IT87 registers -----*/

#define IT87_REG_CONFIG        0x00

#define IT87_REG_ALARM1        0x01
#define IT87_REG_ALARM2        0x02
#define IT87_REG_ALARM3        0x03

#define IT87_REG_BANK          0x06

/*
 * The IT8718F and IT8720F have the VID value in a different register, in
 * Super-I/O configuration space.
 */
#define IT87_REG_VID           0x0a

/* Interface Selection register on other chips */
#define IT87_REG_IFSEL         0x0a

/*
 * The IT8705F and IT8712F earlier than revision 0x08 use register 0x0b
 * for fan divisors. Later IT8712F revisions must use 16-bit tachometer
 * mode.
 */
#define IT87_REG_FAN_DIV       0x0b
#define IT87_REG_FAN_16BIT     0x0c

/*
 * Monitors:
 * - up to 13 voltage (0 to 7, battery, avcc, 10 to 12)
 * - up to 6 temp (1 to 6)
 * - up to 6 fan (1 to 6)
 */

static const u8 IT87_REG_FAN[]         = { 0x0d, 0x0e, 0x0f, 0x80, 0x82, 0x4c };
static const u8 IT87_REG_FAN_MIN[]     = { 0x10, 0x11, 0x12, 0x84, 0x86, 0x4e };
static const u8 IT87_REG_FANX[]        = { 0x18, 0x19, 0x1a, 0x81, 0x83, 0x4d };
static const u8 IT87_REG_FANX_MIN[]    = { 0x1b, 0x1c, 0x1d, 0x85, 0x87, 0x4f };

static const u8 IT87_REG_FAN_8665[]    = { 0x0d, 0x0e, 0x0f, 0x80, 0x82, 0x93 };
static const u8 IT87_REG_FAN_MIN_8665[] = {
					   0x10, 0x11, 0x12, 0x84, 0x86, 0xb2 };
static const u8 IT87_REG_FANX_8665[]   = { 0x18, 0x19, 0x1a, 0x81, 0x83, 0x94 };
static const u8 IT87_REG_FANX_MIN_8665[] = {
					   0x1b, 0x1c, 0x1d, 0x85, 0x87, 0xb3 };

static const u8 IT87_REG_TEMP_OFFSET[] = { 0x56, 0x57, 0x59, 0x5a, 0x90, 0x91 };

static const u8 IT87_REG_TEMP_OFFSET_8686[] = {
					   0x56, 0x57, 0x59, 0x90, 0x91, 0x92 };

#define IT87_REG_FAN_MAIN_CTRL 0x13
#define IT87_REG_FAN_CTL       0x14

static const u8 IT87_REG_PWM[]         = { 0x15, 0x16, 0x17, 0x7f, 0xa7, 0xaf };
static const u8 IT87_REG_PWM_8665[]    = { 0x15, 0x16, 0x17, 0x1e, 0x1f, 0x92 };

static const u8 IT87_REG_PWM_DUTY[]    = { 0x63, 0x6b, 0x73, 0x7b, 0xa3, 0xab };

static const u8 IT87_REG_VIN[]	= { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
				    0x27, 0x28, 0x2f, 0x2c, 0x2d, 0x2e };

#define IT87_REG_TEMP(nr)      (0x29 + (nr))

#define IT87_REG_VIN_MAX(nr)   (0x30 + (nr) * 2)
#define IT87_REG_VIN_MIN(nr)   (0x31 + (nr) * 2)

static const u8 IT87_REG_TEMP_HIGH[]   = { 0x40, 0x42, 0x44, 0x46, 0xb4, 0xb6 };
static const u8 IT87_REG_TEMP_LOW[]    = { 0x41, 0x43, 0x45, 0x47, 0xb5, 0xb7 };

static const u8 IT87_REG_TEMP_HIGH_8686[] = {
					   0x40, 0x42, 0x44, 0xb4, 0xb6, 0xb8 };
static const u8 IT87_REG_TEMP_LOW_8686[] = {
					   0x41, 0x43, 0x45, 0xb5, 0xb7, 0xb9 };

#define IT87_REG_VIN_ENABLE   0x50
#define IT87_REG_TEMP_ENABLE  0x51
#define IT87_REG_TEMP_EXTRA   0x55
#define IT87_REG_BEEP_ENABLE  0x5c

#define IT87_REG_CHIPID       0x58
/* Smartfan enable bit for it879x superios in MMIO*/
#define IT87_SMARTFAN_ENABLE  0x947

static const u8 IT87_REG_AUTO_BASE[] = { 0x60, 0x68, 0x70, 0x78, 0xa0, 0xa8 };

#define IT87_REG_AUTO_TEMP(nr, i) (IT87_REG_AUTO_BASE[nr] + (i))
#define IT87_REG_AUTO_PWM(nr, i)  (IT87_REG_AUTO_BASE[nr] + 5 + (i))

#define IT87_REG_TEMP456_ENABLE	0x77

static const u16 IT87_REG_TEMP_SRC1[] = { 0x21d, 0x21e, 0x21f };
#define IT87_REG_TEMP_SRC2	0x23d

#define NUM_VIN			    ARRAY_SIZE(IT87_REG_VIN)
#define NUM_VIN_LIMIT		8
#define NUM_TEMP		    6
#define NUM_FAN			    ARRAY_SIZE(IT87_REG_FAN)
#define NUM_FAN_DIV		    3
#define NUM_PWM			    ARRAY_SIZE(IT87_REG_PWM)
#define NUM_AUTO_PWM	    ARRAY_SIZE(IT87_REG_PWM)

struct it87_devices {
	const char *name;
	const char * const model;
	u32 features;
	u8 num_temp_limit;
	u8 num_temp_offset;
	u8 num_temp_map;	/* Number of temperature sources for pwm */
	u8 peci_mask;
	u8 old_peci_mask;
	u8 smbus_bitmap;	/* SMBus enable bits in extra config register */
	u8 ec_special_config;
};

#define FEAT_12MV_ADC		BIT(0)
#define FEAT_NEWER_AUTOPWM	BIT(1)
#define FEAT_OLD_AUTOPWM	BIT(2)
#define FEAT_16BIT_FANS		BIT(3)
#define FEAT_TEMP_PECI		BIT(5)
#define FEAT_TEMP_OLD_PECI	BIT(6)
#define FEAT_FAN16_CONFIG	BIT(7)	/* Need to enable 16-bit fans */
#define FEAT_FIVE_FANS		BIT(8)	/* Supports five fans */
#define FEAT_VID		    BIT(9)	/* Set if chip supports VID */
#define FEAT_IN7_INTERNAL	BIT(10)	/* Set if in7 is internal */
#define FEAT_SIX_FANS		BIT(11)	/* Supports six fans */
#define FEAT_10_9MV_ADC		BIT(12)
#define FEAT_AVCC3		    BIT(13)	/* Chip supports in9/AVCC3 */
#define FEAT_FIVE_PWM		BIT(14)	/* Chip supports 5 pwm chn */
#define FEAT_SIX_PWM		BIT(15)	/* Chip supports 6 pwm chn */
#define FEAT_PWM_FREQ2		BIT(16)	/* Separate pwm freq 2 */
#define FEAT_SIX_TEMP		BIT(17)	/* Up to 6 temp sensors */
#define FEAT_VIN3_5V		BIT(18)	/* VIN3 connected to +5V */
/*
 * Disabling configuration mode on some chips can result in system
 * hang-ups and access failures to the Super-IO chip at the
 * second SIO address. Never exit configuration mode on these
 * chips to avoid the problem.
 */
#define FEAT_NOCONF		    BIT(19)	/* Chip conf mode enabled on startup */
#define FEAT_FOUR_FANS		BIT(20)	/* Supports four fans */
#define FEAT_FOUR_PWM		BIT(21)	/* Supports four fan controls */
#define FEAT_FOUR_TEMP		BIT(22)
#define FEAT_FANCTL_ONOFF	BIT(23)	/* chip has FAN_CTL ON/OFF */
#define FEAT_NEW_TEMPMAP	BIT(24)	/* new temp input selection */
#define FEAT_BANK_SEL		BIT(25)	/* Chip has multi-bank support */
#define FEAT_11MV_ADC		BIT(26)
/*
 * MMIO access on chips seem to come in three flavors
 * MMIO (FEAT_MMIO) Accesses a preconfigured memory space.
 * (Initialized on Startup)
 * MMIO (FEAT_BRIDGE_MMIO) with the mapping initialized on startup
 * (Version supported by prior versions of it87)
 * MMIO with a PCI to ISA bridge.
 * (This requires us to configure the PCI to ISA bridges
 * present in the system to allow access).
 * MMIO (FEAT_MMIO_H2RAM) Device has H2RAM based MMIO Access.
 * (H2RAM MMIO composes the base address differently than normal MMIO access).
*/
#define FEAT_MMIO		    BIT(27)	/* Chip supports MMIO */
#define FEAT_BRIDGE_MMIO    BIT(28) /* Chip Supports PCI bridge based MMIO */
#define FEAT_MMIO_H2RAM     BIT(29) /* Chip Supports H2RAM MMIO access */
#define FEAT_H2RAM_EX_ADDR   BIT(30) /* Chip supports 24 bit H2RAM address */
/*
 * ECIO (FEAT_ECIO). Some chip configurations use a special I/O port pair
 * to access H2RAM on AMD systems.
 * Usually by checking if the SMFI_Enable Register is set to 0x00.
*/
#define FEAT_ECIO_H2RAM     BIT(31) /* Chip Supports H2RAM access via ECIO */

static const struct it87_devices it87_devices[] = {
	[it87] = {
		.name = "it87",
		.model = "IT87F",
		.features = FEAT_OLD_AUTOPWM | FEAT_FANCTL_ONOFF,
		/* may need to overwrite */
		.num_temp_limit = 3,
		.num_temp_offset = 0,
		.num_temp_map = 3,
	},
	[it8712] = {
		.name = "it8712",
		.model = "IT8712F",
		.features = FEAT_OLD_AUTOPWM | FEAT_VID | FEAT_FANCTL_ONOFF,
		/* may need to overwrite */
		.num_temp_limit = 3,
		.num_temp_offset = 0,
		.num_temp_map = 3,
	},
	[it8716] = {
		.name = "it8716",
		.model = "IT8716F",
		.features = FEAT_16BIT_FANS | FEAT_VID
		  | FEAT_FAN16_CONFIG | FEAT_FIVE_FANS | FEAT_PWM_FREQ2
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
	},
	[it8718] = {
		.name = "it8718",
		.model = "IT8718F",
		.features = FEAT_16BIT_FANS | FEAT_VID
		  | FEAT_TEMP_OLD_PECI | FEAT_FAN16_CONFIG | FEAT_FIVE_FANS
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.old_peci_mask = 0x4,
	},
	[it8720] = {
		.name = "it8720",
		.model = "IT8720F",
		.features = FEAT_16BIT_FANS | FEAT_VID
		  | FEAT_TEMP_OLD_PECI | FEAT_FAN16_CONFIG | FEAT_FIVE_FANS
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.old_peci_mask = 0x4,
	},
	[it8721] = {
		.name = "it8721",
		.model = "IT8721F",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_TEMP_PECI
		  | FEAT_FAN16_CONFIG | FEAT_FIVE_FANS | FEAT_IN7_INTERNAL
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x05,
		.old_peci_mask = 0x02,  /* Actually reports PCH */
	},
	[it8728] = {
		.name = "it8728",
		.model = "IT8728F",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_FIVE_FANS
		  | FEAT_IN7_INTERNAL | FEAT_PWM_FREQ2
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 6,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8732] = {
		.name = "it8732",
		.model = "IT8732F",
		.features = FEAT_NEWER_AUTOPWM | FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_TEMP_PECI
		  | FEAT_10_9MV_ADC | FEAT_IN7_INTERNAL | FEAT_FOUR_FANS
		  | FEAT_FOUR_PWM | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
		.old_peci_mask = 0x02,  /* Actually reports PCH */
	},
	[it8736] = {
		.name = "it8736",
		.model = "IT8736F",
		.features = FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_TEMP_PECI
		  | FEAT_10_9MV_ADC | FEAT_IN7_INTERNAL | FEAT_FOUR_FANS
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
		.old_peci_mask = 0x02,  /* Actually reports PCH */
	},
	[it8738] = {
		.name = "it8738",
		.model = "IT8738E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_TEMP_PECI
		  | FEAT_10_9MV_ADC | FEAT_IN7_INTERNAL
		  | FEAT_FANCTL_ONOFF
		  | FEAT_AVCC3,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
		.old_peci_mask = 0x02,
	},
	[it8771] = {
		.name = "it8771",
		.model = "IT8771E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
				/* PECI: guesswork */
				/* 12mV ADC (OHM) */
				/* 16 bit fans (OHM) */
				/* three fans, always 16 bit (guesswork) */
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8772] = {
		.name = "it8772",
		.model = "IT8772E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
				/* PECI (coreboot) */
				/* 12mV ADC (HWSensors4, OHM) */
				/* 16 bit fans (HWSensors4, OHM) */
				/* three fans, always 16 bit (datasheet) */
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8781] = {
		.name = "it8781",
		.model = "IT8781F",
		.features = FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_FAN16_CONFIG | FEAT_PWM_FREQ2
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.old_peci_mask = 0x4,
	},
	[it8782] = {
		.name = "it8782",
		.model = "IT8782F",
		.features = FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_FAN16_CONFIG | FEAT_PWM_FREQ2
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.old_peci_mask = 0x4,
	},
	[it8783] = {
		.name = "it8783",
		.model = "IT8783E/F",
		.features = FEAT_16BIT_FANS
		  | FEAT_TEMP_OLD_PECI | FEAT_FAN16_CONFIG | FEAT_PWM_FREQ2
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.old_peci_mask = 0x4,
	},
	[it8785] = {
		.name = "it8785",
		.model = "IT8785E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8786] = {
		.name = "it8786",
		.model = "IT8786E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL
		  | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8790] = {
		.name = "it8790",
		.model = "IT8790E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_10_9MV_ADC
		  | FEAT_16BIT_FANS | FEAT_TEMP_PECI
		  | FEAT_IN7_INTERNAL | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF
		  | FEAT_NOCONF | FEAT_MMIO_H2RAM,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8792] = {
		.name = "it8792",
		.model = "IT8792E/IT8795E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_11MV_ADC
		  | FEAT_16BIT_FANS | FEAT_TEMP_PECI
		  | FEAT_IN7_INTERNAL | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF
		  | FEAT_NOCONF | FEAT_MMIO_H2RAM | FEAT_ECIO_H2RAM,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8603] = {
		.name = "it8603",
		.model = "IT8603E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL
		  | FEAT_AVCC3 | FEAT_PWM_FREQ2,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 4,
		.peci_mask = 0x07,
	},
	[it8606] = {
		.name = "it8606",
		.model = "IT8606E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL
		  | FEAT_AVCC3 | FEAT_PWM_FREQ2,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8607] = {
		.name = "it8607",
		.model = "IT8607E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_IN7_INTERNAL | FEAT_NEW_TEMPMAP
		  | FEAT_AVCC3 | FEAT_PWM_FREQ2
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 6,
		.peci_mask = 0x07,
	},
	[it8613] = {
		.name = "it8613",
		.model = "IT8613E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_11MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_FIVE_FANS
		  | FEAT_FIVE_PWM | FEAT_IN7_INTERNAL | FEAT_PWM_FREQ2
		  | FEAT_AVCC3 | FEAT_NEW_TEMPMAP,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 6,
		.peci_mask = 0x07,
	},
	[it8620] = {
		.name = "it8620",
		.model = "IT8620E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_SIX_FANS
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_VIN3_5V | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8622] = {
		.name = "it8622",
		.model = "IT8622E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_FIVE_FANS | FEAT_FOUR_TEMP
		  | FEAT_FIVE_PWM | FEAT_IN7_INTERNAL | FEAT_PWM_FREQ2
		  | FEAT_AVCC3 | FEAT_VIN3_5V,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 4,
		.peci_mask = 0x0f,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
	[it8625] = {
		.name = "it8625",
		.model = "IT8625E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_16BIT_FANS
		  | FEAT_AVCC3 | FEAT_NEW_TEMPMAP
		  | FEAT_11MV_ADC | FEAT_IN7_INTERNAL | FEAT_SIX_FANS
		  | FEAT_SIX_PWM | FEAT_BANK_SEL,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 6,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
	[it8628] = {
		.name = "it8628",
		.model = "IT8628E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_TEMP_PECI | FEAT_SIX_FANS
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_AVCC3
		  | FEAT_FANCTL_ONOFF,
		.num_temp_limit = 6,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8655] = {
		.name = "it8655",
		.model = "IT8655E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_16BIT_FANS
		  | FEAT_AVCC3 | FEAT_NEW_TEMPMAP
		  | FEAT_10_9MV_ADC | FEAT_IN7_INTERNAL | FEAT_BANK_SEL
		  | FEAT_SIX_TEMP | FEAT_MMIO,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 6,
		.smbus_bitmap = BIT(2),
	},
	[it8665] = {
		.name = "it8665",
		.model = "IT8665E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_16BIT_FANS
		  | FEAT_AVCC3 | FEAT_NEW_TEMPMAP
		  | FEAT_10_9MV_ADC | FEAT_IN7_INTERNAL | FEAT_SIX_FANS
		  | FEAT_SIX_PWM | FEAT_BANK_SEL | FEAT_MMIO | FEAT_SIX_TEMP,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 6,
		.smbus_bitmap = BIT(2),
	},
	[it8686] = {
		.name = "it8686",
		.model = "IT8686E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC
		  | FEAT_16BIT_FANS | FEAT_SIX_FANS | FEAT_NEW_TEMPMAP
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_BANK_SEL | FEAT_AVCC3,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 7,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
	[it8688] = {
		.name = "it8688",
		.model = "IT8688E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_SIX_FANS | FEAT_NEW_TEMPMAP
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_BANK_SEL | FEAT_AVCC3 | FEAT_BRIDGE_MMIO,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 7,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
	[it8689] = {
		.name = "it8689",
		.model = "IT8689E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_SIX_FANS | FEAT_NEW_TEMPMAP
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_BANK_SEL | FEAT_AVCC3 | FEAT_BRIDGE_MMIO,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 7,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
	[it87952] = {
		.name = "it87952",
		.model = "IT87952E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_11MV_ADC
		  | FEAT_16BIT_FANS | FEAT_TEMP_PECI
		  | FEAT_IN7_INTERNAL | FEAT_PWM_FREQ2 | FEAT_FANCTL_ONOFF
		  | FEAT_NOCONF | FEAT_MMIO_H2RAM | FEAT_H2RAM_EX_ADDR,
		.num_temp_limit = 3,
		.num_temp_offset = 3,
		.num_temp_map = 3,
		.peci_mask = 0x07,
	},
	[it8696] = {
		.name = "it8696",
		.model = "IT8696E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_SIX_FANS | FEAT_NEW_TEMPMAP
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_BANK_SEL | FEAT_AVCC3 | FEAT_BRIDGE_MMIO,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 7,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
	[it8698] = {
		.name = "it8698",
		.model = "IT8698E",
		.features = FEAT_NEWER_AUTOPWM | FEAT_12MV_ADC | FEAT_16BIT_FANS
		  | FEAT_SIX_FANS | FEAT_NEW_TEMPMAP
		  | FEAT_IN7_INTERNAL | FEAT_SIX_PWM | FEAT_PWM_FREQ2
		  | FEAT_SIX_TEMP | FEAT_BANK_SEL | FEAT_AVCC3 | FEAT_BRIDGE_MMIO,
		.num_temp_limit = 6,
		.num_temp_offset = 6,
		.num_temp_map = 7,
		.smbus_bitmap = BIT(1) | BIT(2),
	},
};

#define has_16bit_fans(data)	((data)->features & FEAT_16BIT_FANS)
#define has_12mv_adc(data)	((data)->features & FEAT_12MV_ADC)
#define has_11mv_adc(data)	((data)->features & FEAT_11MV_ADC)
#define has_10_9mv_adc(data)	((data)->features & FEAT_10_9MV_ADC)
#define has_newer_autopwm(data)	((data)->features & FEAT_NEWER_AUTOPWM)
#define has_old_autopwm(data)	((data)->features & FEAT_OLD_AUTOPWM)
#define has_temp_peci(data, nr)	(((data)->features & FEAT_TEMP_PECI) && \
				 ((data)->peci_mask & BIT(nr)))
#define has_temp_old_peci(data, nr) \
				(((data)->features & FEAT_TEMP_OLD_PECI) && \
				 ((data)->old_peci_mask & BIT(nr)))
#define has_fan16_config(data)	((data)->features & FEAT_FAN16_CONFIG)
#define has_four_fans(data)	((data)->features & (FEAT_FOUR_FANS | \
						     FEAT_FIVE_FANS | \
						     FEAT_SIX_FANS))
#define has_five_fans(data)	((data)->features & (FEAT_FIVE_FANS | \
						     FEAT_SIX_FANS))
#define has_six_fans(data)	((data)->features & FEAT_SIX_FANS)
#define has_vid(data)		((data)->features & FEAT_VID)
#define has_in7_internal(data)	((data)->features & FEAT_IN7_INTERNAL)
#define has_avcc3(data)		((data)->features & FEAT_AVCC3)
#define has_four_pwm(data)	((data)->features & (FEAT_FOUR_PWM | \
						     FEAT_FIVE_PWM | \
						     FEAT_SIX_PWM))
#define has_five_pwm(data)	((data)->features & (FEAT_FIVE_PWM | \
						     FEAT_SIX_PWM))
#define has_six_pwm(data)	((data)->features & FEAT_SIX_PWM)
#define has_pwm_freq2(data)	((data)->features & FEAT_PWM_FREQ2)
#define has_four_temp(data)	((data)->features & FEAT_FOUR_TEMP)
#define has_six_temp(data)	((data)->features & FEAT_SIX_TEMP)
#define has_vin3_5v(data)	((data)->features & FEAT_VIN3_5V)
#define has_noconf(data)	((data)->features & FEAT_NOCONF)
#define has_scaling(data)	((data)->features & (FEAT_12MV_ADC | \
						     FEAT_10_9MV_ADC | \
						     FEAT_11MV_ADC))
#define has_fanctl_onoff(data)	((data)->features & FEAT_FANCTL_ONOFF)
#define has_new_tempmap(data)	((data)->features & FEAT_NEW_TEMPMAP)
#define has_bank_sel(data)	((data)->features & FEAT_BANK_SEL)
#define has_mmio(data)		((data)->features & FEAT_MMIO)
#define has_bridge_mmio(data)   ((data)->features & FEAT_BRIDGE_MMIO)
#define has_h2ram_mmio(data)    ((data)->features & FEAT_MMIO_H2RAM)
#define has_h2ram_ex_addr(data) ((data)->features & FEAT_H2RAM_EX_ADDR)
#define has_h2ram_ecio(data)    ((data)->features & FEAT_ECIO_H2RAM)

struct it87_sio_data {
	enum chips type;
	u8 sioaddr;
	/* Values read from Super-I/O config space */
	u8 revision;
	u8 vid_value;
	u8 beep_pin;
	u8 internal;	/* Internal sensors can be labeled */
	bool need_in7_reroute;
	/* Features skipped based on config or DMI */
	u16 skip_in;
	u8 skip_vid;
	u8 skip_fan;
	u8 skip_pwm;
	u8 skip_temp;
	u8 smbus_bitmap;
	u8 ec_special_config;
	/* mmio/ecio configuration flags */
	bool mmio;
	bool mmio_h2ram;
	bool ecio_h2ram;
	bool mmio_bridge;
};

/*
 * For each registered chip, we need to keep some data in memory.
 * The structure is dynamically allocated.
 */
struct it87_data {
	const struct attribute_group *groups[7];
	enum chips type;
	u32 features;
	u8 peci_mask;
	u8 old_peci_mask;

	u8 smbus_bitmap;	/* !=0 if SMBus needs to be disabled */
	u8 saved_bank;		/* saved bank register value */
	u8 ec_special_config;	/* EC special config register restore value */
	u8 sioaddr;		/* SIO port address */

	void __iomem *mmio;	/* MMIO address if available */
    bool mmio_bridge;   /* ISA bridge MMIO without hybrid Access */
    bool mmio_h2ram;    /* ISA bridge MMIO with hybrid access */
    bool ecio_h2ram;    /* Extended ECIO ports with hybrid access. */

	int (*read)(struct it87_data *, u16);
	void (*write)(struct it87_data *, u16, u8);

	const u8 *REG_FAN;
	const u8 *REG_FANX;
	const u8 *REG_FAN_MIN;
	const u8 *REG_FANX_MIN;

	const u8 *REG_PWM;

	const u8 *REG_TEMP_OFFSET;
	const u8 *REG_TEMP_LOW;
	const u8 *REG_TEMP_HIGH;

	unsigned short addr;
	struct mutex update_lock;
	bool valid;		/* true if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 in_scaled;		/* Internal voltage sensors are scaled */
	u16 in_internal;	/* Bitfield, internal sensors (for labels) */
	u16 has_in;		/* Bitfield, voltage sensors enabled */
	u8 in[NUM_VIN][3];	/* [nr][0]=in, [1]=min, [2]=max */
	bool need_in7_reroute;
	u8 has_fan;		/* Bitfield, fans enabled */
	u16 fan[NUM_FAN][2];	/* Register values, [nr][0]=fan, [1]=min */
	u8 has_temp;		/* Bitfield, temp sensors enabled */
	s8 temp[NUM_TEMP][4];	/* [nr][0]=temp, [1]=min, [2]=max, [3]=offset */
	u8 num_temp_limit;	/* Number of temperature limit registers */
	u8 num_temp_offset;	/* Number of temperature offset registers */
	u8 temp_src[4];		/* Up to 4 temperature source registers */
	u8 sensor;		/* Register value (IT87_REG_TEMP_ENABLE) */
	u8 extra;		/* Register value (IT87_REG_TEMP_EXTRA) */
	u8 fan_div[NUM_FAN_DIV];/* Register encoding, shifted right */
	bool has_vid;		/* True if VID supported */
	u8 vid;			/* Register encoding, combined */
	u8 vrm;
	u32 alarms;		/* Register encoding, combined */
	bool has_beep;		/* true if beep supported */
	u8 beeps;		/* Register encoding */
	u8 fan_main_ctrl;	/* Register value */
	u8 fan_ctl;		/* Register value */

	/*
	 * The following 3 arrays correspond to the same registers up to
	 * the IT8720F. The meaning of bits 6-0 depends on the value of bit
	 * 7, and we want to preserve settings on mode changes, so we have
	 * to track all values separately.
	 * Starting with the IT8721F, the manual PWM duty cycles are stored
	 * in separate registers (8-bit values), so the separate tracking
	 * is no longer needed, but it is still done to keep the driver
	 * simple.
	 */
	u8 has_pwm;		/* Bitfield, pwm control enabled */
	u8 pwm_ctrl[NUM_PWM];	/* Register value */
	u8 pwm_duty[NUM_PWM];	/* Manual PWM value set by user */
	u8 pwm_temp_map[NUM_PWM];/* PWM to temp. chan. mapping (bits 1-0) */
	u8 pwm_temp_map_mask;	/* 0x03 for old, 0x07 for new temp map */
	u8 pwm_temp_map_shift;	/* 0 for old, 3 for new temp map */
	u8 pwm_num_temp_map;	/* from config data, 3..7 depending on chip */

	/* Automatic fan speed control registers */
	u8 auto_pwm[NUM_AUTO_PWM][4];	/* [nr][3] is hard-coded */
	s8 auto_temp[NUM_AUTO_PWM][5];	/* [nr][0] is point1_temp_hyst */
};

/* Board specific settings from DMI matching */
struct it87_dmi_data {
	u8 skip_pwm;		/* pwm channels to skip for this board  */
	bool skip_acpi_res;	/* ignore acpi failures on this board */
};

/* Global for results from DMI matching, if needed */
static struct it87_dmi_data *dmi_data;

/* MMIO PCI to ISA Bridge management struct */
struct it87_h2ram_handle
{
    struct pci_dev *bridge;
    bool            is_amd;
    bool            is_intel;

    /* Saved dwords we modify (captured once, restored in quiesce/release) */
    u32 r48, r60, r6c;     /* AMD: 0x48 IoMemPortDecodeEnable, 0x60 range, 0x6C ROM range 2 */
    u32 rd8, r98;          /* Intel: 0xD8 BIOSDecodeEnable, 0x98 LPCGenericMemoryRange */
    bool saved;

    /* Per-slot requested windows (idx 0=2E, 1=4E), 64KiB aligned */
    u32 base[2];
    u32 pages[2];          /* number of 64KiB pages (>=1) */
    bool have[2];

    /* AMD: track currently programmed base to minimize churn */
    u32 current_base;
};

/* Global MMIO bridge state tracker */
static struct it87_h2ram_handle it87_h2_global;
static bool                    it87_h2_global_ready;
/* Only call it87_h2_global_init() once from sm_it87_init() */
static bool                    it87_h2_global_inited;

/* Global MMIO mutex lock (serializes access to the bridge) */
static DEFINE_MUTEX(mmio_lock);

/* MMIO management enum */
enum it87_mmio_state
{
    IT87_MMIO_ORIGINAL     = -1,
    IT87_MMIO_DISABLED     = 0,
    IT87_MMIO_ENABLED_2E   = 1,   /* slot 0 (SIO 0x2E) */
    IT87_MMIO_ENABLED_4E   = 2,   /* slot 1 (SIO 0x4E) */
    IT87_MMIO_ENABLED_BOTH = 3    /* Intel only */
};

/* ==== BEGIN: Global H2RAM / ISA-bridge MMIO manager and hybrid accessors ==== */

/* ----- Intel BIOSDecodeEnable mask helpers ----- */

static u16 _intel_bios_mask_for_data_space(u32 base)
{
    if ((base & ~0xFFFFF) == 0xFF400000) return 0x0001;
    if ((base & ~0xFFFFF) == 0xFF500000) return 0x0002;
    if ((base & ~0xFFFFF) == 0xFF600000) return 0x0004;
    if ((base & ~0xFFFFF) == 0xFF700000) return 0x0008;
    if ((base & ~0xFFFF)  == 0x000E0000) return 0x0040;
    if ((base & ~0xFFFF)  == 0x000F0000) return 0x0080;
    if ((base & ~0x7FFFF) == 0xFFC00000) return 0x0100;
    if ((base & ~0x7FFFF) == 0xFFC80000) return 0x0200;
    if ((base & ~0x7FFFF) == 0xFFD00000) return 0x0400;
    if ((base & ~0x7FFFF) == 0xFFD80000) return 0x0800;
    if ((base & ~0x7FFFF) == 0xFFE00000) return 0x1000;
    if ((base & ~0x7FFFF) == 0xFFE80000) return 0x2000;
    if ((base & ~0x7FFFF) == 0xFFF00000) return 0x4000;
    if ((base & ~0x7FFFF) == 0xFFF80000) return 0x8000;
    return 0;
}

static u16 _intel_bios_mask_for_feat_space(u32 base)
{
    if ((base & ~0xFFFFF) == 0xFF000000) return 0x0001;
    if ((base & ~0xFFFFF) == 0xFF100000) return 0x0002;
    if ((base & ~0xFFFFF) == 0xFF200000) return 0x0004;
    if ((base & ~0xFFFFF) == 0xFF300000) return 0x0008;
    if ((base & ~0x7FFFF) == 0xFF800000) return 0x0100;
    if ((base & ~0x7FFFF) == 0xFF880000) return 0x0200;
    if ((base & ~0x7FFFF) == 0xFF900000) return 0x0400;
    if ((base & ~0x7FFFF) == 0xFF980000) return 0x0800;
    if ((base & ~0x7FFFF) == 0xFFA00000) return 0x1000;
    if ((base & ~0x7FFFF) == 0xFFA80000) return 0x2000;
    if ((base & ~0x7FFFF) == 0xFFB00000) return 0x4000;
    if ((base & ~0x7FFFF) == 0xFFB80000) return 0x8000;
    return 0;
}

/* ----- internal save/restore of original bridge state ----- */

static void _save_regs(struct it87_h2ram_handle *h)
{
    u16 v;

    if (!h || !h->bridge || h->saved)return;

    v = h->bridge->vendor;
    if (v==IT87_H2_VENDOR_AMD) {
        pci_reg_read(h->bridge, 0x48, &h->r48);
        pci_reg_read(h->bridge, 0x60, &h->r60);
        pci_reg_read(h->bridge, 0x6c, &h->r6c);

    } else if (v==IT87_H2_VENDOR_INTEL) {
        pci_reg_read(h->bridge, 0xd8, &h->rd8);
        pci_reg_read(h->bridge, 0x98, &h->r98);
    }

    h->saved = true;
}

static void _restore_regs(struct it87_h2ram_handle *h)
{
    u16 v;

    if (!h || !h->bridge || !h->saved)return;

    v = h->bridge->vendor;
    if (v==IT87_H2_VENDOR_AMD) {
        pci_reg_write(h->bridge, 0x48, h->r48);
        pci_reg_write(h->bridge, 0x60, h->r60);
        pci_reg_write(h->bridge, 0x6c, h->r6c);
        h->current_base = 0;

    } else if (v==IT87_H2_VENDOR_INTEL) {
        pci_reg_write(h->bridge, 0xd8, h->rd8);
        pci_reg_write(h->bridge, 0x98, h->r98);
    }
}

/* ----- discrete per-slot programming ----- */

/* AMD:
 *  slot 0 (2E): START=(base>>16)&0xFF00; END=START+pages;
 *               0x60=(END<<16)|START
 *               0x6C: preserve upper 24 bits, clear low 8 (write back)
 *               0x48: set bit5
 *  slot 1 (4E): START=(base>>16)&0xFFFF; END=START+pages;
 *               0x60=(END<<16)|START
 *               0x6C=(0xFFFF0000 | END)
 *               0x48: set bit5
 */
static int _amd_enable_slot(struct it87_h2ram_handle *h, int idx, u32 base, u32 pages)
{
    u32 start, end, val;
    int ret;

    if (idx==0) {
        start = ((base >> 16) & 0xFF00);
    } else {
        start = ((base >> 16) & 0xFFFF);
    }
    end = start + pages;

    val = (end << 16) | (start & 0xFFFF);
    ret = pci_reg_write(h->bridge, 0x60, val);
    if (ret)return ret;

    if (idx==0) {
        u32 old6c;

        ret = pci_reg_read(h->bridge, 0x6C, &old6c);
        if (ret)return ret;
        /* clear low 8 bits, keep upper 24 */
        val = (old6c & 0xFFFFFF00);
    } else {
        val = 0xFFFF0000u | (end & 0xFFFF);
    }
    ret = pci_reg_write(h->bridge, 0x6C, val);
    if (ret)return ret;

    ret = pci_reg_read(h->bridge, 0x48, &val);
    if (ret)return ret;
    val |= BIT(5);
    ret = pci_reg_write(h->bridge, 0x48, val);
    if (ret)return ret;

    h->current_base = base;
    return 0;
}

/* Intel:
 *  0x98: (START<<16)|1 with START=(base>>16)
 *  0xD8: clear enable bit(s) (active-low):
 *        - slot0: clear bit0
 *        - slot1: clear one bit chosen from address tables (fallback 0x0001)
 *  Both windows can remain enabled simultaneously.
 */
static int _intel_enable_slot(struct it87_h2ram_handle *h, int idx, u32 base)
{
    u32 start = (base >> 16) & 0xFFFF;
    u32 d;
    int ret;

    /* BIOSDecodeEnable */
    ret = pci_reg_read(h->bridge, 0xD8, &d);
    if (ret)return ret;

    if (idx==0) {
        d &= ~BIT(0);
    } else {
        u16 mask = _intel_bios_mask_for_data_space(base);

        if (!mask)mask = _intel_bios_mask_for_feat_space(base);
        if (!mask)mask = 0x0001;
        d &= ~((u32)mask);
    }
    ret = pci_reg_write(h->bridge, 0xD8, d);
    if (ret)return ret;

    /* LPCGenericMemoryRange */
    d = (start << 16) | 1u;
    ret = pci_reg_write(h->bridge, 0x98, d);
    if (ret)return ret;

    return 0;
}

static int _enable_slot(struct it87_h2ram_handle *h, int idx, u32 base, u32 pages)
{
    u16 v;

    if (!h || !h->bridge)return -ENODEV;
    _save_regs(h);

    v = h->bridge->vendor;
    if (v==IT87_H2_VENDOR_AMD)return _amd_enable_slot(h, idx, base, pages);
    if (v==IT87_H2_VENDOR_INTEL)return _intel_enable_slot(h, idx, base);
    return -ENODEV;
}

/* ----- compact internal API (per-bridge handle) ----- */

static int it87_h2_init(struct it87_h2ram_handle *h)
{
    struct pci_dev *pdev;

    if (!h)return -EINVAL;

    memset(h, 0, sizeof(*h));

    pdev = pci_get_class((PCI_CLASS_BRIDGE_ISA << 8), NULL);
    while (pdev) {
        if (pdev->vendor==IT87_H2_VENDOR_AMD || pdev->vendor==IT87_H2_VENDOR_INTEL) {
            int ret;

            h->bridge = pdev;
            pci_dev_get(h->bridge);
            ret = pcim_enable_device(h->bridge);
            pci_dev_put(pdev);
            if (ret) {
                h->bridge = NULL;
                return ret;
            }
            h->is_amd   = (h->bridge->vendor==IT87_H2_VENDOR_AMD);
            h->is_intel = (h->bridge->vendor==IT87_H2_VENDOR_INTEL);
            return 0;
        }
        pdev = pci_get_class((PCI_CLASS_BRIDGE_ISA << 8), pdev);
    }
    return -ENODEV;
}

static int it87_h2_set_slot(struct it87_h2ram_handle *h, int idx, u64 mmio_base, u32 size)
{
    u32 base32, pages;

    if (!h || !h->bridge)return -ENODEV;
    if (idx<0 || idx>1)return -EINVAL;
    if (mmio_base==0 || size==0)return -EINVAL;
    if (mmio_base > 0xFFFFFFFFull)return -ERANGE;

    base32 = (u32)mmio_base;
    base32 &= ~0xFFFFu;                        /* 64KiB align down */
    if (size < 0x10000u)size = 0x10000u;
    pages = (size + 0x10000u - 1) >> 16;       /* ceil(size/64KiB) */
    if (pages==0)pages = 1;

    h->base[idx]  = base32;
    h->pages[idx] = pages;
    h->have[idx]  = true;

    return 0;
}

static int it87_h2_set_state(struct it87_h2ram_handle *h, enum it87_mmio_state st)
{
    int ret;

    if (!h || !h->bridge)return -ENODEV;

    if (st==IT87_MMIO_ORIGINAL || st==IT87_MMIO_DISABLED) {
        _restore_regs(h);
        return 0;
    }

    if (st==IT87_MMIO_ENABLED_2E) {
        if (!h->have[0])return -EINVAL;
        if (h->is_amd) {
            if (h->current_base != h->base[0]) {
                ret = _enable_slot(h, 0, h->base[0], h->pages[0]);
                if (ret)return ret;
            }
            return 0;
        }
        return _enable_slot(h, 0, h->base[0], h->pages[0]);
    }

    if (st==IT87_MMIO_ENABLED_4E) {
        if (!h->have[1])return -EINVAL;
        if (h->is_amd) {
            if (h->current_base != h->base[1]) {
                ret = _enable_slot(h, 1, h->base[1], h->pages[1]);
                if (ret)return ret;
            }
            return 0;
        }
        return _enable_slot(h, 1, h->base[1], h->pages[1]);
    }

    if (st==IT87_MMIO_ENABLED_BOTH) {
        if (!h->have[0] || !h->have[1])return -EINVAL;

        if (h->is_amd) {
            /* AMD cannot hold two simultaneously – pick slot 0 */
            if (h->current_base != h->base[0]) {
                ret = _enable_slot(h, 0, h->base[0], h->pages[0]);
                if (ret)return ret;
            }
            return 0;
        }

        /* Intel: enable both, order is not critical */
        ret = _enable_slot(h, 0, h->base[0], h->pages[0]);
        if (ret)return ret;
        ret = _enable_slot(h, 1, h->base[1], h->pages[1]);
        if (ret)return ret;
        return 0;
    }

    return -EINVAL;
}

static enum it87_mmio_state it87_h2_get_state(struct it87_h2ram_handle *h)
{
    if (!h || !h->bridge)return IT87_MMIO_ORIGINAL;

    if (h->is_amd) {
        if (h->current_base==0)return IT87_MMIO_ORIGINAL;
        if (h->have[0] && h->current_base==h->base[0])return IT87_MMIO_ENABLED_2E;
        if (h->have[1] && h->current_base==h->base[1])return IT87_MMIO_ENABLED_4E;
        return IT87_MMIO_ORIGINAL;
    }

    if (h->have[0] && h->have[1])return IT87_MMIO_ENABLED_BOTH;
    if (h->have[0])return IT87_MMIO_ENABLED_2E;
    if (h->have[1])return IT87_MMIO_ENABLED_4E;
    return IT87_MMIO_ORIGINAL;
}

static int it87_h2_use_slot(struct it87_h2ram_handle *h, int idx)
{
    if (!h || !h->bridge)return -ENODEV;
    if (idx<0 || idx>1)return -EINVAL;
    if (!h->have[idx])return -ENOENT;

    if (h->is_amd) {
        if (h->current_base != h->base[idx]) {
            return _enable_slot(h, idx, h->base[idx], h->pages[idx]);
        }
        return 0;
    }

    /* Intel: already enabled by set_state or previous enables */
    return 0;
}

static void it87_h2_release(struct it87_h2ram_handle *h)
{
    if (!h || !h->bridge)return;
    _restore_regs(h);
    pci_dev_put(h->bridge);
    h->bridge = NULL;
}

/* ----- Global, locked API for shared MMIO bridge ----- */

/* Called once from sm_it87_init(), after at least one it87_find()
 * reported a valid mmio_address + mmio_bridge/mmio_h2ram.
 */
static int it87_h2_global_init(void)
{
    int ret;

    mutex_lock(&mmio_lock);
    ret = it87_h2_init(&it87_h2_global);
    if (!ret)
        it87_h2_global_ready = true;
    mutex_unlock(&mmio_lock);

    return ret;
}

/* Configure a slot (just updates state, does not touch PCI yet) */
static int it87_h2_global_set_slot(int idx, u64 mmio_base, u32 size)
{
    int ret;

    mutex_lock(&mmio_lock);
    if (!it87_h2_global_ready) {
        mutex_unlock(&mmio_lock);
        return -ENODEV;
    }
    ret = it87_h2_set_slot(&it87_h2_global, idx, mmio_base, size);
    mutex_unlock(&mmio_lock);

    return ret;
}

/* Program bridge to a given state (this does PCI writes) */
static int it87_h2_global_set_state(enum it87_mmio_state st)
{
    int ret;

    mutex_lock(&mmio_lock);
    if (!it87_h2_global_ready) {
        mutex_unlock(&mmio_lock);
        return -ENODEV;
    }
    ret = it87_h2_set_state(&it87_h2_global, st);
    mutex_unlock(&mmio_lock);

    return ret;
}

/* Query logical state (locked for consistency) */
static enum it87_mmio_state it87_h2_global_get_state(void)
{
    enum it87_mmio_state st;

    mutex_lock(&mmio_lock);
    if (!it87_h2_global_ready)
        st = IT87_MMIO_ORIGINAL;
    else
        st = it87_h2_get_state(&it87_h2_global);
    mutex_unlock(&mmio_lock);

    return st;
}

/* Ensure a specific slot is active (AMD may reprogram bridge) */
static int it87_h2_global_use_slot(int idx)
{
    int ret;

    mutex_lock(&mmio_lock);
    if (!it87_h2_global_ready) {
        mutex_unlock(&mmio_lock);
        return -ENODEV;
    }
    ret = it87_h2_use_slot(&it87_h2_global, idx);
    mutex_unlock(&mmio_lock);

    return ret;
}

/* Fully release: restore PCI config and drop PCI ref */
static void it87_h2_global_release(void)
{
    mutex_lock(&mmio_lock);
    if (it87_h2_global_ready) {
        it87_h2_release(&it87_h2_global);
        it87_h2_global_ready  = false;
        it87_h2_global_inited = false;
    }
    mutex_unlock(&mmio_lock);
}



/* ==== END: Global H2RAM / ISA-bridge MMIO manager and hybrid accessors ==== */

/* ECIO H2RAM Access manager */

/* ------------------------------------------------------------
 * ECIO low-level I/O helpers with debug logging
 * ------------------------------------------------------------ */

static inline u8 it87_ecio_inb(u16 port)
{
    u8 value = inb(port);

    pr_debug("ECIO inb  [0x%04x] -> 0x%02x\n",
             port, value);

    return value;
}

static inline void it87_ecio_outb(u8 value, u16 port)
{
    pr_debug("ECIO outb [0x%04x] <- 0x%02x\n",
             port, value);

    outb(value, port);
}
/* ------------------------------------------------------------
 * Low-level wait helpers (bus busy handling)
 * ------------------------------------------------------------ */

/*
 * Wait for IBF == 0 (input buffer empty, EC ready to accept a byte).
 * Returns 0 on success, -ETIMEDOUT on timeout.
 */
static int it87_ecio_wait_ibe(void)
{
    unsigned long deadline = jiffies + ECIO_STEP_TIMEOUT;

    while(time_before(jiffies, deadline)) {
        u8 status = it87_ecio_inb(ECIO_CMD_STAT);

        /* IBF clear => we can write next byte */
        if (!(status & ECIO_CMD_IBF))
            return 0;

        cpu_relax();
        udelay(10);
    }

    return -ETIMEDOUT;
}

/*
 * Wait for OBF == 1 (output buffer full, data ready to read).
 * Returns 0 on success, -ETIMEDOUT on timeout.
 */
static int it87_ecio_wait_obf(void)
{
    unsigned long deadline = jiffies + ECIO_STEP_TIMEOUT;

    while(time_before(jiffies, deadline)) {
        u8 status = it87_ecio_inb(ECIO_CMD_STAT);

        /* OBF set => data available in ECIO_DATA */
        if (status & ECIO_CMD_OBF)
            return 0;

        cpu_relax();
        udelay(10);
    }

    return -ETIMEDOUT;
}

/* ------------------------------------------------------------
 * Single-attempt EC-IO transactions (no mutex here)
 * ------------------------------------------------------------ */

/*
 * Core read:
 *   CMD_READ -> off_hi -> off_lo -> (wait OBF) -> read data
 *
 * offset must be >= 0x0100 (high byte non-zero).
 *
 *   - WaitIBE before every write
 *   - WaitIBE after every write
 *   - WaitOBF before reading data
 */
static int it87_ecio_read_once(u16 offset, u8 *value)
{
    int err;
    u8 off_hi;
    u8 off_lo;

    if (!value)
        return -EINVAL;

    off_hi = (offset >> 8) & 0xFF;
    off_lo = offset & 0xFF;

    if (off_hi == 0)
        return -EINVAL;

    /* CMD_READ (0xB0) */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(ECIO_CMD_READ, ECIO_CMD_STAT);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    /* off_hi */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(off_hi, ECIO_DATA);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    /* off_lo */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(off_lo, ECIO_DATA);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    /* wait for data to appear */
    err = it87_ecio_wait_obf();
    if (err)
        return err;

    *value = it87_ecio_inb(ECIO_DATA);
    return 0;
}

/*
 * Core write:
 *   CMD_WRITE -> off_hi -> off_lo -> data
 *
 * offset must be >= 0x0100 (high byte non-zero).
 *
 *   - WaitIBE before every write
 *   - WaitIBE after every write (including the data byte)
 */
static int it87_ecio_write_once(u16 offset, u8 value)
{
    int err;
    u8 off_hi;
    u8 off_lo;

    off_hi = (offset >> 8) & 0xFF;
    off_lo = offset & 0xFF;

    if (off_hi == 0)
        return -EINVAL;

    /* CMD_WRITE (0xB1) */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(ECIO_CMD_WRITE, ECIO_CMD_STAT);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    /* off_hi */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(off_hi, ECIO_DATA);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    /* off_lo */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(off_lo, ECIO_DATA);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    /* data */
    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    it87_ecio_outb(value, ECIO_DATA);

    err = it87_ecio_wait_ibe();
    if (err)
        return err;

    return 0;
}

/* ------------------------------------------------------------
 * Main Accessors
 * ------------------------------------------------------------ */

/*
 * it87_ecio_read
 *
 *  - reg is the EC offset (0x0100..0xFFFF)
 *  - Returns 0..255 (byte) on success
 *
 */
static int _it87_ecio_read(struct it87_data *data, u16 reg)
{
    int err;
    u8 value = 0;

    (void)data;

    mutex_lock(&it87_ecio_lock);

    err = it87_ecio_read_once(reg, &value);

    mutex_unlock(&it87_ecio_lock);

    if (err) {
        pr_debug("ECIO read failed at offset 0x%04x (err=%d)\n",
                 reg, err);
    } else {
        pr_debug("ECIO read 0x%02x from offset 0x%04x\n",
                 value, reg);
    }

    return value;
}

/*
 * it87_ecio_write
 *
 *  - reg is the EC offset (0x0100..0xFFFF)
 *  - value is the byte to write
 *  - Errors are logged; no errno is returned to caller.
 *
 */
static void _it87_ecio_write(struct it87_data *data, u16 reg, u8 value)
{
    int err;

    (void)data;

    mutex_lock(&it87_ecio_lock);

    err = it87_ecio_write_once(reg, value);

    mutex_unlock(&it87_ecio_lock);

    if (err) {
        pr_debug("ECIO write failed at offset 0x%04x "
                 "(value=0x%02x, err=%d)\n",
                 reg, value, err);
    }
}
/* End of ECIO H2RAM access manager */

static int adc_lsb(const struct it87_data *data, int nr)
{
	int lsb;

	if (has_12mv_adc(data))
		lsb = 120;
	else if (has_10_9mv_adc(data))
		lsb = 109;
	else if (has_11mv_adc(data))
		lsb = 110;
	else
		lsb = 160;
	if (data->in_scaled & BIT(nr))
		lsb <<= 1;
	return lsb;
}

static u8 in_to_reg(const struct it87_data *data, int nr, long val)
{
	val = DIV_ROUND_CLOSEST(val * 10, adc_lsb(data, nr));
	return clamp_val(val, 0, 255);
}

static int in_from_reg(const struct it87_data *data, int nr, int val)
{
	return DIV_ROUND_CLOSEST(val * adc_lsb(data, nr), 10);
}

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = clamp_val(rpm, 1, 1000000);
	return clamp_val((1350000 + rpm * div / 2) / (rpm * div), 1, 254);
}

static inline u16 FAN16_TO_REG(long rpm)
{
	if (rpm == 0)
		return 0xffff;
	return clamp_val((1350000 + rpm) / (rpm * 2), 1, 0xfffe);
}

#define FAN_FROM_REG(val, div) ((val) == 0 ? -1 : (val) == 255 ? 0 : \
				1350000 / ((val) * (div)))
/* The divider is fixed to 2 in 16-bit mode */
#define FAN16_FROM_REG(val) ((val) == 0 ? -1 : (val) == 0xffff ? 0 : \
			     1350000 / ((val) * 2))

#define TEMP_TO_REG(val) (clamp_val(((val) < 0 ? (((val) - 500) / 1000) : \
				    ((val) + 500) / 1000), -128, 127))
#define TEMP_FROM_REG(val) ((val) * 1000)

static u8 pwm_to_reg(const struct it87_data *data, long val)
{
	if (has_newer_autopwm(data))
		return val;
	else
		return val >> 1;
}

static int pwm_from_reg(const struct it87_data *data, u8 reg)
{
	if (has_newer_autopwm(data))
		return reg;
	else
		return (reg & 0x7f) << 1;
}

static int DIV_TO_REG(int val)
{
	int answer = 0;

	while (answer < 7 && (val >>= 1))
		answer++;
	return answer;
}

#define DIV_FROM_REG(val) BIT(val)

static u8 temp_map_from_reg(const struct it87_data *data, u8 reg)
{
	u8 map;

	map = (reg >> data->pwm_temp_map_shift) & data->pwm_temp_map_mask;
	if (map >= data->pwm_num_temp_map)  /* map is 0-based */
		map = 0;

	return map;
}

static u8 temp_map_to_reg(const struct it87_data *data, int nr, u8 map)
{
	u8 ctrl = data->pwm_ctrl[nr];

	return (ctrl & ~(data->pwm_temp_map_mask << data->pwm_temp_map_shift)) |
		(map << data->pwm_temp_map_shift);
}

/*
 * PWM base frequencies. The frequency has to be divided by either 128 or 256,
 * depending on the chip type, to calculate the actual PWM frequency.
 *
 * Some of the chip datasheets suggest a base frequency of 51 kHz instead
 * of 750 kHz for the slowest base frequency, resulting in a PWM frequency
 * of 200 Hz. Sometimes both PWM frequency select registers are affected,
 * sometimes just one. It is unknown if this is a datasheet error or real,
 * so this is ignored for now.
 */
static const unsigned int pwm_freq[8] = {
	48000000,
	24000000,
	12000000,
	8000000,
	6000000,
	3000000,
	1500000,
	750000,
};

static int _it87_io_read(struct it87_data *data, u16 reg)
{
	outb_p(reg, data->addr + IT87_ADDR_REG_OFFSET);
	return inb_p(data->addr + IT87_DATA_REG_OFFSET);
}

static void _it87_io_write(struct it87_data *data, u16 reg, u8 value)
{
	outb_p(reg, data->addr + IT87_ADDR_REG_OFFSET);
	outb_p(value, data->addr + IT87_DATA_REG_OFFSET);
}

static int smbus_disable(struct it87_data *data)
{
	int err;

	if (data->smbus_bitmap) {
		err = superio_enter(data->sioaddr, has_noconf(data));
		if (err)
			return err;
		superio_select(data->sioaddr, PME);
		superio_outb(data->sioaddr, IT87_SPECIAL_CFG_REG,
			     data->ec_special_config & ~data->smbus_bitmap);
		superio_exit(data->sioaddr, has_noconf(data));
		if (has_bank_sel(data) && !data->mmio)
			data->saved_bank = _it87_io_read(data, IT87_REG_BANK);
	}
	return 0;
}

static int smbus_enable(struct it87_data *data)
{
	int err;

	if (data->smbus_bitmap) {
		if (has_bank_sel(data) && !data->mmio)
			_it87_io_write(data, IT87_REG_BANK, data->saved_bank);
		err = superio_enter(data->sioaddr, has_noconf(data));
		if (err)
			return err;

		superio_select(data->sioaddr, PME);
		superio_outb(data->sioaddr, IT87_SPECIAL_CFG_REG,
			     data->ec_special_config);
		superio_exit(data->sioaddr, has_noconf(data));
	}
	return 0;
}

static u8 it87_io_set_bank(struct it87_data *data, u8 bank)
{
	u8 _bank = bank;

	if (has_bank_sel(data)) {
		u8 breg = _it87_io_read(data, IT87_REG_BANK);

		_bank = breg >> 5;
		if (bank != _bank) {
			breg &= 0x1f;
			breg |= (bank << 5);
			_it87_io_write(data, IT87_REG_BANK, breg);
		}
	}
	return _bank;
}

/*
 * Must be called with data->update_lock held, except during initialization.
 * Must be called with SMBus accesses disabled.
 * We ignore the IT87 BUSY flag at this moment - it could lead to deadlocks,
 * would slow down the IT87 access and should not be necessary.
 */
static int it87_io_read(struct it87_data *data, u16 reg)
{
	u8 bank;
	int val;

	bank = it87_io_set_bank(data, reg >> 8);
	val = _it87_io_read(data, reg & 0xff);
	it87_io_set_bank(data, bank);

	return val;
}

/*
 * Must be called with data->update_lock held, except during initialization.
 * Must be called with SMBus accesses disabled.
 * We ignore the IT87 BUSY flag at this moment - it could lead to deadlocks,
 * would slow down the IT87 access and should not be necessary.
 */
static void it87_io_write(struct it87_data *data, u16 reg, u8 value)
{
	u8 bank;

	bank = it87_io_set_bank(data, reg >> 8);
	_it87_io_write(data, reg & 0xff, value);
	it87_io_set_bank(data, bank);
}

/* ----- MMIO / hybrid backends ----- */

/* Pure MMIO helper – no bridge logic here */
static int it87_mmio_read(struct it87_data *data, u16 reg)
{
    return readb(data->mmio + reg);
}

static void it87_mmio_write(struct it87_data *data, u16 reg, u8 value)
{
    writeb(value, data->mmio + reg);
}

/* Pure MMIO via ISA bridge window (no hybrid I/O) */
static int it87_bridge_read(struct it87_data *data, u16 reg)
{
    if (likely(data->mmio) &&
       !(data->features & FEAT_MMIO) &&
       it87_h2_global_ready &&
       data->mmio_bridge) {
        int slot = (data->sioaddr==REG_4E) ? 1 : 0;

        if (unlikely(it87_h2_global_use_slot(slot)))
            return -EIO;
    }

    return it87_mmio_read(data, reg);
}

static void it87_bridge_write(struct it87_data *data, u16 reg, u8 value)
{
    if (likely(data->mmio) &&
       !(data->features & FEAT_MMIO) &&
       it87_h2_global_ready &&
       data->mmio_bridge) {
        int slot = (data->sioaddr==REG_4E) ? 1 : 0;

        if (unlikely(it87_h2_global_use_slot(slot)))
            return; /* best effort */
    }

    it87_mmio_write(data, reg, value);
}

/* Hybrid H2RAM access:
 *   0x000–0x7FF : legacy EC I/O (banked)
 *   0x800–0xFFF : MMIO via H2RAM window
 */
static int it87_h2ram_read(struct it87_data *data, u16 reg)
{
    if (reg >= H2RAM_LOW_BOUND && reg <= H2RAM_HI_BOUND) {
        if (likely(data->mmio) &&
           !(data->features & FEAT_MMIO) &&
           it87_h2_global_ready &&
           data->mmio_h2ram) {
            int slot = (data->sioaddr==REG_4E) ? 1 : 0;

            if (unlikely(it87_h2_global_use_slot(slot)))
                return -EIO;
        }
        /* High region: go through MMIO window */
        return it87_bridge_read(data, reg);
    }
    /* Low region: conventional EC I/O path */
    return _it87_io_read(data, reg);
}

static void it87_h2ram_write(struct it87_data *data, u16 reg, u8 value)
{
    if (reg >= H2RAM_LOW_BOUND && reg <= H2RAM_HI_BOUND) {
        if (likely(data->mmio) &&
           !(data->features & FEAT_MMIO) &&
           it87_h2_global_ready &&
           data->mmio_h2ram) {
            int slot = (data->sioaddr==REG_4E) ? 1 : 0;

            if (unlikely(it87_h2_global_use_slot(slot)))
                return; /* best effort */
        }

        /* High region: go through MMIO window */
        it87_bridge_write(data, reg, value);
        return;
    }

    /* Low region: conventional EC I/O path */
    _it87_io_write(data, reg, value);
}

/* ECIO + Conventional I/O path */
static int it87_ecio_read(struct it87_data *data, u16 reg)
{
    /* High region: go through ECIO window if hybrid ECIO is active */
    if(reg>=H2RAM_LOW_BOUND && reg<=H2RAM_HI_BOUND && data->ecio_h2ram) {
        return _it87_ecio_read(data, reg);
    }

    /* Low region (or no ECIO hybrid): conventional EC I/O path */
    return _it87_io_read(data, reg);
}

static void it87_ecio_write(struct it87_data *data, u16 reg, u8 value)
{
    /* High region: go through ECIO window if hybrid ECIO is active */
    if(reg>=H2RAM_LOW_BOUND && reg<=H2RAM_HI_BOUND && data->ecio_h2ram) {
        _it87_ecio_write(data, reg, value);
        return;
    }

    /* Low region (or no ECIO hybrid): conventional EC I/O path */
    _it87_io_write(data, reg, value);
}

static void it87_update_pwm_ctrl(struct it87_data *data, int nr)
{
	u8 ctrl;

	ctrl = data->read(data, data->REG_PWM[nr]);
	data->pwm_ctrl[nr] = ctrl;
	if (has_newer_autopwm(data)) {
		data->pwm_temp_map[nr] = temp_map_from_reg(data, ctrl);
		data->pwm_duty[nr] = data->read(data, IT87_REG_PWM_DUTY[nr]);
	} else {
		if (ctrl & 0x80)	/* Automatic mode */
			data->pwm_temp_map[nr] = temp_map_from_reg(data, ctrl);
		else				/* Manual mode */
			data->pwm_duty[nr] = ctrl & 0x7f;
	}

	if (has_old_autopwm(data)) {
		int i;

		for (i = 0; i < 5 ; i++)
			data->auto_temp[nr][i] = data->read(data,
						IT87_REG_AUTO_TEMP(nr, i));
		for (i = 0; i < 3 ; i++)
			data->auto_pwm[nr][i] = data->read(data,
						IT87_REG_AUTO_PWM(nr, i));
	} else if (has_newer_autopwm(data)) {
		int i;

		/*
		 * 0: temperature hysteresis (base + 5)
		 * 1: fan off temperature (base + 0)
		 * 2: fan start temperature (base + 1)
		 * 3: fan max temperature (base + 2)
		 */
		data->auto_temp[nr][0] =
			data->read(data, IT87_REG_AUTO_TEMP(nr, 5));

		for (i = 0; i < 3 ; i++)
			data->auto_temp[nr][i + 1] =
				data->read(data, IT87_REG_AUTO_TEMP(nr, i));
		/*
		 * 0: start pwm value (base + 3)
		 * 1: pwm slope (base + 4, 1/8th pwm)
		 */
		data->auto_pwm[nr][0] =
			data->read(data, IT87_REG_AUTO_TEMP(nr, 3));
		data->auto_pwm[nr][1] =
			data->read(data, IT87_REG_AUTO_TEMP(nr, 4));
	}
}

static int it87_lock(struct it87_data *data)
{
	int err;

	mutex_lock(&data->update_lock);
	err = smbus_disable(data);
	if (err)
		mutex_unlock(&data->update_lock);
	return err;
}

static void it87_unlock(struct it87_data *data)
{
	smbus_enable(data);
	mutex_unlock(&data->update_lock);
}

static struct it87_data *it87_update_device(struct device *dev)
{
	struct it87_data *data = dev_get_drvdata(dev);
	struct it87_data *ret = data;
	int err;
	int i;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + HZ + HZ / 2) ||
		       !data->valid) {
		err = smbus_disable(data);
		if (err) {
			ret = ERR_PTR(err);
			goto unlock;
		}
		if (update_vbat) {
			/*
			 * Cleared after each update, so reenable.  Value
			 * returned by this read will be previous value
			 */
			data->write(data, IT87_REG_CONFIG,
				    data->read(data, IT87_REG_CONFIG) | 0x40);
		}
		for (i = 0; i < NUM_VIN; i++) {
			if (!(data->has_in & BIT(i)))
				continue;

			data->in[i][0] = data->read(data, IT87_REG_VIN[i]);

			/* VBAT and AVCC don't have limit registers */
			if (i >= NUM_VIN_LIMIT)
				continue;

			data->in[i][1] = data->read(data, IT87_REG_VIN_MIN(i));
			data->in[i][2] = data->read(data, IT87_REG_VIN_MAX(i));
		}

		for (i = 0; i < NUM_FAN; i++) {
			/* Skip disabled fans */
			if (!(data->has_fan & BIT(i)))
				continue;

			data->fan[i][1] = data->read(data,
					data->REG_FAN_MIN[i]);
			data->fan[i][0] = data->read(data, data->REG_FAN[i]);
			/* Add high byte if in 16-bit mode */
			if (has_16bit_fans(data)) {
				data->fan[i][0] |= data->read(data,
						data->REG_FANX[i]) << 8;
				data->fan[i][1] |= data->read(data,
						data->REG_FANX_MIN[i]) << 8;
			}
		}
		for (i = 0; i < NUM_TEMP; i++) {
			if (!(data->has_temp & BIT(i)))
				continue;
			data->temp[i][0] =
				data->read(data, IT87_REG_TEMP(i));

			if (i >= data->num_temp_limit)
				continue;

			if (i < data->num_temp_offset)
				data->temp[i][3] =
				  data->read(data, data->REG_TEMP_OFFSET[i]);

			data->temp[i][1] =
				data->read(data, data->REG_TEMP_LOW[i]);
			data->temp[i][2] =
				data->read(data, data->REG_TEMP_HIGH[i]);
		}

		/* Newer chips don't have clock dividers */
		if ((data->has_fan & 0x07) && !has_16bit_fans(data)) {
			i = data->read(data, IT87_REG_FAN_DIV);
			data->fan_div[0] = i & 0x07;
			data->fan_div[1] = (i >> 3) & 0x07;
			data->fan_div[2] = (i & 0x40) ? 3 : 1;
		}

		data->alarms =
			data->read(data, IT87_REG_ALARM1) |
			(data->read(data, IT87_REG_ALARM2) << 8) |
			(data->read(data, IT87_REG_ALARM3) << 16);
		data->beeps = data->read(data, IT87_REG_BEEP_ENABLE);

		data->fan_main_ctrl = data->read(data, IT87_REG_FAN_MAIN_CTRL);
		data->fan_ctl = data->read(data, IT87_REG_FAN_CTL);
		for (i = 0; i < NUM_PWM; i++) {
			if (!(data->has_pwm & BIT(i)))
				continue;
			it87_update_pwm_ctrl(data, i);
		}

		data->sensor = data->read(data, IT87_REG_TEMP_ENABLE);
		data->extra = data->read(data, IT87_REG_TEMP_EXTRA);
		/*
		 * The IT8705F does not have VID capability.
		 * The IT8718F and later don't use IT87_REG_VID for the
		 * same purpose.
		 */
		if (data->type == it8712 || data->type == it8716) {
			data->vid = data->read(data, IT87_REG_VID);
			/*
			 * The older IT8712F revisions had only 5 VID pins,
			 * but we assume it is always safe to read 6 bits.
			 */
			data->vid &= 0x3f;
		}
		data->last_updated = jiffies;
		data->valid = true;
		smbus_enable(data);
	}
unlock:
	mutex_unlock(&data->update_lock);
	return ret;
}

static ssize_t show_in(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	struct it87_data *data = it87_update_device(dev);
	int index = sattr->index;
	int nr = sattr->nr;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", in_from_reg(data, nr, data->in[nr][index]));
}

static ssize_t set_in(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	struct it87_data *data = dev_get_drvdata(dev);
	int index = sattr->index;
	int nr = sattr->nr;
	unsigned long val;
	int err;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	data->in[nr][index] = in_to_reg(data, nr, val);
	data->write(data, index == 1 ? IT87_REG_VIN_MIN(nr)
				     : IT87_REG_VIN_MAX(nr),
		    data->in[nr][index]);
	it87_unlock(data);
	return count;
}

static SENSOR_DEVICE_ATTR_2(in0_input, S_IRUGO, show_in, NULL, 0, 0);
static SENSOR_DEVICE_ATTR_2(in0_min, S_IRUGO | S_IWUSR, show_in, set_in, 0, 1);
static SENSOR_DEVICE_ATTR_2(in0_max, S_IRUGO | S_IWUSR, show_in, set_in, 0, 2);

static SENSOR_DEVICE_ATTR_2(in1_input, S_IRUGO, show_in, NULL, 1, 0);
static SENSOR_DEVICE_ATTR_2(in1_min, S_IRUGO | S_IWUSR, show_in, set_in, 1, 1);
static SENSOR_DEVICE_ATTR_2(in1_max, S_IRUGO | S_IWUSR, show_in, set_in, 1, 2);

static SENSOR_DEVICE_ATTR_2(in2_input, S_IRUGO, show_in, NULL, 2, 0);
static SENSOR_DEVICE_ATTR_2(in2_min, S_IRUGO | S_IWUSR, show_in, set_in, 2, 1);
static SENSOR_DEVICE_ATTR_2(in2_max, S_IRUGO | S_IWUSR, show_in, set_in, 2, 2);

static SENSOR_DEVICE_ATTR_2(in3_input, S_IRUGO, show_in, NULL, 3, 0);
static SENSOR_DEVICE_ATTR_2(in3_min, S_IRUGO | S_IWUSR, show_in, set_in, 3, 1);
static SENSOR_DEVICE_ATTR_2(in3_max, S_IRUGO | S_IWUSR, show_in, set_in, 3, 2);

static SENSOR_DEVICE_ATTR_2(in4_input, S_IRUGO, show_in, NULL, 4, 0);
static SENSOR_DEVICE_ATTR_2(in4_min, S_IRUGO | S_IWUSR, show_in, set_in, 4, 1);
static SENSOR_DEVICE_ATTR_2(in4_max, S_IRUGO | S_IWUSR, show_in, set_in, 4, 2);

static SENSOR_DEVICE_ATTR_2(in5_input, S_IRUGO, show_in, NULL, 5, 0);
static SENSOR_DEVICE_ATTR_2(in5_min, S_IRUGO | S_IWUSR, show_in, set_in, 5, 1);
static SENSOR_DEVICE_ATTR_2(in5_max, S_IRUGO | S_IWUSR, show_in, set_in, 5, 2);

static SENSOR_DEVICE_ATTR_2(in6_input, S_IRUGO, show_in, NULL, 6, 0);
static SENSOR_DEVICE_ATTR_2(in6_min, S_IRUGO | S_IWUSR, show_in, set_in, 6, 1);
static SENSOR_DEVICE_ATTR_2(in6_max, S_IRUGO | S_IWUSR, show_in, set_in, 6, 2);

static SENSOR_DEVICE_ATTR_2(in7_input, S_IRUGO, show_in, NULL, 7, 0);
static SENSOR_DEVICE_ATTR_2(in7_min, S_IRUGO | S_IWUSR, show_in, set_in, 7, 1);
static SENSOR_DEVICE_ATTR_2(in7_max, S_IRUGO | S_IWUSR, show_in, set_in, 7, 2);

static SENSOR_DEVICE_ATTR_2(in8_input, S_IRUGO, show_in, NULL, 8, 0);
static SENSOR_DEVICE_ATTR_2(in9_input, S_IRUGO, show_in, NULL, 9, 0);
static SENSOR_DEVICE_ATTR_2(in10_input, S_IRUGO, show_in, NULL, 10, 0);
static SENSOR_DEVICE_ATTR_2(in11_input, S_IRUGO, show_in, NULL, 11, 0);
static SENSOR_DEVICE_ATTR_2(in12_input, S_IRUGO, show_in, NULL, 12, 0);

/* Up to 6 temperatures */
static ssize_t show_temp(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	int nr = sattr->nr;
	int index = sattr->index;
	struct it87_data *data = it87_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp[nr][index]));
}

static ssize_t set_temp(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	int nr = sattr->nr;
	int index = sattr->index;
	struct it87_data *data = dev_get_drvdata(dev);
	long val;
	u8 reg, regval;
	int err;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	switch (index) {
	default:
	case 1:
		reg = data->REG_TEMP_LOW[nr];
		break;
	case 2:
		reg = data->REG_TEMP_HIGH[nr];
		break;
	case 3:
		regval = data->read(data, IT87_REG_BEEP_ENABLE);
		if (!(regval & 0x80)) {
			regval |= 0x80;
			data->write(data, IT87_REG_BEEP_ENABLE, regval);
		}
		data->valid = false;
		reg = data->REG_TEMP_OFFSET[nr];
		break;
	}

	data->temp[nr][index] = TEMP_TO_REG(val);
	data->write(data, reg, data->temp[nr][index]);
	it87_unlock(data);
	return count;
}

static SENSOR_DEVICE_ATTR_2(temp1_input, S_IRUGO, show_temp, NULL, 0, 0);
static SENSOR_DEVICE_ATTR_2(temp1_min, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    0, 1);
static SENSOR_DEVICE_ATTR_2(temp1_max, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    0, 2);
static SENSOR_DEVICE_ATTR_2(temp1_offset, S_IRUGO | S_IWUSR, show_temp,
			    set_temp, 0, 3);
static SENSOR_DEVICE_ATTR_2(temp2_input, S_IRUGO, show_temp, NULL, 1, 0);
static SENSOR_DEVICE_ATTR_2(temp2_min, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    1, 1);
static SENSOR_DEVICE_ATTR_2(temp2_max, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    1, 2);
static SENSOR_DEVICE_ATTR_2(temp2_offset, S_IRUGO | S_IWUSR, show_temp,
			    set_temp, 1, 3);
static SENSOR_DEVICE_ATTR_2(temp3_input, S_IRUGO, show_temp, NULL, 2, 0);
static SENSOR_DEVICE_ATTR_2(temp3_min, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    2, 1);
static SENSOR_DEVICE_ATTR_2(temp3_max, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    2, 2);
static SENSOR_DEVICE_ATTR_2(temp3_offset, S_IRUGO | S_IWUSR, show_temp,
			    set_temp, 2, 3);
static SENSOR_DEVICE_ATTR_2(temp4_input, S_IRUGO, show_temp, NULL, 3, 0);
static SENSOR_DEVICE_ATTR_2(temp4_min, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    3, 1);
static SENSOR_DEVICE_ATTR_2(temp4_max, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    3, 2);
static SENSOR_DEVICE_ATTR_2(temp4_offset, S_IRUGO | S_IWUSR, show_temp,
			    set_temp, 3, 3);
static SENSOR_DEVICE_ATTR_2(temp5_input, S_IRUGO, show_temp, NULL, 4, 0);
static SENSOR_DEVICE_ATTR_2(temp5_min, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    4, 1);
static SENSOR_DEVICE_ATTR_2(temp5_max, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    4, 2);
static SENSOR_DEVICE_ATTR_2(temp5_offset, S_IRUGO | S_IWUSR, show_temp,
			    set_temp, 4, 3);
static SENSOR_DEVICE_ATTR_2(temp6_input, S_IRUGO, show_temp, NULL, 5, 0);
static SENSOR_DEVICE_ATTR_2(temp6_min, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    5, 1);
static SENSOR_DEVICE_ATTR_2(temp6_max, S_IRUGO | S_IWUSR, show_temp, set_temp,
			    5, 2);
static SENSOR_DEVICE_ATTR_2(temp6_offset, S_IRUGO | S_IWUSR, show_temp,
			    set_temp, 5, 3);

static const u8 temp_types_8686[NUM_TEMP][9] = {
	{ 0, 8, 8, 8, 8, 8, 8, 8, 7 },
	{ 0, 6, 8, 8, 6, 0, 0, 0, 7 },
	{ 0, 6, 5, 8, 6, 0, 0, 0, 7 },
	{ 4, 8, 8, 8, 8, 8, 8, 8, 7 },
	{ 4, 6, 8, 8, 6, 0, 0, 0, 7 },
	{ 4, 6, 5, 8, 6, 0, 0, 0, 7 },
};

static int get_temp_type(struct it87_data *data, int index)
{
	u8 reg, extra;
	int ttype, type = 0;

	if (has_bank_sel(data)) {
		u8 src1, src2;

		src1 = (data->temp_src[index / 2] >> ((index % 2) * 4)) & 0x0f;

		switch (data->type) {
		case it8686:
		case it8688:
		case it8689:
			if (src1 < 9)
				type = temp_types_8686[index][src1];
			break;
		case it8625:
			if (index < 3)
				break;
			fallthrough; /* special Linux kernel function */
		case it8655:
		case it8665:
			if (src1 < 3) {
				index = src1;
				break;
			}
			src2 = data->temp_src[3];
			switch (src1) {
			case 3:
				type = (src2 & BIT(index)) ? 6 : 5;
				break;
			case 4 ... 8:
				type = (src2 & BIT(index)) ? 4 : 6;
				break;
			case 9:
				type = (src2 & BIT(index)) ? 5 : 0;
				break;
			default:
				break;
			}
			return type;
		default:
			return 0;
		}
	}
	if (type)
		return type;

	/* Dectect PECI vs. AMDTSI */
	ttype = 6;
	if ((has_temp_peci(data, index)) || data->type == it8721 ||
			data->type == it8720) {
		extra = data->read(data, IT87_REG_IFSEL);
		if ((extra & 0x70) == 0x40)
			ttype = 5;
	}

	reg = data->read(data, IT87_REG_TEMP_ENABLE);

	/* Per chip special detection */
	switch (data->type) {
	case it8622:
		if (!(reg & 0xc0) && index == 3)
			type = ttype;
		break;
	default:
		break;
	}

	if (type || index >= 3)
		return type;

	extra = data->read(data, IT87_REG_TEMP_EXTRA);

	if ((has_temp_peci(data, index) && (reg >> 6 == index + 1)) ||
			(has_temp_old_peci(data, index) && (extra & 0x80)))
		type = ttype;	/* Intel PECI or AMDTSI */
	else if (reg & BIT(index))
		type = 3;	/* thermal diode */
	else if (reg & BIT(index + 3))
		type = 4;	/* thermistor */

	return type;
}

static ssize_t show_temp_type(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = it87_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", get_temp_type(data, sensor_attr->index));
}

static ssize_t set_temp_type(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;

	struct it87_data *data = dev_get_drvdata(dev);
	long val;
	u8 reg, extra;
	int err;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	reg = data->read(data, IT87_REG_TEMP_ENABLE);
	reg &= ~(1 << nr);
	reg &= ~(8 << nr);
	if (has_temp_peci(data, nr) && (reg >> 6 == nr + 1 || val == 6))
		reg &= 0x3f;
	extra = data->read(data, IT87_REG_TEMP_EXTRA);
	if (has_temp_old_peci(data, nr) && ((extra & 0x80) || val == 6))
		extra &= 0x7f;
	if (val == 2) {	/* backwards compatibility */
		dev_warn(dev,
			 "Sensor type 2 is deprecated, please use 4 instead\n");
		val = 4;
	}
	/* 3 = thermal diode; 4 = thermistor; 6 = Intel PECI; 0 = disabled */
	if (val == 3)
		reg |= 1 << nr;
	else if (val == 4)
		reg |= 8 << nr;
	else if (has_temp_peci(data, nr) && val == 6)
		reg |= (nr + 1) << 6;
	else if (has_temp_old_peci(data, nr) && val == 6)
		extra |= 0x80;
	else if (val != 0) {
		count = -EINVAL;
		goto unlock;
	}

	data->sensor = reg;
	data->extra = extra;
	data->write(data, IT87_REG_TEMP_ENABLE, data->sensor);
	if (has_temp_old_peci(data, nr))
		data->write(data, IT87_REG_TEMP_EXTRA, data->extra);
	data->valid = false;	/* Force cache refresh */
unlock:
	it87_unlock(data);
	return count;
}

static SENSOR_DEVICE_ATTR(temp1_type, S_IRUGO | S_IWUSR, show_temp_type,
			  set_temp_type, 0);
static SENSOR_DEVICE_ATTR(temp2_type, S_IRUGO | S_IWUSR, show_temp_type,
			  set_temp_type, 1);
static SENSOR_DEVICE_ATTR(temp3_type, S_IRUGO | S_IWUSR, show_temp_type,
			  set_temp_type, 2);
static SENSOR_DEVICE_ATTR(temp4_type, S_IRUGO | S_IWUSR, show_temp_type,
			  set_temp_type, 3);
static SENSOR_DEVICE_ATTR(temp5_type, S_IRUGO | S_IWUSR, show_temp_type,
			  set_temp_type, 4);
static SENSOR_DEVICE_ATTR(temp6_type, S_IRUGO | S_IWUSR, show_temp_type,
			  set_temp_type, 5);

/* 6 Fans */

static int pwm_mode(const struct it87_data *data, int nr)
{
	if (has_fanctl_onoff(data) && nr < 3 &&
	    !(data->fan_main_ctrl & BIT(nr)))
		return 0;			/* Full speed */
	if (data->pwm_ctrl[nr] & 0x80)
		return 2;			/* Automatic mode */
	if ((!has_fanctl_onoff(data) || nr >= 3) &&
	    data->pwm_duty[nr] == pwm_to_reg(data, 0xff))
		return 0;			/* Full speed */

	return 1;				/* Manual mode */
}

static ssize_t show_fan(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	int nr = sattr->nr;
	int index = sattr->index;
	int speed;
	struct it87_data *data = it87_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	speed = has_16bit_fans(data) ?
		FAN16_FROM_REG(data->fan[nr][index]) :
		FAN_FROM_REG(data->fan[nr][index],
			     DIV_FROM_REG(data->fan_div[nr]));
	return sprintf(buf, "%d\n", speed);
}

static ssize_t show_fan_div(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = it87_update_device(dev);
	int nr = sensor_attr->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%lu\n", DIV_FROM_REG(data->fan_div[nr]));
}

static ssize_t show_pwm_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = it87_update_device(dev);
	int nr = sensor_attr->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", pwm_mode(data, nr));
}

static ssize_t show_pwm(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = it87_update_device(dev);
	int nr = sensor_attr->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n",
		       pwm_from_reg(data, data->pwm_duty[nr]));
}

static ssize_t show_pwm_freq(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = it87_update_device(dev);
	int nr = sensor_attr->index;
	unsigned int freq;
	int index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	if (has_pwm_freq2(data) && nr == 1)
		index = (data->extra >> 4) & 0x07;
	else
		index = (data->fan_ctl >> 4) & 0x07;

	freq = pwm_freq[index] / (has_newer_autopwm(data) ? 256 : 128);

	return sprintf(buf, "%u\n", freq);
}

static ssize_t set_fan(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	int nr = sattr->nr;
	int index = sattr->index;

	struct it87_data *data = dev_get_drvdata(dev);
	long val;
	int err;
	u8 reg;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	if (has_16bit_fans(data)) {
		data->fan[nr][index] = FAN16_TO_REG(val);
		data->write(data, data->REG_FAN_MIN[nr],
			    data->fan[nr][index] & 0xff);
		data->write(data, data->REG_FANX_MIN[nr],
			    data->fan[nr][index] >> 8);
	} else {
		reg = data->read(data, IT87_REG_FAN_DIV);
		switch (nr) {
		case 0:
			data->fan_div[nr] = reg & 0x07;
			break;
		case 1:
			data->fan_div[nr] = (reg >> 3) & 0x07;
			break;
		case 2:
			data->fan_div[nr] = (reg & 0x40) ? 3 : 1;
			break;
		}
		data->fan[nr][index] =
		  FAN_TO_REG(val, DIV_FROM_REG(data->fan_div[nr]));
		data->write(data, data->REG_FAN_MIN[nr], data->fan[nr][index]);
	}
	it87_unlock(data);
	return count;
}

static ssize_t set_fan_div(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = dev_get_drvdata(dev);
	int nr = sensor_attr->index;
	unsigned long val;
	int min, err;
	u8 old;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	old = data->read(data, IT87_REG_FAN_DIV);

	/* Save fan min limit */
	min = FAN_FROM_REG(data->fan[nr][1], DIV_FROM_REG(data->fan_div[nr]));

	switch (nr) {
	case 0:
	case 1:
		data->fan_div[nr] = DIV_TO_REG(val);
		break;
	case 2:
		if (val < 8)
			data->fan_div[nr] = 1;
		else
			data->fan_div[nr] = 3;
	}
	val = old & 0x80;
	val |= (data->fan_div[0] & 0x07);
	val |= (data->fan_div[1] & 0x07) << 3;
	if (data->fan_div[2] == 3)
		val |= 0x1 << 6;
	data->write(data, IT87_REG_FAN_DIV, val);

	/* Restore fan min limit */
	data->fan[nr][1] = FAN_TO_REG(min, DIV_FROM_REG(data->fan_div[nr]));
	data->write(data, data->REG_FAN_MIN[nr], data->fan[nr][1]);
	it87_unlock(data);
	return count;
}

/* Returns 0 if OK, -EINVAL otherwise */
static int check_trip_points(struct device *dev, int nr)
{
	const struct it87_data *data = dev_get_drvdata(dev);
	int i, err = 0;

	if (has_old_autopwm(data)) {
		for (i = 0; i < 3; i++) {
			if (data->auto_temp[nr][i] > data->auto_temp[nr][i + 1])
				err = -EINVAL;
		}
		for (i = 0; i < 2; i++) {
			if (data->auto_pwm[nr][i] > data->auto_pwm[nr][i + 1])
				err = -EINVAL;
		}
	} else if (has_newer_autopwm(data)) {
		for (i = 1; i < 3; i++) {
			if (data->auto_temp[nr][i] > data->auto_temp[nr][i + 1])
				err = -EINVAL;
		}
	}

	if (err) {
		dev_err(dev,
			"Inconsistent trip points, not switching to automatic mode\n");
		dev_err(dev, "Adjust the trip points and try again\n");
	}
	return err;
}

/* SmartFan global control in H2RAM:
 *   0x00 = manual / non-automatic (any channel non-auto)
 *   0x01 = automatic (all channels automatic)
 *
 * Only call this when data->mmio_h2ram or data->ecio_h2ram is true.
 * For newer H2RAM based controllers with separate SmartFan toggle'
 */
static void it87_update_smartfan_global(struct it87_data *data)
{
    bool all_auto = true;
    int  i;
    u8   val;
    int  cur;

    for (i=0; i<NUM_AUTO_PWM; i++) {
        /* Skip PWM channels that are not actually used/enabled */
        if (!(data->has_pwm & BIT(i)))
            continue;

        /* pwm_mode(): 0 = full, 1 = manual, 2 = automatic */
        if (pwm_mode(data, i) != 2) {
            all_auto = false;
            break;
        }
    }

    val = all_auto ? 0x01 : 0x00;
    cur = data->read(data,IT87_SMARTFAN_ENABLE);
    if (cur >= 0 && (u8)cur == val)
        return;
    /* 0x947 is the SmartFan global control byte in H2RAM */
    data->write(data, IT87_SMARTFAN_ENABLE, val);
}


static ssize_t set_pwm_enable(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = dev_get_drvdata(dev);
	int nr = sensor_attr->index;
	long val;
	int err;

	if (kstrtol(buf, 10, &val) < 0 || val < 0 || val > 2)
		return -EINVAL;

	/* Check trip points before switching to automatic mode */
	if (val == 2) {
		if (check_trip_points(dev, nr) < 0)
			return -EINVAL;
	}

	err = it87_lock(data);
	if (err)
		return err;

	it87_update_pwm_ctrl(data, nr);

	if (val == 0) {
		if (nr < 3 && has_fanctl_onoff(data)) {
			int tmp;
			/* make sure the fan is on when in on/off mode */
			tmp = data->read(data, IT87_REG_FAN_CTL);
			data->write(data, IT87_REG_FAN_CTL, tmp | BIT(nr));
			/* set on/off mode */
			data->fan_main_ctrl &= ~BIT(nr);
			data->write(data, IT87_REG_FAN_MAIN_CTRL,
				    data->fan_main_ctrl);
		} else {
			u8 ctrl;

			/* No on/off mode, set maximum pwm value */
			data->pwm_duty[nr] = pwm_to_reg(data, 0xff);
			data->write(data, IT87_REG_PWM_DUTY[nr],
				    data->pwm_duty[nr]);
			/* and set manual mode */
			if (has_newer_autopwm(data)) {
				ctrl = temp_map_to_reg(data, nr,
						       data->pwm_temp_map[nr]);
				ctrl &= 0x7f;
			} else {
				ctrl = data->pwm_duty[nr];
			}
			data->pwm_ctrl[nr] = ctrl;
			data->write(data, data->REG_PWM[nr], ctrl);
		}
	} else {
		u8 ctrl;

		if (has_newer_autopwm(data)) {
			ctrl = temp_map_to_reg(data, nr,
					       data->pwm_temp_map[nr]);
			if (val == 1)
				ctrl &= 0x7f;
			else
				ctrl |= 0x80;
		} else {
			ctrl = (val == 1 ? data->pwm_duty[nr] : 0x80);
		}
		data->pwm_ctrl[nr] = ctrl;
		data->write(data, data->REG_PWM[nr], ctrl);

		if (has_fanctl_onoff(data) && nr < 3) {
			/* set SmartGuardian mode */
			data->fan_main_ctrl |= BIT(nr);
			data->write(data, IT87_REG_FAN_MAIN_CTRL,
				    data->fan_main_ctrl);
		}
	}

	 /* If this device uses H2RAM/ECIO SmartFan, sync the global bit at 0x947 first */
    if (data->mmio_h2ram || data->ecio_h2ram)
    {
        it87_update_smartfan_global(data);
    }

	it87_unlock(data);
	return count;
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = dev_get_drvdata(dev);
	int nr = sensor_attr->index;
	long val;
	int err;

	if (kstrtol(buf, 10, &val) < 0 || val < 0 || val > 255)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	it87_update_pwm_ctrl(data, nr);
	if (has_newer_autopwm(data)) {
		/*
		 * If we are in automatic mode, the PWM duty cycle register
		 * is read-only so we can't write the value.
		 */
		if (data->pwm_ctrl[nr] & 0x80) {
			count = -EBUSY;
			goto unlock;
		}
		data->pwm_duty[nr] = pwm_to_reg(data, val);
		data->write(data, IT87_REG_PWM_DUTY[nr],
			    data->pwm_duty[nr]);
	} else {
		data->pwm_duty[nr] = pwm_to_reg(data, val);
		/*
		 * If we are in manual mode, write the duty cycle immediately;
		 * otherwise, just store it for later use.
		 */
		if (!(data->pwm_ctrl[nr] & 0x80)) {
			data->pwm_ctrl[nr] = data->pwm_duty[nr];
			data->write(data, data->REG_PWM[nr],
				    data->pwm_ctrl[nr]);
		}
	}
unlock:
	it87_unlock(data);
	return count;
}

static ssize_t set_pwm_freq(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = dev_get_drvdata(dev);
	int nr = sensor_attr->index;
	unsigned long val;
	int err;
	int i;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	val = clamp_val(val, 0, 1000000);
	val *= has_newer_autopwm(data) ? 256 : 128;

	/* Search for the nearest available frequency */
	for (i = 0; i < ARRAY_SIZE(pwm_freq) - 1; i++) {
		if (val > (pwm_freq[i] + pwm_freq[i + 1]) / 2)
			break;
	}

	err = it87_lock(data);
	if (err)
		return err;

	if (nr == 0) {
		data->fan_ctl = data->read(data, IT87_REG_FAN_CTL) & 0x8f;
		data->fan_ctl |= i << 4;
		data->write(data, IT87_REG_FAN_CTL, data->fan_ctl);
	} else {
		data->extra = data->read(data, IT87_REG_TEMP_EXTRA) & 0x8f;
		data->extra |= i << 4;
		data->write(data, IT87_REG_TEMP_EXTRA, data->extra);
	}
	it87_unlock(data);
	return count;
}

static ssize_t show_pwm_temp_map(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = it87_update_device(dev);
	int nr = sensor_attr->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", data->pwm_temp_map[nr] + 1);
}

static ssize_t set_pwm_temp_map(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct it87_data *data = dev_get_drvdata(dev);
	int nr = sensor_attr->index;
	unsigned long val;
	int err;
	u8 map;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	if (!val || val > data->pwm_num_temp_map)
		return -EINVAL;

	map = val - 1;

	err = it87_lock(data);
	if (err)
		return err;

	it87_update_pwm_ctrl(data, nr);
	data->pwm_temp_map[nr] = map;
	/*
	 * If we are in automatic mode, write the temp mapping immediately;
	 * otherwise, just store it for later use.
	 */
	if (data->pwm_ctrl[nr] & 0x80) {
		data->pwm_ctrl[nr] = temp_map_to_reg(data, nr, map);
		data->write(data, data->REG_PWM[nr], data->pwm_ctrl[nr]);
	}
	it87_unlock(data);
	return count;
}

static ssize_t show_auto_pwm(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct it87_data *data = it87_update_device(dev);
	struct sensor_device_attribute_2 *sensor_attr =
			to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int point = sensor_attr->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n",
		       pwm_from_reg(data, data->auto_pwm[nr][point]));
}

static ssize_t set_auto_pwm(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct it87_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr =
			to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int point = sensor_attr->index;
	int regaddr;
	long val;
	int err;

	if (kstrtol(buf, 10, &val) < 0 || val < 0 || val > 255)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	data->auto_pwm[nr][point] = pwm_to_reg(data, val);
	if (has_newer_autopwm(data))
		regaddr = IT87_REG_AUTO_TEMP(nr, 3);
	else
		regaddr = IT87_REG_AUTO_PWM(nr, point);
	data->write(data, regaddr, data->auto_pwm[nr][point]);
	it87_unlock(data);
	return count;
}

static ssize_t show_auto_pwm_slope(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct it87_data *data = it87_update_device(dev);
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%d\n", data->auto_pwm[nr][1] & 0x7f);
}

static ssize_t set_auto_pwm_slope(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct it87_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	unsigned long val;
	int err;

	if (kstrtoul(buf, 10, &val) < 0 || val > 127)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	data->auto_pwm[nr][1] = (data->auto_pwm[nr][1] & 0x80) | val;
	data->write(data, IT87_REG_AUTO_TEMP(nr, 4), data->auto_pwm[nr][1]);
	it87_unlock(data);
	return count;
}

static ssize_t show_auto_temp(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct it87_data *data = it87_update_device(dev);
	struct sensor_device_attribute_2 *sensor_attr =
			to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int point = sensor_attr->index;
	int reg;

	if (IS_ERR(data))
		return PTR_ERR(data);

	if (has_old_autopwm(data) || point)
		reg = data->auto_temp[nr][point];
	else
		reg = data->auto_temp[nr][1] - (data->auto_temp[nr][0] & 0x1f);

	return sprintf(buf, "%d\n", TEMP_FROM_REG(reg));
}

static ssize_t set_auto_temp(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct it87_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2 *sensor_attr =
			to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int point = sensor_attr->index;
	long val;
	int reg;
	int err;

	if (kstrtol(buf, 10, &val) < 0 || val < -128000 || val > 127000)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	if (has_newer_autopwm(data) && !point) {
		reg = data->auto_temp[nr][1] - TEMP_TO_REG(val);
		reg = clamp_val(reg, 0, 0x1f) | (data->auto_temp[nr][0] & 0xe0);
		data->auto_temp[nr][0] = reg;
		data->write(data, IT87_REG_AUTO_TEMP(nr, 5), reg);
	} else {
		reg = TEMP_TO_REG(val);
		data->auto_temp[nr][point] = reg;
		if (has_newer_autopwm(data))
			point--;
		data->write(data, IT87_REG_AUTO_TEMP(nr, point), reg);
	}
	it87_unlock(data);
	return count;
}

static SENSOR_DEVICE_ATTR_2(fan1_input, S_IRUGO, show_fan, NULL, 0, 0);
static SENSOR_DEVICE_ATTR_2(fan1_min, S_IRUGO | S_IWUSR, show_fan, set_fan,
			    0, 1);
static SENSOR_DEVICE_ATTR(fan1_div, S_IRUGO | S_IWUSR, show_fan_div,
			  set_fan_div, 0);

static SENSOR_DEVICE_ATTR_2(fan2_input, S_IRUGO, show_fan, NULL, 1, 0);
static SENSOR_DEVICE_ATTR_2(fan2_min, S_IRUGO | S_IWUSR, show_fan, set_fan,
			    1, 1);
static SENSOR_DEVICE_ATTR(fan2_div, S_IRUGO | S_IWUSR, show_fan_div,
			  set_fan_div, 1);

static SENSOR_DEVICE_ATTR_2(fan3_input, S_IRUGO, show_fan, NULL, 2, 0);
static SENSOR_DEVICE_ATTR_2(fan3_min, S_IRUGO | S_IWUSR, show_fan, set_fan,
			    2, 1);
static SENSOR_DEVICE_ATTR(fan3_div, S_IRUGO | S_IWUSR, show_fan_div,
			  set_fan_div, 2);

static SENSOR_DEVICE_ATTR_2(fan4_input, S_IRUGO, show_fan, NULL, 3, 0);
static SENSOR_DEVICE_ATTR_2(fan4_min, S_IRUGO | S_IWUSR, show_fan, set_fan,
			    3, 1);

static SENSOR_DEVICE_ATTR_2(fan5_input, S_IRUGO, show_fan, NULL, 4, 0);
static SENSOR_DEVICE_ATTR_2(fan5_min, S_IRUGO | S_IWUSR, show_fan, set_fan,
			    4, 1);

static SENSOR_DEVICE_ATTR_2(fan6_input, S_IRUGO, show_fan, NULL, 5, 0);
static SENSOR_DEVICE_ATTR_2(fan6_min, S_IRUGO | S_IWUSR, show_fan, set_fan,
			    5, 1);

static SENSOR_DEVICE_ATTR(pwm1_enable, S_IRUGO | S_IWUSR,
			  show_pwm_enable, set_pwm_enable, 0);
static SENSOR_DEVICE_ATTR(pwm1, S_IRUGO | S_IWUSR, show_pwm, set_pwm, 0);
static SENSOR_DEVICE_ATTR(pwm1_freq, S_IRUGO | S_IWUSR, show_pwm_freq,
			  set_pwm_freq, 0);
static SENSOR_DEVICE_ATTR(pwm1_auto_channels_temp, S_IRUGO,
			  show_pwm_temp_map, set_pwm_temp_map, 0);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point1_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 0, 0);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point2_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 0, 1);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point3_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 0, 2);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point4_pwm, S_IRUGO,
			    show_auto_pwm, NULL, 0, 3);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point1_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 0, 1);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 0, 0);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point2_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 0, 2);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point3_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 0, 3);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_point4_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 0, 4);
static SENSOR_DEVICE_ATTR_2(pwm1_auto_start, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 0, 0);
static SENSOR_DEVICE_ATTR(pwm1_auto_slope, S_IRUGO | S_IWUSR,
			  show_auto_pwm_slope, set_auto_pwm_slope, 0);

static SENSOR_DEVICE_ATTR(pwm2_enable, S_IRUGO | S_IWUSR,
			  show_pwm_enable, set_pwm_enable, 1);
static SENSOR_DEVICE_ATTR(pwm2, S_IRUGO | S_IWUSR, show_pwm, set_pwm, 1);
static SENSOR_DEVICE_ATTR(pwm2_freq, S_IRUGO, show_pwm_freq, set_pwm_freq, 1);
static SENSOR_DEVICE_ATTR(pwm2_auto_channels_temp, S_IRUGO,
			  show_pwm_temp_map, set_pwm_temp_map, 1);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point1_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 1, 0);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point2_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 1, 1);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point3_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 1, 2);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point4_pwm, S_IRUGO,
			    show_auto_pwm, NULL, 1, 3);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point1_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 1, 1);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 1, 0);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point2_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 1, 2);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point3_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 1, 3);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_point4_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 1, 4);
static SENSOR_DEVICE_ATTR_2(pwm2_auto_start, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 1, 0);
static SENSOR_DEVICE_ATTR(pwm2_auto_slope, S_IRUGO | S_IWUSR,
			  show_auto_pwm_slope, set_auto_pwm_slope, 1);

static SENSOR_DEVICE_ATTR(pwm3_enable, S_IRUGO | S_IWUSR,
			  show_pwm_enable, set_pwm_enable, 2);
static SENSOR_DEVICE_ATTR(pwm3, S_IRUGO | S_IWUSR, show_pwm, set_pwm, 2);
static SENSOR_DEVICE_ATTR(pwm3_freq, S_IRUGO, show_pwm_freq, NULL, 2);
static SENSOR_DEVICE_ATTR(pwm3_auto_channels_temp, S_IRUGO,
			  show_pwm_temp_map, set_pwm_temp_map, 2);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point1_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 2, 0);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point2_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 2, 1);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point3_pwm, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 2, 2);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point4_pwm, S_IRUGO,
			    show_auto_pwm, NULL, 2, 3);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point1_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 1);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 0);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point2_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 2);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point3_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 3);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_point4_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 4);
static SENSOR_DEVICE_ATTR_2(pwm3_auto_start, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 2, 0);
static SENSOR_DEVICE_ATTR(pwm3_auto_slope, S_IRUGO | S_IWUSR,
			  show_auto_pwm_slope, set_auto_pwm_slope, 2);

static SENSOR_DEVICE_ATTR(pwm4_enable, S_IRUGO | S_IWUSR,
			  show_pwm_enable, set_pwm_enable, 3);
static SENSOR_DEVICE_ATTR(pwm4, S_IRUGO | S_IWUSR, show_pwm, set_pwm, 3);
static SENSOR_DEVICE_ATTR(pwm4_freq, S_IRUGO, show_pwm_freq, NULL, 3);
static SENSOR_DEVICE_ATTR(pwm4_auto_channels_temp, S_IRUGO,
			  show_pwm_temp_map, set_pwm_temp_map, 3);
static SENSOR_DEVICE_ATTR_2(pwm4_auto_point1_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 1);
static SENSOR_DEVICE_ATTR_2(pwm4_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 0);
static SENSOR_DEVICE_ATTR_2(pwm4_auto_point2_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 2);
static SENSOR_DEVICE_ATTR_2(pwm4_auto_point3_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 3);
static SENSOR_DEVICE_ATTR_2(pwm4_auto_start, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 3, 0);
static SENSOR_DEVICE_ATTR(pwm4_auto_slope, S_IRUGO | S_IWUSR,
			  show_auto_pwm_slope, set_auto_pwm_slope, 3);

static SENSOR_DEVICE_ATTR(pwm5_enable, S_IRUGO | S_IWUSR,
			  show_pwm_enable, set_pwm_enable, 4);
static SENSOR_DEVICE_ATTR(pwm5, S_IRUGO | S_IWUSR, show_pwm, set_pwm, 4);
static SENSOR_DEVICE_ATTR(pwm5_freq, S_IRUGO, show_pwm_freq, NULL, 4);
static SENSOR_DEVICE_ATTR(pwm5_auto_channels_temp, S_IRUGO,
			  show_pwm_temp_map, set_pwm_temp_map, 4);
static SENSOR_DEVICE_ATTR_2(pwm5_auto_point1_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 1);
static SENSOR_DEVICE_ATTR_2(pwm5_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 0);
static SENSOR_DEVICE_ATTR_2(pwm5_auto_point2_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 2);
static SENSOR_DEVICE_ATTR_2(pwm5_auto_point3_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 3);
static SENSOR_DEVICE_ATTR_2(pwm5_auto_start, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 4, 0);
static SENSOR_DEVICE_ATTR(pwm5_auto_slope, S_IRUGO | S_IWUSR,
			  show_auto_pwm_slope, set_auto_pwm_slope, 4);

static SENSOR_DEVICE_ATTR(pwm6_enable, S_IRUGO | S_IWUSR,
			  show_pwm_enable, set_pwm_enable, 5);
static SENSOR_DEVICE_ATTR(pwm6, S_IRUGO | S_IWUSR, show_pwm, set_pwm, 5);
static SENSOR_DEVICE_ATTR(pwm6_freq, S_IRUGO, show_pwm_freq, NULL, 5);
static SENSOR_DEVICE_ATTR(pwm6_auto_channels_temp, S_IRUGO,
			  show_pwm_temp_map, set_pwm_temp_map, 5);
static SENSOR_DEVICE_ATTR_2(pwm6_auto_point1_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 1);
static SENSOR_DEVICE_ATTR_2(pwm6_auto_point1_temp_hyst, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 0);
static SENSOR_DEVICE_ATTR_2(pwm6_auto_point2_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 2);
static SENSOR_DEVICE_ATTR_2(pwm6_auto_point3_temp, S_IRUGO | S_IWUSR,
			    show_auto_temp, set_auto_temp, 2, 3);
static SENSOR_DEVICE_ATTR_2(pwm6_auto_start, S_IRUGO | S_IWUSR,
			    show_auto_pwm, set_auto_pwm, 5, 0);
static SENSOR_DEVICE_ATTR(pwm6_auto_slope, S_IRUGO | S_IWUSR,
			  show_auto_pwm_slope, set_auto_pwm_slope, 5);

/* Alarms */
static ssize_t show_alarms(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct it87_data *data = it87_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%u\n", data->alarms);
}
static DEVICE_ATTR(alarms, S_IRUGO, show_alarms, NULL);

static ssize_t show_alarm(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct it87_data *data = it87_update_device(dev);
	int bitnr = to_sensor_dev_attr(attr)->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%u\n", (data->alarms >> bitnr) & 1);
}

static ssize_t clear_intrusion(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct it87_data *data = dev_get_drvdata(dev);
	int err, config;
	long val;

	if (kstrtol(buf, 10, &val) < 0 || val != 0)
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	config = data->read(data, IT87_REG_CONFIG);
	config |= BIT(5);
	data->write(data, IT87_REG_CONFIG, config);
	/* Invalidate cache to force re-read */
	data->valid = false;
	it87_unlock(data);
	return count;
}

static SENSOR_DEVICE_ATTR(in0_alarm, S_IRUGO, show_alarm, NULL, 8);
static SENSOR_DEVICE_ATTR(in1_alarm, S_IRUGO, show_alarm, NULL, 9);
static SENSOR_DEVICE_ATTR(in2_alarm, S_IRUGO, show_alarm, NULL, 10);
static SENSOR_DEVICE_ATTR(in3_alarm, S_IRUGO, show_alarm, NULL, 11);
static SENSOR_DEVICE_ATTR(in4_alarm, S_IRUGO, show_alarm, NULL, 12);
static SENSOR_DEVICE_ATTR(in5_alarm, S_IRUGO, show_alarm, NULL, 13);
static SENSOR_DEVICE_ATTR(in6_alarm, S_IRUGO, show_alarm, NULL, 14);
static SENSOR_DEVICE_ATTR(in7_alarm, S_IRUGO, show_alarm, NULL, 15);
static SENSOR_DEVICE_ATTR(fan1_alarm, S_IRUGO, show_alarm, NULL, 0);
static SENSOR_DEVICE_ATTR(fan2_alarm, S_IRUGO, show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(fan3_alarm, S_IRUGO, show_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(fan4_alarm, S_IRUGO, show_alarm, NULL, 3);
static SENSOR_DEVICE_ATTR(fan5_alarm, S_IRUGO, show_alarm, NULL, 6);
static SENSOR_DEVICE_ATTR(fan6_alarm, S_IRUGO, show_alarm, NULL, 7);
static SENSOR_DEVICE_ATTR(temp1_alarm, S_IRUGO, show_alarm, NULL, 16);
static SENSOR_DEVICE_ATTR(temp2_alarm, S_IRUGO, show_alarm, NULL, 17);
static SENSOR_DEVICE_ATTR(temp3_alarm, S_IRUGO, show_alarm, NULL, 18);
static SENSOR_DEVICE_ATTR(temp4_alarm, S_IRUGO, show_alarm, NULL, 19);
static SENSOR_DEVICE_ATTR(temp5_alarm, S_IRUGO, show_alarm, NULL, 20);
static SENSOR_DEVICE_ATTR(temp6_alarm, S_IRUGO, show_alarm, NULL, 21);
static SENSOR_DEVICE_ATTR(intrusion0_alarm, S_IRUGO | S_IWUSR,
			  show_alarm, clear_intrusion, 4);

static ssize_t show_beep(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct it87_data *data = it87_update_device(dev);
	int bitnr = to_sensor_dev_attr(attr)->index;

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%u\n", (data->beeps >> bitnr) & 1);
}

static ssize_t set_beep(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int bitnr = to_sensor_dev_attr(attr)->index;
	struct it87_data *data = dev_get_drvdata(dev);
	long val;
	int err;

	if (kstrtol(buf, 10, &val) < 0 || (val != 0 && val != 1))
		return -EINVAL;

	err = it87_lock(data);
	if (err)
		return err;

	data->beeps = data->read(data, IT87_REG_BEEP_ENABLE);
	if (val)
		data->beeps |= BIT(bitnr);
	else
		data->beeps &= ~BIT(bitnr);
	data->write(data, IT87_REG_BEEP_ENABLE, data->beeps);
	it87_unlock(data);
	return count;
}

static SENSOR_DEVICE_ATTR(in0_beep, S_IRUGO | S_IWUSR,
			  show_beep, set_beep, 1);
static SENSOR_DEVICE_ATTR(in1_beep, S_IRUGO, show_beep, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_beep, S_IRUGO, show_beep, NULL, 1);
static SENSOR_DEVICE_ATTR(in3_beep, S_IRUGO, show_beep, NULL, 1);
static SENSOR_DEVICE_ATTR(in4_beep, S_IRUGO, show_beep, NULL, 1);
static SENSOR_DEVICE_ATTR(in5_beep, S_IRUGO, show_beep, NULL, 1);
static SENSOR_DEVICE_ATTR(in6_beep, S_IRUGO, show_beep, NULL, 1);
static SENSOR_DEVICE_ATTR(in7_beep, S_IRUGO, show_beep, NULL, 1);
/* fanX_beep writability is set later */
static SENSOR_DEVICE_ATTR(fan1_beep, S_IRUGO, show_beep, set_beep, 0);
static SENSOR_DEVICE_ATTR(fan2_beep, S_IRUGO, show_beep, set_beep, 0);
static SENSOR_DEVICE_ATTR(fan3_beep, S_IRUGO, show_beep, set_beep, 0);
static SENSOR_DEVICE_ATTR(fan4_beep, S_IRUGO, show_beep, set_beep, 0);
static SENSOR_DEVICE_ATTR(fan5_beep, S_IRUGO, show_beep, set_beep, 0);
static SENSOR_DEVICE_ATTR(fan6_beep, S_IRUGO, show_beep, set_beep, 0);
static SENSOR_DEVICE_ATTR(temp1_beep, S_IRUGO | S_IWUSR,
			  show_beep, set_beep, 2);
static SENSOR_DEVICE_ATTR(temp2_beep, S_IRUGO, show_beep, NULL, 2);
static SENSOR_DEVICE_ATTR(temp3_beep, S_IRUGO, show_beep, NULL, 2);
static SENSOR_DEVICE_ATTR(temp4_beep, S_IRUGO, show_beep, NULL, 2);
static SENSOR_DEVICE_ATTR(temp5_beep, S_IRUGO, show_beep, NULL, 2);
static SENSOR_DEVICE_ATTR(temp6_beep, S_IRUGO, show_beep, NULL, 2);

static ssize_t show_vrm_reg(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct it87_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->vrm);
}

static ssize_t store_vrm_reg(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct it87_data *data = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	data->vrm = val;

	return count;
}
static DEVICE_ATTR(vrm, S_IRUGO | S_IWUSR, show_vrm_reg, store_vrm_reg);

static ssize_t show_vid_reg(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct it87_data *data = it87_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%ld\n", (long)vid_from_reg(data->vid, data->vrm));
}
static DEVICE_ATTR(cpu0_vid, S_IRUGO, show_vid_reg, NULL);

static ssize_t show_label(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	static const char * const labels[] = {
		"+5V",
		"5VSB",
		"Vbat",
		"AVCC",
	};
	static const char * const labels_it8721[] = {
		"+3.3V",
		"3VSB",
		"Vbat",
		"+3.3V",
	};
	struct it87_data *data = dev_get_drvdata(dev);
	int nr = to_sensor_dev_attr(attr)->index;
	const char *label;

	if (has_vin3_5v(data) && nr == 0)
		label = labels[0];
	else if (has_scaling(data))
		label = labels_it8721[nr];
	else
		label = labels[nr];

	return sprintf(buf, "%s\n", label);
}
static SENSOR_DEVICE_ATTR(in3_label, S_IRUGO, show_label, NULL, 0);
static SENSOR_DEVICE_ATTR(in7_label, S_IRUGO, show_label, NULL, 1);
static SENSOR_DEVICE_ATTR(in8_label, S_IRUGO, show_label, NULL, 2);
/* AVCC3 */
static SENSOR_DEVICE_ATTR(in9_label, S_IRUGO, show_label, NULL, 3);

static umode_t it87_in_is_visible(struct kobject *kobj,
				  struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct it87_data *data = dev_get_drvdata(dev);
	int i = index / 5;	/* voltage index */
	int a = index % 5;	/* attribute index */

	if (index >= 40) {	/* in8 and higher only have input attributes */
		i = index - 40 + 8;
		a = 0;
	}

	if (!(data->has_in & BIT(i)))
		return 0;

	if (a == 4 && !data->has_beep)
		return 0;

	return attr->mode;
}

static struct attribute *it87_attributes_in[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in0_beep.dev_attr.attr,	/* 4 */

	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_min.dev_attr.attr,
	&sensor_dev_attr_in1_max.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_beep.dev_attr.attr,	/* 9 */

	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_min.dev_attr.attr,
	&sensor_dev_attr_in2_max.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_beep.dev_attr.attr,	/* 14 */

	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_min.dev_attr.attr,
	&sensor_dev_attr_in3_max.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_beep.dev_attr.attr,	/* 19 */

	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_min.dev_attr.attr,
	&sensor_dev_attr_in4_max.dev_attr.attr,
	&sensor_dev_attr_in4_alarm.dev_attr.attr,
	&sensor_dev_attr_in4_beep.dev_attr.attr,	/* 24 */

	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in5_min.dev_attr.attr,
	&sensor_dev_attr_in5_max.dev_attr.attr,
	&sensor_dev_attr_in5_alarm.dev_attr.attr,
	&sensor_dev_attr_in5_beep.dev_attr.attr,	/* 29 */

	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in6_min.dev_attr.attr,
	&sensor_dev_attr_in6_max.dev_attr.attr,
	&sensor_dev_attr_in6_alarm.dev_attr.attr,
	&sensor_dev_attr_in6_beep.dev_attr.attr,	/* 34 */

	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in7_min.dev_attr.attr,
	&sensor_dev_attr_in7_max.dev_attr.attr,
	&sensor_dev_attr_in7_alarm.dev_attr.attr,
	&sensor_dev_attr_in7_beep.dev_attr.attr,	/* 39 */

	&sensor_dev_attr_in8_input.dev_attr.attr,	/* 40 */
	&sensor_dev_attr_in9_input.dev_attr.attr,	/* 41 */
	&sensor_dev_attr_in10_input.dev_attr.attr,	/* 42 */
	&sensor_dev_attr_in11_input.dev_attr.attr,	/* 43 */
	&sensor_dev_attr_in12_input.dev_attr.attr,	/* 44 */
	NULL
};

static const struct attribute_group it87_group_in = {
	.attrs = it87_attributes_in,
	.is_visible = it87_in_is_visible,
};

static umode_t it87_temp_is_visible(struct kobject *kobj,
				    struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct it87_data *data = dev_get_drvdata(dev);
	int i = index / 7;	/* temperature index */
	int a = index % 7;	/* attribute index */

	if (!(data->has_temp & BIT(i)))
		return 0;

	if (a && i >= data->num_temp_limit)
		return 0;

	if (a == 3) {
		if (get_temp_type(data, i) == 0)
			return 0;
		if (has_bank_sel(data))
			return 0444;
		return attr->mode;
	}

	if (a == 5 && i >= data->num_temp_offset)
		return 0;

	if (a == 6 && !data->has_beep)
		return 0;

	return attr->mode;
}

static struct attribute *it87_attributes_temp[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp1_type.dev_attr.attr,	/* 3 */
	&sensor_dev_attr_temp1_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_offset.dev_attr.attr,	/* 5 */
	&sensor_dev_attr_temp1_beep.dev_attr.attr,	/* 6 */

	&sensor_dev_attr_temp2_input.dev_attr.attr,	/* 7 */
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp2_min.dev_attr.attr,
	&sensor_dev_attr_temp2_type.dev_attr.attr,
	&sensor_dev_attr_temp2_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_offset.dev_attr.attr,
	&sensor_dev_attr_temp2_beep.dev_attr.attr,

	&sensor_dev_attr_temp3_input.dev_attr.attr,	/* 14 */
	&sensor_dev_attr_temp3_max.dev_attr.attr,
	&sensor_dev_attr_temp3_min.dev_attr.attr,
	&sensor_dev_attr_temp3_type.dev_attr.attr,
	&sensor_dev_attr_temp3_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_offset.dev_attr.attr,
	&sensor_dev_attr_temp3_beep.dev_attr.attr,

	&sensor_dev_attr_temp4_input.dev_attr.attr,	/* 21 */
	&sensor_dev_attr_temp4_max.dev_attr.attr,
	&sensor_dev_attr_temp4_min.dev_attr.attr,
	&sensor_dev_attr_temp4_type.dev_attr.attr,
	&sensor_dev_attr_temp4_alarm.dev_attr.attr,
	&sensor_dev_attr_temp4_offset.dev_attr.attr,
	&sensor_dev_attr_temp4_beep.dev_attr.attr,

	&sensor_dev_attr_temp5_input.dev_attr.attr,
	&sensor_dev_attr_temp5_max.dev_attr.attr,
	&sensor_dev_attr_temp5_min.dev_attr.attr,
	&sensor_dev_attr_temp5_type.dev_attr.attr,
	&sensor_dev_attr_temp5_alarm.dev_attr.attr,
	&sensor_dev_attr_temp5_offset.dev_attr.attr,
	&sensor_dev_attr_temp5_beep.dev_attr.attr,

	&sensor_dev_attr_temp6_input.dev_attr.attr,
	&sensor_dev_attr_temp6_max.dev_attr.attr,
	&sensor_dev_attr_temp6_min.dev_attr.attr,
	&sensor_dev_attr_temp6_type.dev_attr.attr,
	&sensor_dev_attr_temp6_alarm.dev_attr.attr,
	&sensor_dev_attr_temp6_offset.dev_attr.attr,
	&sensor_dev_attr_temp6_beep.dev_attr.attr,
	NULL
};

static const struct attribute_group it87_group_temp = {
	.attrs = it87_attributes_temp,
	.is_visible = it87_temp_is_visible,
};

static umode_t it87_is_visible(struct kobject *kobj,
			       struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct it87_data *data = dev_get_drvdata(dev);

	if ((index == 2 || index == 3) && !data->has_vid)
		return 0;

	if (index > 3 && !(data->in_internal & BIT(index - 4)))
		return 0;

	return attr->mode;
}

static struct attribute *it87_attributes[] = {
	&dev_attr_alarms.attr,
	&sensor_dev_attr_intrusion0_alarm.dev_attr.attr,
	&dev_attr_vrm.attr,				/* 2 */
	&dev_attr_cpu0_vid.attr,			/* 3 */
	&sensor_dev_attr_in3_label.dev_attr.attr,	/* 4 .. 7 */
	&sensor_dev_attr_in7_label.dev_attr.attr,
	&sensor_dev_attr_in8_label.dev_attr.attr,
	&sensor_dev_attr_in9_label.dev_attr.attr,
	NULL
};

static const struct attribute_group it87_group = {
	.attrs = it87_attributes,
	.is_visible = it87_is_visible,
};

static umode_t it87_fan_is_visible(struct kobject *kobj,
				   struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct it87_data *data = dev_get_drvdata(dev);
	int i = index / 5;	/* fan index */
	int a = index % 5;	/* attribute index */

	if (index >= 15) {	/* fan 4..6 don't have divisor attributes */
		i = (index - 15) / 4 + 3;
		a = (index - 15) % 4;
	}

	if (!(data->has_fan & BIT(i)))
		return 0;

	if (a == 3) {				/* beep */
		if (!data->has_beep)
			return 0;
		/* first fan beep attribute is writable */
		if (i == __ffs(data->has_fan))
			return attr->mode | S_IWUSR;
	}

	if (a == 4 && has_16bit_fans(data))	/* divisor */
		return 0;

	return attr->mode;
}

static struct attribute *it87_attributes_fan[] = {
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_min.dev_attr.attr,
	&sensor_dev_attr_fan1_alarm.dev_attr.attr,
	&sensor_dev_attr_fan1_beep.dev_attr.attr,	/* 3 */
	&sensor_dev_attr_fan1_div.dev_attr.attr,	/* 4 */

	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_min.dev_attr.attr,
	&sensor_dev_attr_fan2_alarm.dev_attr.attr,
	&sensor_dev_attr_fan2_beep.dev_attr.attr,
	&sensor_dev_attr_fan2_div.dev_attr.attr,	/* 9 */

	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan3_min.dev_attr.attr,
	&sensor_dev_attr_fan3_alarm.dev_attr.attr,
	&sensor_dev_attr_fan3_beep.dev_attr.attr,
	&sensor_dev_attr_fan3_div.dev_attr.attr,	/* 14 */

	&sensor_dev_attr_fan4_input.dev_attr.attr,	/* 15 */
	&sensor_dev_attr_fan4_min.dev_attr.attr,
	&sensor_dev_attr_fan4_alarm.dev_attr.attr,
	&sensor_dev_attr_fan4_beep.dev_attr.attr,

	&sensor_dev_attr_fan5_input.dev_attr.attr,	/* 19 */
	&sensor_dev_attr_fan5_min.dev_attr.attr,
	&sensor_dev_attr_fan5_alarm.dev_attr.attr,
	&sensor_dev_attr_fan5_beep.dev_attr.attr,

	&sensor_dev_attr_fan6_input.dev_attr.attr,	/* 23 */
	&sensor_dev_attr_fan6_min.dev_attr.attr,
	&sensor_dev_attr_fan6_alarm.dev_attr.attr,
	&sensor_dev_attr_fan6_beep.dev_attr.attr,
	NULL
};

static const struct attribute_group it87_group_fan = {
	.attrs = it87_attributes_fan,
	.is_visible = it87_fan_is_visible,
};

static umode_t it87_pwm_is_visible(struct kobject *kobj,
				   struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct it87_data *data = dev_get_drvdata(dev);
	int i = index / 4;	/* pwm index */
	int a = index % 4;	/* attribute index */

	if (!(data->has_pwm & BIT(i)))
		return 0;

	/* pwmX_auto_channels_temp is only writable if auto pwm is supported */
	if (a == 3 && (has_old_autopwm(data) || has_newer_autopwm(data)))
		return attr->mode | S_IWUSR;

	/* pwm2_freq is writable if there are two pwm frequency selects */
	if (has_pwm_freq2(data) && i == 1 && a == 2)
		return attr->mode | S_IWUSR;

	return attr->mode;
}

static struct attribute *it87_attributes_pwm[] = {
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_freq.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_channels_temp.dev_attr.attr,

	&sensor_dev_attr_pwm2_enable.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm2_freq.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_channels_temp.dev_attr.attr,

	&sensor_dev_attr_pwm3_enable.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm3_freq.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_channels_temp.dev_attr.attr,

	&sensor_dev_attr_pwm4_enable.dev_attr.attr,
	&sensor_dev_attr_pwm4.dev_attr.attr,
	&sensor_dev_attr_pwm4_freq.dev_attr.attr,
	&sensor_dev_attr_pwm4_auto_channels_temp.dev_attr.attr,

	&sensor_dev_attr_pwm5_enable.dev_attr.attr,
	&sensor_dev_attr_pwm5.dev_attr.attr,
	&sensor_dev_attr_pwm5_freq.dev_attr.attr,
	&sensor_dev_attr_pwm5_auto_channels_temp.dev_attr.attr,

	&sensor_dev_attr_pwm6_enable.dev_attr.attr,
	&sensor_dev_attr_pwm6.dev_attr.attr,
	&sensor_dev_attr_pwm6_freq.dev_attr.attr,
	&sensor_dev_attr_pwm6_auto_channels_temp.dev_attr.attr,

	NULL
};

static const struct attribute_group it87_group_pwm = {
	.attrs = it87_attributes_pwm,
	.is_visible = it87_pwm_is_visible,
};

static umode_t it87_auto_pwm_is_visible(struct kobject *kobj,
					struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct it87_data *data = dev_get_drvdata(dev);
	int i = index / 11;	/* pwm index */
	int a = index % 11;	/* attribute index */

	if (index >= 33) {	/* pwm 4..6 */
		i = (index - 33) / 6 + 3;
		a = (index - 33) % 6 + 4;
	}

	if (!(data->has_pwm & BIT(i)))
		return 0;

	if (has_newer_autopwm(data)) {
		if (a < 4)	/* no auto point pwm */
			return 0;
		if (a == 8)	/* no auto_point4 */
			return 0;
	}
	if (has_old_autopwm(data)) {
		if (a >= 9)	/* no pwm_auto_start, pwm_auto_slope */
			return 0;
	}

	return attr->mode;
}

static struct attribute *it87_attributes_auto_pwm[] = {
	&sensor_dev_attr_pwm1_auto_point1_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point2_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point3_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point4_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point4_temp.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_start.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_slope.dev_attr.attr,

	&sensor_dev_attr_pwm2_auto_point1_pwm.dev_attr.attr,	/* 11 */
	&sensor_dev_attr_pwm2_auto_point2_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point3_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point4_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point4_temp.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_start.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_slope.dev_attr.attr,

	&sensor_dev_attr_pwm3_auto_point1_pwm.dev_attr.attr,	/* 22 */
	&sensor_dev_attr_pwm3_auto_point2_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point3_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point4_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point4_temp.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_start.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_slope.dev_attr.attr,

	&sensor_dev_attr_pwm4_auto_point1_temp.dev_attr.attr,	/* 33 */
	&sensor_dev_attr_pwm4_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_pwm4_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_pwm4_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_pwm4_auto_start.dev_attr.attr,
	&sensor_dev_attr_pwm4_auto_slope.dev_attr.attr,

	&sensor_dev_attr_pwm5_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_pwm5_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_pwm5_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_pwm5_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_pwm5_auto_start.dev_attr.attr,
	&sensor_dev_attr_pwm5_auto_slope.dev_attr.attr,

	&sensor_dev_attr_pwm6_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_pwm6_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_pwm6_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_pwm6_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_pwm6_auto_start.dev_attr.attr,
	&sensor_dev_attr_pwm6_auto_slope.dev_attr.attr,

	NULL,
};

static const struct attribute_group it87_group_auto_pwm = {
	.attrs = it87_attributes_auto_pwm,
	.is_visible = it87_auto_pwm_is_visible,
};

/*
 * Original explanation:
 * On various Gigabyte AM4 boards (AB350, AX370), the second Super-IO chip
 * (IT8792E) needs to be in configuration mode before accessing the first
 * due to a bug in IT8792E which otherwise results in LPC bus access errors.
 * This needs to be done before accessing the first Super-IO chip since
 * the second chip may have been accessed prior to loading this driver.
 *
 * The problem is also reported to affect IT8795E, which is used on X299 boards
 * and has the same chip ID as IT8792E (0x8733). It also appears to affect
 * systems with IT8790E, which is used on some Z97X-Gaming boards as well as
 * Z87X-OC.
 *
 * From other information supplied:
 * ChipIDs 0x8733, 0x8695 (early ID for IT87952E) and 0x8790 are intialised
 * and left in configuration mode, and entering and/or exiting configuration
 * mode is what causes the crash.
 *
 * The recommendation is to look up the chipID before doing any mode swap
 * and then act accordingly.
 */
/* SuperIO detection - will change isa_address if a chip is found */
static int __init it87_find(int sioaddr, unsigned short *address,
			    phys_addr_t *mmio_address,
			    struct it87_sio_data *sio_data,
			    int chip_cnt)
{
	const struct it87_devices *config = NULL;
	phys_addr_t base = 0;
	char mmio_str[32];
	u16 chip_type;
	int err;
	bool enabled = false;

	/* First step, lock memory but don't enter configuration mode */
	err = superio_enter(sioaddr, true);
	if (err)
		return err;

	sio_data->sioaddr = sioaddr;

	err = -ENODEV;
	chip_type = superio_inw(sioaddr, DEVID);
	/* Check for a valid chip before forcing chip id */
	if (chip_type == 0xffff) {
		/* Enter configuration mode */
		__superio_enter(sioaddr);
		enabled = true;
		/* and then try again */
		chip_type = superio_inw(sioaddr, DEVID);
		if (chip_type == 0xffff)
			goto exit;
	}

	if (force_id_cnt == 1) {
		/* If only one value given use for all chips */
		if (force_id[0])
			chip_type = force_id[0];
	} else if (force_id[chip_cnt])
		chip_type = force_id[chip_cnt];

	switch (chip_type) {
	case IT8705F_DEVID:
		sio_data->type = it87;
		break;
	case IT8712F_DEVID:
		sio_data->type = it8712;
		break;
	case IT8716F_DEVID:
	case IT8726F_DEVID:
		sio_data->type = it8716;
		break;
	case IT8718F_DEVID:
		sio_data->type = it8718;
		break;
	case IT8720F_DEVID:
		sio_data->type = it8720;
		break;
	case IT8721F_DEVID:
		sio_data->type = it8721;
		break;
	case IT8728F_DEVID:
		sio_data->type = it8728;
		break;
	case IT8732F_DEVID:
		sio_data->type = it8732;
		break;
	case IT8736F_DEVID:
		sio_data->type = it8736;
		break;
	case IT8738E_DEVID:
		sio_data->type = it8738;
		break;
	case IT8792E_DEVID:
		sio_data->type = it8792;
		break;
	case IT8771E_DEVID:
		sio_data->type = it8771;
		break;
	case IT8772E_DEVID:
		sio_data->type = it8772;
		break;
	case IT8781F_DEVID:
		sio_data->type = it8781;
		break;
	case IT8782F_DEVID:
		sio_data->type = it8782;
		break;
	case IT8783E_DEVID:
		sio_data->type = it8783;
		break;
	case IT8785E_DEVID:
		sio_data->type = it8785;
		break;
	case IT8786E_DEVID:
		sio_data->type = it8786;
		break;
	case IT8790E_DEVID:
		sio_data->type = it8790;
		break;
	case IT8603E_DEVID:
	case IT8623E_DEVID:
		sio_data->type = it8603;
		break;
	case IT8606E_DEVID:
		sio_data->type = it8606;
		break;
	case IT8607E_DEVID:
		sio_data->type = it8607;
		break;
	case IT8613E_DEVID:
		sio_data->type = it8613;
		break;
	case IT8620E_DEVID:
		sio_data->type = it8620;
		break;
	case IT8622E_DEVID:
		sio_data->type = it8622;
		break;
	case IT8625E_DEVID:
		sio_data->type = it8625;
		break;
	case IT8628E_DEVID:
		sio_data->type = it8628;
		break;
	case IT8655E_DEVID:
		sio_data->type = it8655;
		break;
	case IT8665E_DEVID:
		sio_data->type = it8665;
		break;
	case IT8686E_DEVID:
		sio_data->type = it8686;
		break;
	case IT8688E_DEVID:
		sio_data->type = it8688;
		break;
	case IT8689E_DEVID:
		sio_data->type = it8689;
		break;
	case IT87952E_DEVID:
		sio_data->type = it87952;
		break;
	case IT8696E_DEVID:
		sio_data->type = it8696;
		break;
	case IT8698E_DEVID:
		sio_data->type = it8698;
		break;
	case 0xffff:	/* No device at all */
		goto exit;
	default:
		pr_debug("Unsupported chip (DEVID=0x%x)\n", chip_type);
		goto exit;
	}

	config = &it87_devices[sio_data->type];

	/*
	 * If previously we didn't enter configuration mode and it isn't a
	 * chip we know is initialised in configuration mode, then enter
	 * configuration mode.
	 *
	 * I don't know if any such chips can exist but be defensive.
	 */
	if (!enabled && !has_noconf(config)) {
		__superio_enter(sioaddr);
		enabled = true;
	}

	superio_select(sioaddr, PME);
	if (!(superio_inb(sioaddr, IT87_ACT_REG) & 0x01)) {
		pr_info("Device (chip %s ioreg 0x%x) not activated, skipping\n",
			config->model, sioaddr);
		goto exit;
	}

	*address = superio_inw(sioaddr, IT87_BASE_REG) & ~(IT87_EXTENT - 1);
	if (*address == 0) {
		pr_info("Base address not set (chip %s ioreg 0x%x), skipping\n",
			config->model, sioaddr);
		goto exit;
	}

	err = 0;
	sio_data->revision = superio_inb(sioaddr, DEVREV) & 0x0f;
    /* Check for standard mmio support. If so, then compose base address*/
	if ((has_mmio(config) || has_bridge_mmio(config)) && mmio) {
		u8 reg;

		reg = superio_inb(sioaddr, IT87_EC_HWM_MIO_REG);
		if (reg & BIT(5)) {
			base = 0xf0000000 + ((reg & 0x0f) << 24);
			base += (reg & 0xc0) << 14;

			if (has_bridge_mmio(config)) {
			    sio_data->mmio_bridge = 1;
			} else {
				sio_data->mmio = 1;
			}
		}
	}
    /* Check for H2RAM MMIO */
	if (has_h2ram_mmio(config) && mmio) {
		u8 enable;
		u8 reg;
        u8 reg1;
        u8 reg2;
        /* Select H2RAM (SMFI) configuration space */
        superio_select(sioaddr, H2RAM);
        /* Read SMFI enable register */
        enable = superio_inb(sioaddr, IT87_SMFI_ENABLE);
        /* If SMFI is enabled Read MMIO base address */
        if (enable) {
            reg  = superio_inb(sioaddr, IT87_SMFI_BASE_LOW);
            reg1 = superio_inb(sioaddr, IT87_SMFI_BASE_HI);
            /* If IT87952 compose 24 bit address */
            if (has_h2ram_ex_addr(config)) {
                reg2  = superio_inb(sioaddr, IT87_SMFI_BASE_EX);
                base  = 0xFC000000;
                base |= (u32)reg1 << 16;           /* F6 full       */
                base |= (u32)(reg & 0xF0) << 12;   /* F5 high nib   */
                base |= (u32)(reg2 & 0x0F) << 24;
            }
            /* If not IT87952 compose 16 bit address */
            else {
                base  = 0xFF000000;
                base |= (u32)reg1 << 16;           /* F6 full       */
                base |= (u32)(reg & 0xF0) << 12;
            }
            sio_data->mmio_h2ram = 1;
        }
        /* If SMFI is disabled check if has ECIO and on AMD, if so, then enable ECIO */
        else if ((boot_cpu_data.x86_vendor == X86_VENDOR_AMD) && has_h2ram_ecio(config)) {
            pr_info("AMD platform with ECIO H2RAM detected, enabling ECIO backend\n");
            sio_data->ecio_h2ram = 1;
        }

        superio_select(sioaddr, PME);
	}

	*mmio_address = base;

	mmio_str[0] = '\0';
	if (base)
		snprintf(mmio_str, sizeof(mmio_str), " [MMIO at %pa]", &base);

	pr_info("Found %s chip at 0x%x%s, revision %d\n",
			it87_devices[sio_data->type].model,
			*address, mmio_str, sio_data->revision);

	/* in7 (VSB or VCCH5V) is always internal on some chips */
	if (has_in7_internal(config))
		sio_data->internal |= BIT(1);

	/* in8 (Vbat) is always internal */
	sio_data->internal |= BIT(2);

	/* in9 (AVCC3), always internal if supported */
	if (has_avcc3(config))
		sio_data->internal |= BIT(3); /* in9 is AVCC */
	else
		sio_data->skip_in |= BIT(9);

	if (!has_four_pwm(config))
		sio_data->skip_pwm |= BIT(3) | BIT(4) | BIT(5);
	else if (!has_five_pwm(config))
		sio_data->skip_pwm |= BIT(4) | BIT(5);
	else if (!has_six_pwm(config))
		sio_data->skip_pwm |= BIT(5);

	if (!has_vid(config))
		sio_data->skip_vid = 1;

	/* Read GPIO config and VID value from LDN 7 (GPIO) */
	if (sio_data->type == it87) {
		/* The IT8705F has a different LD number for GPIO */
		superio_select(sioaddr, 5);
		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8783) {
		int reg25, reg27, reg2a, reg2c, regef;

		superio_select(sioaddr, GPIO);

		reg25 = superio_inb(sioaddr, IT87_SIO_GPIO1_REG);
		reg27 = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		reg2a = superio_inb(sioaddr, IT87_SIO_PINX1_REG);
		reg2c = superio_inb(sioaddr, IT87_SIO_PINX2_REG);
		regef = superio_inb(sioaddr, IT87_SIO_SPI_REG);

		/* Check if fan3 is there or not */
		if ((reg27 & BIT(0)) || !(reg2c & BIT(2)))
			sio_data->skip_fan |= BIT(2);
		if ((reg25 & BIT(4)) ||
		    (!(reg2a & BIT(1)) && (regef & BIT(0))))
			sio_data->skip_pwm |= BIT(2);

		/* Check if fan2 is there or not */
		if (reg27 & BIT(7))
			sio_data->skip_fan |= BIT(1);
		if (reg27 & BIT(3))
			sio_data->skip_pwm |= BIT(1);

		/* VIN5 */
		if ((reg27 & BIT(0)) || (reg2c & BIT(2)))
			sio_data->skip_in |= BIT(5); /* No VIN5 */

		/* VIN6 */
		if (reg27 & BIT(1))
			sio_data->skip_in |= BIT(6); /* No VIN6 */

		/*
		 * VIN7
		 * Does not depend on bit 2 of Reg2C, contrary to datasheet.
		 */
		if (reg27 & BIT(2)) {
			/*
			 * The data sheet is a bit unclear regarding the
			 * internal voltage divider for VCCH5V. It says
			 * "This bit enables and switches VIN7 (pin 91) to the
			 * internal voltage divider for VCCH5V".
			 * This is different to other chips, where the internal
			 * voltage divider would connect VIN7 to an internal
			 * voltage source. Maybe that is the case here as well.
			 *
			 * Since we don't know for sure, re-route it if that is
			 * not the case, and ask the user to report if the
			 * resulting voltage is sane.
			 */
			if (!(reg2c & BIT(1))) {
				reg2c |= BIT(1);
				superio_outb(sioaddr, IT87_SIO_PINX2_REG,
					     reg2c);
				sio_data->need_in7_reroute = true;
				pr_notice("Routing internal VCCH5V to in7.\n");
			}
			pr_notice("in7 routed to internal voltage divider, with external pin disabled.\n");
			pr_notice("Please report if it displays a reasonable voltage.\n");
		}

		if (reg2c & BIT(0))
			sio_data->internal |= BIT(0);
		if (reg2c & BIT(1))
			sio_data->internal |= BIT(1);

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8603 || sio_data->type == it8606 ||
		   sio_data->type == it8607) {
		int reg27, reg29;

		superio_select(sioaddr, GPIO);

		reg27 = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);

		/* Check if fan3 is there or not */
		if (reg27 & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg27 & BIT(7))
			sio_data->skip_fan |= BIT(2);

		/* Check if fan2 is there or not */
		reg29 = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg29 & BIT(1))
			sio_data->skip_pwm |= BIT(1);
		if (reg29 & BIT(2))
			sio_data->skip_fan |= BIT(1);

		switch (sio_data->type) {
		case it8603:
			sio_data->skip_in |= BIT(5); /* No VIN5 */
			sio_data->skip_in |= BIT(6); /* No VIN6 */
			break;
		case it8607:
			sio_data->skip_pwm |= BIT(0);/* No fan1 */
			sio_data->skip_fan |= BIT(0);
			break;
		default:
			break;
		}

		sio_data->beep_pin = superio_inb(sioaddr,
				IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8613) {
		int reg27, reg29, reg2a;

		superio_select(sioaddr, GPIO);

		/* Check for pwm3, fan3, pwm5, fan5 */
		reg27 = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		if (!(reg27 & BIT(1)))
			sio_data->skip_fan |= BIT(4);
		if (reg27 & BIT(3))
			sio_data->skip_pwm |= BIT(4);
		if (reg27 & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg27 & BIT(7))
			sio_data->skip_fan |= BIT(2);

		/* Check for pwm2, fan2 */
		reg29 = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg29 & BIT(1))
			sio_data->skip_pwm |= BIT(1);
		if (reg29 & BIT(2))
			sio_data->skip_fan |= BIT(1);

		/* Check for pwm4, fan4 */
		reg2a = superio_inb(sioaddr, IT87_SIO_PINX1_REG);
		if (!(reg2a & BIT(0)) || (reg29 & BIT(7))) {
			sio_data->skip_fan |= BIT(3);
			sio_data->skip_pwm |= BIT(3);
		}

		sio_data->skip_pwm |= BIT(0); /* No pwm1 */
		sio_data->skip_fan |= BIT(0); /* No fan1 */
		sio_data->skip_in |= BIT(3);  /* No VIN3 */
		sio_data->skip_in |= BIT(6);  /* No VIN6 */

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;

	}
	else if (sio_data->type == it8620 || sio_data->type == it8628 ||
		 sio_data->type == it8686 ||
		 sio_data->type == it8688 || sio_data->type == it8689) {
		int reg;

		superio_select(sioaddr, GPIO);

		/* Check for pwm5 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO1_REG);
		if (reg & BIT(6))
			sio_data->skip_pwm |= BIT(4);

		/* Check for fan4, fan5 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO2_REG);
		if (!(reg & BIT(5)))
			sio_data->skip_fan |= BIT(3);
		if (!(reg & BIT(4)))
			sio_data->skip_fan |= BIT(4);

		/* Check for pwm3, fan3 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		if (reg & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg & BIT(7))
			sio_data->skip_fan |= BIT(2);

		/* Check for pwm4 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO4_REG);
		if (reg & BIT(2))
			sio_data->skip_pwm |= BIT(3);

		/* Check for pwm2, fan2 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg & BIT(1))
			sio_data->skip_pwm |= BIT(1);
		if (reg & BIT(2))
			sio_data->skip_fan |= BIT(1);
		/* Check for pwm6, fan6 */
		if (!(reg & BIT(7))) {
			sio_data->skip_pwm |= BIT(5);
			sio_data->skip_fan |= BIT(5);
		}

		/* Check if AVCC is on VIN3 */
		reg = superio_inb(sioaddr, IT87_SIO_PINX2_REG);
		if (reg & BIT(0)) {
			/* For it8686, the bit just enables AVCC3 */
			if (sio_data->type != it8686 &&
			    sio_data->type != it8688 &&
			    sio_data->type != it8689)
				sio_data->internal |= BIT(0);
		} else {
			sio_data->internal &= ~BIT(3);
			sio_data->skip_in |= BIT(9);
		}

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8622) {
		int reg;

		superio_select(sioaddr, GPIO);

		/* Check for pwm4, fan4 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO1_REG);
		if (reg & BIT(6))
			sio_data->skip_fan |= BIT(3);
		if (reg & BIT(5))
			sio_data->skip_pwm |= BIT(3);

		/* Check for pwm3, fan3, pwm5, fan5 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		if (reg & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg & BIT(7))
			sio_data->skip_fan |= BIT(2);
		if (reg & BIT(3))
			sio_data->skip_pwm |= BIT(4);
		if (reg & BIT(1))
			sio_data->skip_fan |= BIT(4);

		/* Check for pwm2, fan2 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg & BIT(1))
			sio_data->skip_pwm |= BIT(1);
		if (reg & BIT(2))
			sio_data->skip_fan |= BIT(1);

		/* Check for AVCC */
		reg = superio_inb(sioaddr, IT87_SIO_PINX2_REG);
		if (!(reg & BIT(0)))
			sio_data->skip_in |= BIT(9);

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8732 || sio_data->type == it8736 ||
		   sio_data->type == it8738) {
		int reg;

		superio_select(sioaddr, GPIO);

		/* Check for pwm2, fan2 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg & BIT(1))
			sio_data->skip_pwm |= BIT(1);
		if (reg & BIT(2))
			sio_data->skip_fan |= BIT(1);

		/* Check for pwm3, fan3, fan4 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		if (reg & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg & BIT(7))
			sio_data->skip_fan |= BIT(2);
		if (reg & BIT(5))
			sio_data->skip_fan |= BIT(3);

		/* Check if AVCC is on VIN3 */
		if (sio_data->type != it8738) {
			reg = superio_inb(sioaddr, IT87_SIO_PINX2_REG);
			if (reg & BIT(0))
				sio_data->internal |= BIT(0);
		}

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8655) {
		int reg;

		superio_select(sioaddr, GPIO);

		/* Check for pwm2 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg & BIT(1))
			sio_data->skip_pwm |= BIT(1);

		/* Check for fan2 */
		reg = superio_inb(sioaddr, IT87_SIO_PINX4_REG);
		if (reg & BIT(4))
			sio_data->skip_fan |= BIT(1);

		/* Check for pwm3, fan3 */
		reg = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		if (reg & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg & BIT(7))
			sio_data->skip_fan |= BIT(2);

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else if (sio_data->type == it8665 || sio_data->type == it8625) {
		int reg27, reg29, reg2d, regd3;

		superio_select(sioaddr, GPIO);

		reg27 = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		reg29 = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		reg2d = superio_inb(sioaddr, IT87_SIO_PINX4_REG);
		regd3 = superio_inb(sioaddr, IT87_SIO_GPIO9_REG);

		/* Check for pwm2 */
		if (reg29 & BIT(1))
			sio_data->skip_pwm |= BIT(1);

		/* Check for pwm3, fan3 */
		if (reg27 & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg27 & BIT(7))
			sio_data->skip_fan |= BIT(2);

		/* Check for fan2, pwm4, fan4, pwm5, fan5 */
		if (sio_data->type == it8625) {
			int reg25 = superio_inb(sioaddr, IT87_SIO_GPIO1_REG);

			if (reg29 & BIT(2))
				sio_data->skip_fan |= BIT(1);
			if (reg25 & BIT(6))
				sio_data->skip_fan |= BIT(3);
			if (reg25 & BIT(5))
				sio_data->skip_pwm |= BIT(3);
			if (reg27 & BIT(3))
				sio_data->skip_pwm |= BIT(4);
			if (!(reg27 & BIT(1)))
				sio_data->skip_fan |= BIT(4);
		} else {
			int reg26 = superio_inb(sioaddr, IT87_SIO_GPIO2_REG);

			if (reg2d & BIT(4))
				sio_data->skip_fan |= BIT(1);
			if (regd3 & BIT(2))
				sio_data->skip_pwm |= BIT(3);
			if (regd3 & BIT(3))
				sio_data->skip_fan |= BIT(3);
			if (reg26 & BIT(5))
				sio_data->skip_pwm |= BIT(4);
			/*
			 * Table 6-1 in datasheet claims that FAN_TAC5 would
			 * be enabled with 26h[4]=0. This contradicts with the
			 * information in section 8.3.9 and with feedback from
			 * users.
			 */
			if (!(reg26 & BIT(4)))
				sio_data->skip_fan |= BIT(4);
		}

		/* Check for pwm6, fan6 */
		if (regd3 & BIT(0))
			sio_data->skip_pwm |= BIT(5);
		if (regd3 & BIT(1))
			sio_data->skip_fan |= BIT(5);

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	} else {
		int reg;
		bool uart6;

		superio_select(sioaddr, GPIO);

		/* Check for fan4, fan5 */
		if (has_five_fans(config)) {
			reg = superio_inb(sioaddr, IT87_SIO_GPIO2_REG);
			switch (sio_data->type) {
			case it8718:
				if (reg & BIT(5))
					sio_data->skip_fan |= BIT(3);
				if (reg & BIT(4))
					sio_data->skip_fan |= BIT(4);
				break;
			case it8720:
			case it8721:
			case it8728:
				if (!(reg & BIT(5)))
					sio_data->skip_fan |= BIT(3);
				if (!(reg & BIT(4)))
					sio_data->skip_fan |= BIT(4);
				break;
			default:
				break;
			}
		}

		reg = superio_inb(sioaddr, IT87_SIO_GPIO3_REG);
		if (!sio_data->skip_vid) {
			/* We need at least 4 VID pins */
			if (reg & 0x0f) {
				pr_info("VID is disabled (pins used for GPIO)\n");
				sio_data->skip_vid = 1;
			}
		}

		/* Check if fan3 is there or not */
		if (reg & BIT(6))
			sio_data->skip_pwm |= BIT(2);
		if (reg & BIT(7))
			sio_data->skip_fan |= BIT(2);

		/* Check if fan2 is there or not */
		if (sio_data->type == it8785)
			reg = superio_inb(sioaddr, IT87_SIO_GPIO4_REG);
		else
			reg = superio_inb(sioaddr, IT87_SIO_GPIO5_REG);
		if (reg & BIT(1))
			sio_data->skip_pwm |= BIT(1);
		if (reg & BIT(2))
			sio_data->skip_fan |= BIT(1);

		if ((sio_data->type == it8718 || sio_data->type == it8720) &&
		    !(sio_data->skip_vid))
			sio_data->vid_value = superio_inb(sioaddr,
							  IT87_SIO_VID_REG);

		reg = superio_inb(sioaddr, IT87_SIO_PINX2_REG);

		uart6 = sio_data->type == it8782 && (reg & BIT(2));

		/*
		 * The IT8720F has no VIN7 pin, so VCCH5V should always be
		 * routed internally to VIN7 with an internal divider.
		 * Curiously, there still is a configuration bit to control
		 * this, which means it can be set incorrectly. And even
		 * more curiously, many boards out there are improperly
		 * configured, even though the IT8720F datasheet claims
		 * that the internal routing of VCCH5V to VIN7 is the default
		 * setting. So we force the internal routing in this case.
		 *
		 * On IT8782F, VIN7 is multiplexed with one of the UART6 pins.
		 * If UART6 is enabled, re-route VIN7 to the internal divider
		 * if that is not already the case.
		 */
		if ((sio_data->type == it8720 || uart6) && !(reg & BIT(1))) {
			reg |= BIT(1);
			superio_outb(sioaddr, IT87_SIO_PINX2_REG, reg);
			sio_data->need_in7_reroute = true;
			pr_notice("Routing internal VCCH5V to in7\n");
		}
		if (reg & BIT(0))
			sio_data->internal |= BIT(0);
		if (reg & BIT(1))
			sio_data->internal |= BIT(1);

		/*
		 * On IT8782F, UART6 pins overlap with VIN5, VIN6, and VIN7.
		 * While VIN7 can be routed to the internal voltage divider,
		 * VIN5 and VIN6 are not available if UART6 is enabled.
		 *
		 * Also, temp3 is not available if UART6 is enabled and TEMPIN3
		 * is the temperature source. Since we can not read the
		 * temperature source here, skip_temp is preliminary.
		 */
		if (uart6) {
			sio_data->skip_in |= BIT(5) | BIT(6);
			sio_data->skip_temp |= BIT(2);
		}

		sio_data->beep_pin = superio_inb(sioaddr,
						 IT87_SIO_BEEP_PIN_REG) & 0x3f;
	}
	if (sio_data->beep_pin)
		pr_info("Beeping is supported\n");

	/* Set values based on DMI matches */
	if (dmi_data)
		sio_data->skip_pwm |= dmi_data->skip_pwm;

	if (config->smbus_bitmap && !base) {
		u8 reg;

		superio_select(sioaddr, PME);
		reg = superio_inb(sioaddr, IT87_SPECIAL_CFG_REG);
		sio_data->ec_special_config = reg;
		sio_data->smbus_bitmap = reg & config->smbus_bitmap;
	}

exit:
	superio_exit(sioaddr, !enabled);
	return err;
}

static void it87_init_regs(struct platform_device *pdev)
{
	struct it87_data *data = platform_get_drvdata(pdev);

	/* Initialize chip specific register pointers */
	switch (data->type) {
	case it8628:
	case it8686:
	case it8688:
	case it8689:
	case it8696:
	case it8698:
		data->REG_FAN = IT87_REG_FAN;
		data->REG_FANX = IT87_REG_FANX;
		data->REG_FAN_MIN = IT87_REG_FAN_MIN;
		data->REG_FANX_MIN = IT87_REG_FANX_MIN;
		data->REG_PWM = IT87_REG_PWM;
		data->REG_TEMP_OFFSET = IT87_REG_TEMP_OFFSET_8686;
		data->REG_TEMP_LOW = IT87_REG_TEMP_LOW_8686;
		data->REG_TEMP_HIGH = IT87_REG_TEMP_HIGH_8686;
		break;
	case it8625:
	case it8655:
	case it8665:
		data->REG_FAN = IT87_REG_FAN_8665;
		data->REG_FANX = IT87_REG_FANX_8665;
		data->REG_FAN_MIN = IT87_REG_FAN_MIN_8665;
		data->REG_FANX_MIN = IT87_REG_FANX_MIN_8665;
		data->REG_PWM = IT87_REG_PWM_8665;
		data->REG_TEMP_OFFSET = IT87_REG_TEMP_OFFSET;
		data->REG_TEMP_LOW = IT87_REG_TEMP_LOW;
		data->REG_TEMP_HIGH = IT87_REG_TEMP_HIGH;
		break;
	case it8622:
		data->REG_FAN = IT87_REG_FAN;
		data->REG_FANX = IT87_REG_FANX;
		data->REG_FAN_MIN = IT87_REG_FAN_MIN;
		data->REG_FANX_MIN = IT87_REG_FANX_MIN;
		data->REG_PWM = IT87_REG_PWM_8665;
		data->REG_TEMP_OFFSET = IT87_REG_TEMP_OFFSET;
		data->REG_TEMP_LOW = IT87_REG_TEMP_LOW;
		data->REG_TEMP_HIGH = IT87_REG_TEMP_HIGH;
		break;
	case it8613:
		data->REG_FAN = IT87_REG_FAN;
		data->REG_FANX = IT87_REG_FANX;
		data->REG_FAN_MIN = IT87_REG_FAN_MIN;
		data->REG_FANX_MIN = IT87_REG_FANX_MIN;
		data->REG_PWM = IT87_REG_PWM_8665;
		data->REG_TEMP_OFFSET = IT87_REG_TEMP_OFFSET;
		data->REG_TEMP_LOW = IT87_REG_TEMP_LOW;
		data->REG_TEMP_HIGH = IT87_REG_TEMP_HIGH;
		break;
	default:
		data->REG_FAN = IT87_REG_FAN;
		data->REG_FANX = IT87_REG_FANX;
		data->REG_FAN_MIN = IT87_REG_FAN_MIN;
		data->REG_FANX_MIN = IT87_REG_FANX_MIN;
		data->REG_PWM = IT87_REG_PWM;
		data->REG_TEMP_OFFSET = IT87_REG_TEMP_OFFSET;
		data->REG_TEMP_LOW = IT87_REG_TEMP_LOW;
		data->REG_TEMP_HIGH = IT87_REG_TEMP_HIGH;
		break;
	}
	/* Sets various read/write routines for MMIO/ECIO devices *
	 * it87_bridge_read/write use ISA bridge access to MMIO   *
	 * it87_h2ram_read/write uses ISA brdge and conventional  *
	 * I/O port access in the same memory space               *
	 * it87_ecio_read/write uses ECIO (special ports) and     *
	 * conventional I/O in the same memory space              */
	if (data->mmio) {
        if (data->mmio_bridge) {
            data->read  = it87_bridge_read;
            data->write = it87_bridge_write;
        } else if (data->mmio_h2ram) {
            data->read  = it87_h2ram_read;
            data->write = it87_h2ram_write;
        } else {
		    data->read  = it87_mmio_read;
		    data->write = it87_mmio_write;
        }
    } else if (data->ecio_h2ram) {
        data->read  = it87_ecio_read;
        data->write = it87_ecio_write;
	} else if (has_bank_sel(data)) {
		data->read = it87_io_read;
		data->write = it87_io_write;
	} else {
		data->read = _it87_io_read;
		data->write = _it87_io_write;
	}
}

/*
 * Some chips seem to have default value 0xff for all limit
 * registers. For low voltage limits it makes no sense and triggers
 * alarms, so change to 0 instead. For high temperature limits, it
 * means -1 degree C, which surprisingly doesn't trigger an alarm,
 * but is still confusing, so change to 127 degrees C.
 */
static void it87_check_limit_regs(struct it87_data *data)
{
	int i, reg;

	for (i = 0; i < NUM_VIN_LIMIT; i++) {
		reg = data->read(data, IT87_REG_VIN_MIN(i));
		if (reg == 0xff)
			data->write(data, IT87_REG_VIN_MIN(i), 0);
	}
	for (i = 0; i < data->num_temp_limit; i++) {
		reg = data->read(data, data->REG_TEMP_HIGH[i]);
		if (reg == 0xff)
			data->write(data, data->REG_TEMP_HIGH[i], 127);
	}
}


/* Check if voltage monitors are reset manually or by some reason */
static void it87_check_voltage_monitors_reset(struct it87_data *data)
{
	int reg;

	reg = data->read(data, IT87_REG_VIN_ENABLE);
	if ((reg & 0xff) == 0) {
		/* Enable all voltage monitors */
		data->write(data, IT87_REG_VIN_ENABLE, 0xff);
	}
}

/* Check if tachometers are reset manually or by some reason */
static void it87_check_tachometers_reset(struct platform_device *pdev)
{
	struct it87_sio_data *sio_data = dev_get_platdata(&pdev->dev);
	struct it87_data *data = platform_get_drvdata(pdev);
	u8 mask, fan_main_ctrl;

	mask = 0x70 & ~(sio_data->skip_fan << 4);
	fan_main_ctrl = data->read(data, IT87_REG_FAN_MAIN_CTRL);
	if ((fan_main_ctrl & mask) == 0) {
		/* Enable all fan tachometers */
		fan_main_ctrl |= mask;
		data->write(data, IT87_REG_FAN_MAIN_CTRL, data->fan_main_ctrl);
	}
}

/* Set tachometers to 16-bit mode if needed */
static void it87_check_tachometers_16bit_mode(struct platform_device *pdev)
{
	struct it87_data *data = platform_get_drvdata(pdev);
	int reg;

	if (!has_fan16_config(data))
		return;

	reg = data->read(data, IT87_REG_FAN_16BIT);
	if (~reg & 0x07 & data->has_fan) {
		dev_dbg(&pdev->dev,
			"Setting fan1-3 to 16-bit mode\n");
		data->write(data, IT87_REG_FAN_16BIT, reg | 0x07);
	}
}

static void it87_start_monitoring(struct it87_data *data)
{
	data->write(data, IT87_REG_CONFIG,
		    (data->read(data, IT87_REG_CONFIG) & 0x3e)
		    | (update_vbat ? 0x41 : 0x01));
}

/* Called when we have found a new IT87. */
static void it87_init_device(struct platform_device *pdev)
{
	struct it87_sio_data *sio_data = dev_get_platdata(&pdev->dev);
	struct it87_data *data = platform_get_drvdata(pdev);
	int tmp, i;

	if (has_new_tempmap(data)) {
		data->pwm_temp_map_shift = 3;
		data->pwm_temp_map_mask = 0x07;
	} else {
		data->pwm_temp_map_shift = 0;
		data->pwm_temp_map_mask = 0x03;
	}

	/*
	 * For each PWM channel:
	 * - If it is in automatic mode, setting to manual mode should set
	 *   the fan to full speed by default.
	 * - If it is in manual mode, we need a mapping to temperature
	 *   channels to use when later setting to automatic mode later.
	 *   Map to the first sensor by default (we are clueless.)
	 * In both cases, the value can (and should) be changed by the user
	 * prior to switching to a different mode.
	 * Note that this is no longer needed for the IT8721F and later, as
	 * these have separate registers for the temperature mapping and the
	 * manual duty cycle.
	 */
	for (i = 0; i < NUM_AUTO_PWM; i++) {
		data->pwm_temp_map[i] = 0;
		data->pwm_duty[i] = 0x7f;	/* Full speed */
		data->auto_pwm[i][3] = 0x7f;	/* Full speed, hard-coded */
	}

	it87_check_limit_regs(data);

	/*
	 * Temperature channels are not forcibly enabled, as they can be
	 * set to two different sensor types and we can't guess which one
	 * is correct for a given system. These channels can be enabled at
	 * run-time through the temp{1-3}_type sysfs accessors if needed.
	 */

	it87_check_voltage_monitors_reset(data);

	it87_check_tachometers_reset(pdev);

	data->fan_main_ctrl = data->read(data, IT87_REG_FAN_MAIN_CTRL);
	data->has_fan = (data->fan_main_ctrl >> 4) & 0x07;

	it87_check_tachometers_16bit_mode(pdev);

	/* Check for additional fans */
	tmp = data->read(data, IT87_REG_FAN_16BIT);

	if (has_four_fans(data) && (tmp & BIT(4)))
		data->has_fan |= BIT(3); /* fan4 enabled */
	if (has_five_fans(data) && (tmp & BIT(5)))
		data->has_fan |= BIT(4); /* fan5 enabled */
	if (has_six_fans(data)) {
		switch (data->type) {
		case it8620:
		case it8628:
		case it8686:
		case it8688:
		case it8689:
		case it8696:
		case it8698:
			if (tmp & BIT(2))
				data->has_fan |= BIT(5); /* fan6 enabled */
			break;
		case it8625:
		case it8665:
			tmp = data->read(data, IT87_REG_FAN_DIV);
			if (tmp & BIT(3))
				data->has_fan |= BIT(5); /* fan6 enabled */
			break;
		default:
			break;
		}
	}

	/* Fan input pins may be used for alternative functions */
	data->has_fan &= ~sio_data->skip_fan;

	/* Check if pwm6 is enabled */
	if (has_six_pwm(data)) {
		switch (data->type) {
		case it8620:
		case it8686:
		case it8688:
		case it8689:
		case it8696:
		case it8698:
			tmp = data->read(data, IT87_REG_FAN_DIV);
			if (!(tmp & BIT(3)))
				sio_data->skip_pwm |= BIT(5);
			break;
		default:
			break;
		}
	}

	if (has_bank_sel(data)) {
		for (i = 0; i < 3; i++)
			data->temp_src[i] =
				data->read(data, IT87_REG_TEMP_SRC1[i]);
		data->temp_src[3] = data->read(data, IT87_REG_TEMP_SRC2);
	}

	it87_start_monitoring(data);
}

/* Return 1 if and only if the PWM interface is safe to use */
static int it87_check_pwm(struct device *dev)
{
	struct it87_data *data = dev_get_drvdata(dev);
	/*
	 * Some BIOSes fail to correctly configure the IT87 fans. All fans off
	 * and polarity set to active low is sign that this is the case so we
	 * disable pwm control to protect the user.
	 */
	int tmp = data->read(data, IT87_REG_FAN_CTL);

	if ((tmp & 0x87) == 0) {
		if (fix_pwm_polarity) {
			/*
			 * The user asks us to attempt a chip reconfiguration.
			 * This means switching to active high polarity and
			 * inverting all fan speed values.
			 */
			int i;
			u8 pwm[3];

			for (i = 0; i < ARRAY_SIZE(pwm); i++)
				pwm[i] = data->read(data,
						    data->REG_PWM[i]);

			/*
			 * If any fan is in automatic pwm mode, the polarity
			 * might be correct, as suspicious as it seems, so we
			 * better don't change anything (but still disable the
			 * PWM interface).
			 */
			if (!((pwm[0] | pwm[1] | pwm[2]) & 0x80)) {
				dev_info(dev,
					 "Reconfiguring PWM to active high polarity\n");
				data->write(data, IT87_REG_FAN_CTL, tmp | 0x87);
				for (i = 0; i < 3; i++)
					data->write(data, data->REG_PWM[i],
						    0x7f & ~pwm[i]);
				return 1;
			}

			dev_info(dev,
				 "PWM configuration is too broken to be fixed\n");
		}

		return 0;
	} else if (fix_pwm_polarity) {
		dev_info(dev,
			 "PWM configuration looks sane, won't touch\n");
	}

	return 1;
}

static int it87_probe(struct platform_device *pdev)
{
    struct it87_data      *data;
    struct resource       *res_io;
    struct resource       *res_mmio;
    struct resource       *res_ecio;
    struct device         *dev       = &pdev->dev;
    struct it87_sio_data  *sio_data  = dev_get_platdata(dev);
    int                    enable_pwm_interface;
    struct device         *hwmon_dev;
    int                    err;

    data = devm_kzalloc(dev, sizeof(struct it87_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    /*
     * Resource layout from it87_device_add():
     *   IORESOURCE_IO  index 0: EC HWM window
     *   IORESOURCE_IO  index 1: ECIO range
     *   IORESOURCE_MEM index 0: MMIO/H2RAM window
     */

    /* Primary EC I/O window (always present) */
    res_io = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (res_io) {
		if (!devm_request_region(dev, res_io->start, IT87_EC_EXTENT,
					 DRVNAME)) {
			dev_err(dev, "Failed to request Convetional IO region %pR\n", res_io);
			return -EBUSY;
		}
	}

    /* Extended EC I/O window */
    res_ecio = platform_get_resource(pdev, IORESOURCE_IO, 1);
	if (res_ecio) {
		if (!devm_request_region(dev, res_ecio->start, EXT_ECIO_EXTENT,
					 DRVNAME)) {
			dev_err(dev, "Failed to request Extended ECIO region %pR\n", res_ecio);
			return -EBUSY;
		}
	}

	/* Map Memory resources for MMIO */
    res_mmio = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res_mmio)
    {
        data->mmio = devm_ioremap_resource(dev, res_mmio);
        if (IS_ERR(data->mmio))
            return PTR_ERR(data->mmio);
    }
    else
    {
        data->mmio = NULL;
    }

    if(res_io)
		data->addr = res_io->start;
	else
		data->addr = 0;    /* no conventional EC I/O available */
    data->type              = sio_data->type;
    data->sioaddr           = sio_data->sioaddr;
    data->smbus_bitmap      = sio_data->smbus_bitmap;
    data->ec_special_config = sio_data->ec_special_config;
    data->features          = it87_devices[sio_data->type].features;
    data->num_temp_limit    = it87_devices[sio_data->type].num_temp_limit;
    data->num_temp_offset   = it87_devices[sio_data->type].num_temp_offset;
    data->pwm_num_temp_map  = it87_devices[sio_data->type].num_temp_map;
    data->peci_mask         = it87_devices[sio_data->type].peci_mask;
    data->old_peci_mask     = it87_devices[sio_data->type].old_peci_mask;
    data->mmio_bridge       = sio_data->mmio_bridge;
    data->mmio_h2ram        = sio_data->mmio_h2ram;
    data->ecio_h2ram        = sio_data->ecio_h2ram;

    switch(data->type)
    {
    case it87:
        if (sio_data->revision >= 0x03)
        {
            data->features &= ~FEAT_OLD_AUTOPWM;
            data->features |= FEAT_FAN16_CONFIG | FEAT_16BIT_FANS;
        }
        break;
    case it8712:
        if (sio_data->revision >= 0x08)
        {
            data->features &= ~FEAT_OLD_AUTOPWM;
            data->features |= FEAT_FAN16_CONFIG | FEAT_16BIT_FANS |
                              FEAT_FIVE_FANS;
        }
        break;
    default:
        break;
    }

    platform_set_drvdata(pdev, data);
    mutex_init(&data->update_lock);

    /* Initialize register accessors (select IO vs MMIO backend) */
    it87_init_regs(pdev);

    /* Disable SMBus shadowing while probing sensor blocks */
    err = smbus_disable(data);
    if (err)
        return err;

    if ((data->read(data, IT87_REG_CONFIG) & 0x80) ||
       data->read(data, IT87_REG_CHIPID) != 0x90)
    {
        smbus_enable(data);
        return -ENODEV;
    }

    enable_pwm_interface = it87_check_pwm(dev);
    if (!enable_pwm_interface)
        dev_info(dev, "Detected broken BIOS defaults, disabling PWM interface\n");

    if (has_scaling(data))
    {
        if (sio_data->internal & BIT(0))
            data->in_scaled |= BIT(3);   /* in3 is AVCC */
        if (sio_data->internal & BIT(1))
            data->in_scaled |= BIT(7);   /* in7 is VSB */
        if (sio_data->internal & BIT(2))
            data->in_scaled |= BIT(8);   /* in8 is Vbat */
        if (sio_data->internal & BIT(3))
            data->in_scaled |= BIT(9);   /* in9 is AVCC */
    }
    else if (sio_data->type == it8781 || sio_data->type == it8782 || sio_data->type == it8783)
    {
        if (sio_data->internal & BIT(0))
            data->in_scaled |= BIT(3);   /* in3 is VCC5V */
        if (sio_data->internal & BIT(1))
            data->in_scaled |= BIT(7);   /* in7 is VCCH5V */
    }

    data->has_temp = 0x07;
    if (sio_data->skip_temp & BIT(2))
    {
        if (sio_data->type == it8782 &&
           !(data->read(data, IT87_REG_TEMP_EXTRA) & 0x80))
            data->has_temp &= ~BIT(2);
    }

    data->in_internal      = sio_data->internal;
    data->need_in7_reroute = sio_data->need_in7_reroute;
    data->has_in           = 0x3ff & ~sio_data->skip_in;

    if (has_four_temp(data))
    {
        data->has_temp |= BIT(3);
    }
    else if (has_six_temp(data))
    {
        if (sio_data->type == it8655 || sio_data->type == it8665)
        {
            data->has_temp |= BIT(3) | BIT(4) | BIT(5);
        }
        else
        {
            u8 reg = data->read(data, IT87_REG_TEMP456_ENABLE);

            if ((reg & 0x03) >= 0x02)
                data->has_temp |= BIT(3);
            if (((reg >> 2) & 0x03) >= 0x02)
                data->has_temp |= BIT(4);
            if (((reg >> 4) & 0x03) >= 0x02)
                data->has_temp |= BIT(5);

            if ((reg & 0x03) == 0x01)
                data->has_in |= BIT(10);
            if (((reg >> 2) & 0x03) == 0x01)
                data->has_in |= BIT(11);
            if (((reg >> 4) & 0x03) == 0x01)
                data->has_in |= BIT(12);
        }
    }

    data->has_beep = !!sio_data->beep_pin;

    it87_init_device(pdev);

    smbus_enable(data);

    if (!sio_data->skip_vid)
    {
        data->has_vid = true;
        data->vrm     = vid_which_vrm();
        data->vid     = sio_data->vid_value;
    }

    data->groups[0] = &it87_group;
    data->groups[1] = &it87_group_in;
    data->groups[2] = &it87_group_temp;
    data->groups[3] = &it87_group_fan;

    if (enable_pwm_interface)
    {
        data->has_pwm = BIT(ARRAY_SIZE(IT87_REG_PWM)) - 1;
        data->has_pwm &= ~sio_data->skip_pwm;

        data->groups[4] = &it87_group_pwm;
        if (has_old_autopwm(data) || has_newer_autopwm(data))
            data->groups[5] = &it87_group_auto_pwm;
    }

    hwmon_dev = devm_hwmon_device_register_with_groups(dev,
                             it87_devices[sio_data->type].name,
                             data, data->groups);
    return PTR_ERR_OR_ZERO(hwmon_dev);
}


static void it87_resume_sio(struct platform_device *pdev)
{
	struct it87_data *data = dev_get_drvdata(&pdev->dev);
	int err;
	int reg2c;

	if (!data->need_in7_reroute)
		return;

	err = superio_enter(data->sioaddr, has_noconf(data));
	if (err) {
		dev_warn(&pdev->dev,
			 "Unable to enter Super I/O to reroute in7 (%d)",
			 err);
		return;
	}

	superio_select(data->sioaddr, GPIO);

	reg2c = superio_inb(data->sioaddr, IT87_SIO_PINX2_REG);
	if (!(reg2c & BIT(1))) {
		dev_dbg(&pdev->dev,
			"Routing internal VCCH5V to in7 again");

		reg2c |= BIT(1);
		superio_outb(data->sioaddr, IT87_SIO_PINX2_REG,
			     reg2c);
	}

	superio_exit(data->sioaddr, has_noconf(data));
}

static int it87_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct it87_data *data = dev_get_drvdata(dev);
	int err;

	it87_resume_sio(pdev);

	err = it87_lock(data);
	if (err)
		return err;

	it87_check_pwm(dev);
	it87_check_limit_regs(data);
	it87_check_voltage_monitors_reset(data);
	it87_check_tachometers_reset(pdev);
	it87_check_tachometers_16bit_mode(pdev);

	it87_start_monitoring(data);

	/* force update */
	data->valid = false;

	it87_unlock(data);

	it87_update_device(dev);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(it87_dev_pm_ops, NULL, it87_resume);

static struct platform_driver it87_driver = {
	.driver = {
		.name	= DRVNAME,
		.pm	= pm_sleep_ptr(&it87_dev_pm_ops),
	},
	.probe	= it87_probe,
};

static int __init it87_device_add(int index, unsigned short sio_address,
                                  phys_addr_t mmio_address,
                                  const struct it87_sio_data *sio_data)
{
    struct platform_device *pdev;
    struct resource res[3];
    int nres = 0;
    int err;

    memset(res, 0, sizeof(res));

	/* Only allocate IO Ports if we don't use MMIO */
	if (!((sio_data->mmio_bridge || sio_data->mmio) && mmio_address)) {
		/*
		* 1) Primary EC I/O window (always present, always ACPI-checked)
		*/
		res[nres].name  = DRVNAME;
		res[nres].start = sio_address + IT87_EC_OFFSET;
		res[nres].end   = sio_address + IT87_EC_OFFSET + IT87_EC_EXTENT - 1;
		res[nres].flags = IORESOURCE_IO;

		err = acpi_check_resource_conflict(&res[nres]);
		if (err)
		{
			if (dmi_data && dmi_data->skip_acpi_res)
				pr_info("Ignoring expected ACPI resource conflict\n");
			else if (!ignore_resource_conflict)
				return err;
		}

		nres++;

		/* Extended ECIO port pair (I/O, also ACPI-checked)
		 * Reserves base pair between 0x3F0 and 0x3F4 */
		if (sio_data->ecio_h2ram)
		{
			struct resource *io_ecio = &res[nres];

			io_ecio->name  = DRVNAME;
			io_ecio->start = ECIO_DATA;
			io_ecio->end   = ECIO_CMD_STAT;
			io_ecio->flags = IORESOURCE_IO;

			err = acpi_check_resource_conflict(io_ecio);
			if (err)
			{
				if (dmi_data && dmi_data->skip_acpi_res)
					pr_info("Ignoring expected ACPI resource conflict for ECIO\n");
				else if (!ignore_resource_conflict)
					return err;
			}

			nres++;
		}
	}

    /* Secondary MMIO Resource*/
    if (mmio_address)
    {
        phys_addr_t start = mmio_address;
        phys_addr_t end   = mmio_address + MMIO_HI_BOUND; /* 0x000–0x3FF */

        /* H2RAM chips have an extra EC/HWM block mapped into the window
         * at base+0x900..base+0xCFF instead of base+0x000..base+0x3FF. */
        if (sio_data->mmio_h2ram)
        {
            start = mmio_address;
            end   = mmio_address + H2RAM_HI_BOUND;
        }

        res[nres].name  = DRVNAME;
        res[nres].start = start;
        res[nres].end   = end;
        res[nres].flags = IORESOURCE_MEM;
        nres++;
    }

    pdev = platform_device_alloc(DRVNAME, sio_address);
    if (!pdev)
        return -ENOMEM;

    err = platform_device_add_resources(pdev, res, nres);
    if (err)
    {
        pr_err("Device resource addition failed (%d)\n", err);
        goto exit_device_put;
    }

    err = platform_device_add_data(pdev, sio_data,
                                   sizeof(struct it87_sio_data));
    if (err)
    {
        pr_err("Platform data allocation failed\n");
        goto exit_device_put;
    }

    err = platform_device_add(pdev);
    if (err)
    {
        pr_err("Device addition failed (%d)\n", err);
        goto exit_device_put;
    }

    it87_pdev[index] = pdev;
    return 0;

exit_device_put:
    platform_device_put(pdev);
    return err;
}

/* callback function for DMI */
static int it87_dmi_cb(const struct dmi_system_id *dmi_entry)
{
	dmi_data = dmi_entry->driver_data;

	if (dmi_data && dmi_data->skip_pwm)
		pr_info("Disabling pwm2 due to hardware constraints\n");

	return 1;
}

/*
 * On the Shuttle SN68PT, FAN_CTL2 is apparently not
 * connected to a fan, but to something else. One user
 * has reported instant system power-off when changing
 * the PWM2 duty cycle, so we disable it.
 * I use the board name string as the trigger in case
 * the same board is ever used in other systems.
 */
static struct it87_dmi_data nvidia_fn68pt = {
	.skip_pwm = BIT(1),
};

/*
 * On some Gigabyte boards sensors are marked as ACPI regions but not
 * really handled by ACPI calls, as they return no data.
 * Most commonly this is seen on boards with multiple ITE chips.
 * In this case we just ignore the failure and continue on.
 * This is effectively the same as the use of either
 *     acpi_enforce_resources=lax (kernel)
 * or
 *     ignore_resource_conflict=1 (it87)
 * but set programatically.
 */
static struct it87_dmi_data it87_acpi_ignore = {
	.skip_acpi_res = true,
};

#define IT87_DMI_MATCH_VND(vendor, name, cb, data) \
	{ \
		.callback = cb, \
		.matches = { \
			DMI_EXACT_MATCH(DMI_BOARD_VENDOR, vendor), \
			DMI_EXACT_MATCH(DMI_BOARD_NAME, name), \
		}, \
		.driver_data = data, \
	}

#define IT87_DMI_MATCH_GBT(name, cb, data) \
	IT87_DMI_MATCH_VND("Gigabyte Technology Co., Ltd.", name, cb, data)

static const struct dmi_system_id it87_dmi_table[] __initconst = {
	IT87_DMI_MATCH_GBT("A320M-S2H V2-CF", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8686E */
	IT87_DMI_MATCH_GBT("AB350", it87_dmi_cb, NULL),
		/* ? + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("AX370", it87_dmi_cb, NULL),
		/* ? + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("Q370M D3H GSM PLUS", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8686E */
	IT87_DMI_MATCH_GBT("A520I AC", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E */
	IT87_DMI_MATCH_GBT("Z97X-Gaming G1", it87_dmi_cb, NULL),
		/* ? + IT8790E */
	IT87_DMI_MATCH_GBT("TRX40 AORUS XTREME", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("Z390 AORUS ULTRA-CF", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("X399 DESIGNARE EX-CF", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8686E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("B450 AORUS PRO-CF", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8686E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("Z490 AORUS ELITE AC", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E */
	IT87_DMI_MATCH_GBT("B550 AORUS PRO AC", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("B560I AORUS PRO AX", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E */
	IT87_DMI_MATCH_GBT("X570 AORUS ELITE", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E */
	IT87_DMI_MATCH_GBT("X570 AORUS ELITE WIFI", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E */
	IT87_DMI_MATCH_GBT("X570 AORUS MASTER", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("X570 AORUS PRO", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("X570 AORUS PRO WIFI", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("X570 AORUS ULTRA", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E + IT8792E/IT8795E */
	IT87_DMI_MATCH_GBT("X570 I AORUS PRO WIFI", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8688E */
	IT87_DMI_MATCH_GBT("X570S AERO G", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E */
	IT87_DMI_MATCH_GBT("B650M GAMING X AX", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E */
	IT87_DMI_MATCH_GBT("B660M DS3H DDR4", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E */
	IT87_DMI_MATCH_GBT("X670 AORUS ELITE AX", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E + IT87952E */
	IT87_DMI_MATCH_GBT("X670E AORUS MASTER", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E + IT87922E */
	IT87_DMI_MATCH_GBT("H610M H DDR4", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E */
	IT87_DMI_MATCH_GBT("H610M S2H V2", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E */
	IT87_DMI_MATCH_GBT("Z690 AORUS PRO DDR4", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E + IT87952E */
	IT87_DMI_MATCH_GBT("Z690 AORUS PRO", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E + IT87952E */
	IT87_DMI_MATCH_GBT("Z790 AORUS ELITE AX", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E + IT87952E */
	IT87_DMI_MATCH_GBT("Z790 AORUS MASTER", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8689E + IT87952E */
	IT87_DMI_MATCH_GBT("X870I AORUS PRO ICE", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8696E */
	IT87_DMI_MATCH_GBT("X870 AORUS ELITE WIFI7", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT87952E + IT8696E */
	IT87_DMI_MATCH_GBT("X870 AORUS ELITE WIFI7 ICE", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8696E */
	IT87_DMI_MATCH_GBT("X870 GAMING WIFI6", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8696E */
	IT87_DMI_MATCH_GBT("X870E AORUS MASTER", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8696E */
	IT87_DMI_MATCH_GBT("X870 EAGLE WIFI7", it87_dmi_cb,
			   &it87_acpi_ignore),
		/* IT8696E */
	IT87_DMI_MATCH_VND("ASUSTeK COMPUTER INC.", "PRIME B350-PLUS",
			   it87_dmi_cb, NULL),
		/* IT8655E */
	IT87_DMI_MATCH_VND("nVIDIA", "FN68PT", it87_dmi_cb, &nvidia_fn68pt),
	{ }
};
MODULE_DEVICE_TABLE(dmi, it87_dmi_table);

static int __init sm_it87_init(void)
{
    int                 sioaddr[2] = { REG_2E, REG_4E };
    struct it87_sio_data sio_data;
    unsigned short      isa_address[2];
    phys_addr_t         mmio_address;
    bool                found = false;
    int                 i, err;

    pr_info("it87 driver version %s\n", IT87_DRIVER_VERSION);

    err = platform_driver_register(&it87_driver);
    if (err)
        return err;

    dmi_check_system(it87_dmi_table);

    for (i=0; i<ARRAY_SIZE(sioaddr); i++) {
        memset(&sio_data, 0, sizeof(struct it87_sio_data));
        isa_address[i] = 0;
        mmio_address   = 0;

        err = it87_find(sioaddr[i], &isa_address[i], &mmio_address,
                        &sio_data, i);
        if (err || isa_address[i]==0)
            continue;

        /*
         * Don't register second chip if its ISA address matches
         * the first chip's ISA address.
         */
        if (i && isa_address[i]==isa_address[0])
            continue;

        /*
         * If this chip has a valid MMIO address and is marked as using
         * the ISA bridge window (mmio_bridge / mmio_h2ram), configure
         * the global H2 manager slot for it.
         */
        if (mmio_address &&
           (sio_data.mmio_bridge || sio_data.mmio_h2ram)) {
            phys_addr_t base = mmio_address;
            u32         size;
            int         slot;
            int         ret;

            /* H2RAM chips need a 4 KiB window, others 1 KiB */
            if (sio_data.mmio_h2ram)
                size = 0x1000;
            else
                size = 0x400;

            if (!it87_h2_global_inited) {
                ret = it87_h2_global_init();
                if (ret) {
                    pr_debug("H2RAM global bridge init failed: %d\n",
                             ret);
                } else {
                    it87_h2_global_inited = true;
                }
            }
            if (it87_h2_global_ready) {
                /* slot 0 = 0x2E, slot 1 = 0x4E */
                slot = (sioaddr[i]==REG_4E) ? 1 : 0;
                ret = it87_h2_global_set_slot(slot, base, size);
                if (ret) {
                    pr_debug("H2RAM set_slot(%d,%pa,0x%x) failed: %d\n",
                             slot, &base, size, ret);
                }
            }
        }

        err = it87_device_add(i, isa_address[i], mmio_address, &sio_data);
        if (err)
            goto exit_unregister;
        found = true;
    }
    if (!found) {
        err = -ENODEV;
        goto exit_unregister;
    }

    /*
     * If we successfully configured any H2/bridge slots, enable the
     * decode windows now. On AMD this will program slot 0; on Intel
     * it can enable both at once.
     */
    if (it87_h2_global_ready) {
        enum it87_mmio_state st;
        int                  berr;
        st = it87_h2_global_get_state();
        if (st==IT87_MMIO_ORIGINAL || st==IT87_MMIO_DISABLED) {
            berr = it87_h2_global_set_state(IT87_MMIO_ENABLED_BOTH);
            if (berr)
                pr_debug("H2RAM enable windows failed: %d\n", berr);
        }
    }

    return 0;

exit_unregister:
    platform_driver_unregister(&it87_driver);
    return err;
}

static void __exit sm_it87_exit(void) {
    /* NULL check handled by platform_device_unregister */
    platform_device_unregister(it87_pdev[1]);
    platform_device_unregister(it87_pdev[0]);
    it87_h2_global_release();
    platform_driver_unregister(&it87_driver);
}

MODULE_AUTHOR("Chris Gauthron, Jean Delvare <jdelvare@suse.de>, Frank Crawford");
MODULE_DESCRIPTION("IT87xxF/IT86xxE hardware monitoring driver");

module_param_array(force_id, ushort, &force_id_cnt, 0);
MODULE_PARM_DESC(force_id, "Override one or more detected device ID(s)");

module_param(ignore_resource_conflict, bool, 0);
MODULE_PARM_DESC(ignore_resource_conflict, "Ignore ACPI resource conflict");

module_param(mmio, bool, 0);
MODULE_PARM_DESC(mmio, "Use MMIO if available");

module_param(update_vbat, bool, 0);
MODULE_PARM_DESC(update_vbat, "Update vbat if set else return powerup value");

module_param(fix_pwm_polarity, bool, 0);
MODULE_PARM_DESC(fix_pwm_polarity,
		 "Force PWM polarity to active high (DANGEROUS)");

MODULE_LICENSE("GPL");
MODULE_VERSION(IT87_DRIVER_VERSION);

module_init(sm_it87_init);
module_exit(sm_it87_exit);
