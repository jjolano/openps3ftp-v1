#pragma once

#include <psl1ght/lv2.h>

// credit to PS3MrEnigma for device list and filesystem types

// "generic" devices
#define DEV_FLASH	"CELL_FS_IOS:BUILTIN_FLASH"		// generic dev_flash
#define DEV_USB		"CELL_FS_IOS:USB_MASS_STORAGE"		// generic dev_usb
#define DEV_HDD		"CELL_FS_UTILITY:HDD"			// generic dev_hdd

// known devices
#define DEV_FLASH1	"CELL_FS_IOS:BUILTIN_FLSH1"		// dev_flash
#define DEV_FLASH2	"CELL_FS_IOS:BUILTIN_FLSH2"		// dev_flash2
#define DEV_FLASH3	"CELL_FS_IOS:BUILTIN_FLSH3"		// dev_flash3
#define DEV_BDVD	"CELL_FS_IOS:BDVD_DRIVE"		// dev_bdvd
#define DEV_USB0	"CELL_FS_IOS:USB_MASS_STORAGE000"	// dev_usb000
#define DEV_USB1	"CELL_FS_IOS:USB_MASS_STORAGE001"	// dev_usb001
#define DEV_USB2	"CELL_FS_IOS:USB_MASS_STORAGE002"	// dev_usb002
#define DEV_USB3	"CELL_FS_IOS:USB_MASS_STORAGE003"	// dev_usb003
#define DEV_USB4	"CELL_FS_IOS:USB_MASS_STORAGE004"	// dev_usb004
#define DEV_USB5	"CELL_FS_IOS:USB_MASS_STORAGE005"	// dev_usb005
#define DEV_USB6	"CELL_FS_IOS:USB_MASS_STORAGE006"	// dev_usb006
#define DEV_USB7	"CELL_FS_IOS:USB_MASS_STORAGE007"	// dev_usb007
#define DEV_HDD0	"CELL_FS_UTILITY:HDD0"			// dev_hdd0
#define DEV_HDD1	"CELL_FS_UTILITY:HDD1"			// dev_hdd1
#define DEV_DUMMY	"CELL_FS_DUMMY"				// app_home?
#define DEV_HOSTFS	"CELL_FS_HOSTFS"			// host_root and app_home
#define DEV_ADMINFS	"CELL_FS_ADMINFS"			// app_home?

// devices for older (non-slim) models
#define DEV_HDD_P0	"CELL_FS_IOS:PATA0_HDD_DRIVE"		// maybe dev_hdd0
#define DEV_HDD_P1	"CELL_FS_IOS:PATA1_HDD_DRIVE"		// maybe dev_hdd1
#define DEV_BDVD_P0	"CELL_FS_IOS:PATA0_BDVD_DRIVE"		// dev_bdvd
#define DEV_BDVD_P1	"CELL_FS_IOS:PATA1_BDVD_DRIVE"		// dev_bdvd
#define DEV_CF		"CELL_FS_IOS:COMPACT_FLASH"		// dev_cf
#define DEV_MS		"CELL_FS_IOS:MEMORY_STICK"		// dev_ms
#define DEV_SD		"CELL_FS_IOS:SD_CARD"			// dev_sd

// unknown devices
#define DEV_FLASH4	"CELL_FS_IOS:BUILTIN_FLSH4"		// dev_flash4, unknown
#define DEV_HDD2	"CELL_FS_UTILITY:HDD2"			// dev_hdd2, unknown
#define DEV_PSEUDO	"CELL_FS_PSEUDO"			// unknown

// filesystem types
#define FS_DUMMYFS	"CELL_FS_DUMMYFS"			// for CELL_FS_DUMMY
#define FS_ADMINFS	"CELL_FS_ADMINFS"			// for CELL_FS_HOSTFS
#define FS_FAT32	"CELL_FS_FAT"				// for CELL_FS_IOS:BUILTIN_FLSH*, CELL_FS_IOS:USB_MASS_STORAGE***, and maybe more
#define FS_SIMPLEFS	"CELL_FS_SIMPLEFS"			// internally used for checking if drive is formattable
#define FS_UFS		"CELL_FS_UFS"				// for CELL_FS_UTILITY:HDD*
#define FS_UDF		"CELL_FS_UDF"				// for CELL_FS_IOS:BDVD_DRIVE
#define FS_ISO9660	"CELL_FS_ISO9660"			// maybe dev_bdvd
#define FS_EFAT		"CELL_FS_EFAT"				// unknown
#define FS_PFAT		"CELL_FS_PFAT"				// unknown

LV2_SYSCALL lv2FsMount(const char* name, const char* fs, const char* path, int readonly) { return Lv2Syscall8(837, (u64)name, (u64)fs, (u64)path, 0, readonly, 0, 0, 0); }
LV2_SYSCALL lv2FsUnmount(const char* path) { return Lv2Syscall1(838, (u64)path); }

