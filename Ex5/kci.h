#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

// The major device number. We can't rely on dynamic registration any more, because ioctls need to know it.
#define MAJOR_NUM			245

// Set the message of the device driver
#define IOCTL_SET_PID			_IOW(MAJOR_NUM,0,unsigned long)	// Updates the PID variable to the given argument. long
#define IOCTL_SET_FD			_IOW(MAJOR_NUM,1,unsigned long)	// Updates the FD variable to the given argument.
#define IOCTL_CIPHER			_IOW(MAJOR_NUM,2,unsigned long)	// Sets the cipher flag to the given argument.
#define DEVICE_RANGE_NAME		"kci_dev"			// Registered device name, Can be seen with the command "cat /proc/devices"
#define BUF_LEN				8192				// 8KB buffer
#define SUCCESS				0				// Success code for the kernel module
#define CRO_WP				0x00010000			// Write Protect Bit

// Program constant variables
#define CONST_DEVICE_FILE_PATH		"/dev/kci_dev"
#define CONST_DEBUGFS_PATH		"/sys/kernel/debug/"		// The path to the log file is splited into 3 variables:
#define CONST_DEBUGFS_FOLDER		"kcikmod"			// CONST_DEBUGFS_PATH + CONST_DEBUGFS_FOLDER + '/' + CONST_DEBUGFS_FILE
#define CONST_DEBUGFS_FILE		"calls"				// /sys/kernel/debug/kcikmod/calls

// Define printing strings
#define LOG_READ_MSG			"PID:%ld FD:%ld, Successfully read %ld out of %d bytes."
#define LOG_WRITE_MSG			"PID:%ld FD:%ld, Successfully write %ld out of %d bytes."
#define FD_INVALID_MSG			"The file descriptor must be a positive integer\n"
#define PID_INVALID_MSG			"The process ID must be a positive integer\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define F_ERROR_FUNCTION_SPRINTF_MSG	"[Error] sprintf() failed with an error\n"
#define F_ERROR_FUNCTION_STRTOL_MSG	"[Error] strtol() failed with an error: %s\n"
#define F_ERROR_DEVICE_CLOSE_MSG	"[Error] Close device file: %s\n"
#define F_ERROR_DEVICE_OPEN_MSG		"[Error] Could not open device file '%s': %s\n"

// Program help message
#define HELP_MSG	"Usage: %s [OPTION]...\n\
\n\
  -init KO                   Receives a path to a kernel object (.ko) file KO,\n\
                               and inserts the relevant kernel module into the\n\
                               kernel space, as well as creating a device file\n\
                               for it (check the appendix for further instructions).\n\
  -pid PID                   Set the PID (assume positive integer) of the process\n\
                               the cipher works for. Send ioctl command\n\
                               IOCTL_SET_PID to the kernel module, providing the\n\
                               PID as an argument.\n\
  -fd FD                     Set the file descriptor (assume positive integer)\n\
                               of the file the cipher works for. Send ioctl command\n\
                               IOCTL_SET_FD to the kernel module, providing the\n\
                               FD as an argument. You can obtain the FD number\n\
                               checking /proc/${PID}/fd directory, or using lsof\n\
                               utility.\n\
  -start                     Start actual encryption/decryption. Send IOCTL_CIPHER\n\
                               command to the kernel module, and 1 as the argument.\n\
  -stop                      Stop actual encryption/decryption. Send IOCTL_CIPHER\n\
                               command to the kernel module, and 0 as the argument.\n\
  -rm                        Remove the kernel module from the kernel space\n\
                               (man 2 delete_module). Also, before that, it copies\n\
                               the log file (under /sys/kernel/debug/kcikmod/calls\n\
                               into a file in the current directory with the same\n\
                               name (calls). Finally, it removes the device file\n\
                               created by init.\n\
  --help                     Display this help and exit\n"
#endif
