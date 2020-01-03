#define _GNU_SOURCE
#include "kci.h"

#include <fcntl.h>		// open
#include <sys/ioctl.h>		// ioctl
#include <errno.h>		// ERANGE, errno
#include <stdio.h>		// printf, printf, fprintf, sprintf, stderr, sscanf
#include <stdlib.h>		// EXIT_FAILURE, EXIT_SUCCESS, strtol
#include <string.h>		// strlen, strcmp, strerror, memset
#include <unistd.h>		// lseek, close, read, write, exit
#include <limits.h>		// LONG_MAX, LONG_MIN
#include <sys/syscall.h>	// 
#include <sys/types.h>		// 
#include <sys/stat.h>		// 

int private_init(char *KO) {
	// Receives a path to a kernel object (.ko) file KO, and inserts the relevant kernel module into the kernel space, as well as creating a device file for it.
	// init_module (man 2) – installs the kernel module, equivalent to insmod.
	// Use the finit_module variant of this system call, passing "" and 0 as the 2 nd and 3 rd arguments.
	int ko_fd;
	if ((ko_fd = open(KO,O_RDONLY | O_CLOEXEC)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,KO,strerror(errno));
		return EXIT_FAILURE;
	}
	if (syscall(__NR_finit_module,ko_fd,"",0) == -1) { // The return value is defined by the system call being invoked. In general, a 0 return value indicates success. A -1 return value indicates an error, and an error code is stored in errno.
		fprintf(stderr,"[Error] finit_module() failed: %s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	// mknod (man 2) – creates a filesystem node so we can open it and issue ioctl commands to the module, equivalent to mknod. Use a constant path for the node (/dev/kci_dev).
	// makedev (man 3) – receives a major and minor number, and returns a dev_t to use for the mknod call. You may assume only a single device exists and thus use 0 as the minor number.
	if (mknod(CONST_DEVICE_FILE_PATH,S_IFCHR|0666,makedev(MAJOR_NUM,0))) { //  returns zero on success, or -1 if an error occurred (in which case, errno is set appropriately).
		fprintf(stderr,"[Error] mknod() failed:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	if (close(ko_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,"[Error] Close ko_fd failed: %s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int private_pid(char *PID) { // Set the PID (assume positive integer) of the process the cipher works for. Send ioctl command IOCTL_SET_PID to the kernel module, providing the PID as an argument.
	// Function variables
	int device_fd;
	int input_pid = 0;		// The PID of the process the cipher works for
	char* endptr_PID;		// strtol() for 'input_pid'
	// Convert string to int
	input_pid = strtol(PID,&endptr_PID,10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (input_pid == (int)LONG_MAX || input_pid == (int)LONG_MIN)) || (errno != 0 && input_pid == 0)) {
		fprintf(stderr,F_ERROR_FUNCTION_STRTOL_MSG,strerror(errno));
		return errno;
	} else if (endptr_PID == PID) { // Empty string (assume positive integer)
		printf(PID_INVALID_MSG);
		return EXIT_FAILURE;
	}
	// Send ioctl command
	if ((device_fd = open(CONST_DEVICE_FILE_PATH,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,CONST_DEVICE_FILE_PATH,strerror(errno));
		return EXIT_FAILURE;
	}
	if ((ioctl(device_fd,IOCTL_SET_PID,input_pid)) == -1) { // On success zero is returned. A few ioctl() requests use the return value as an output parameter and return a nonnegative value on success. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,"IOCTL_SET_PID failed:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	if (close(device_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_CLOSE_MSG,strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int private_fd(char *FD) { // Set the file descriptor (assume positive integer) of the file the cipher works for. Send ioctl command IOCTL_SET_FD to the kernel module, providing the FD as an argument. You can obtain the FD number checking /proc/${PID}/fd directory, or using lsof utility.
	// Function variables
	int device_fd;
	int input_fd = 0;		// The file descriptor of the file the cipher works for
	char* endptr_FD;		// strtol() for 'input_fd'
	// Convert string to int
	input_fd = strtol(FD,&endptr_FD,10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (input_fd == (int)LONG_MAX || input_fd == (int)LONG_MIN)) || (errno != 0 && input_fd == 0)) {
		fprintf(stderr,F_ERROR_FUNCTION_STRTOL_MSG,strerror(errno));
		return errno;
	} else if (endptr_FD == FD) { // Empty string (assume positive integer)
		printf(FD_INVALID_MSG);
		return EXIT_FAILURE;
	}
	// Send ioctl command
	if ((device_fd = open(CONST_DEVICE_FILE_PATH,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,CONST_DEVICE_FILE_PATH,strerror(errno));
		return EXIT_FAILURE;
	}
	if ((ioctl(device_fd,IOCTL_SET_FD,input_fd)) == -1) { // On success zero is returned. A few ioctl() requests use the return value as an output parameter and return a nonnegative value on success. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,"IOCTL_SET_FD failed:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	if (close(device_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_CLOSE_MSG,strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int private_start(void) { // Start actual encryption/decryption. Send IOCTL_CIPHER command to the kernel module, and 1 as the argument.
	int device_fd;
	if ((device_fd = open(CONST_DEVICE_FILE_PATH,O_RDWR)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,CONST_DEVICE_FILE_PATH,strerror(errno));
		return EXIT_FAILURE;
	}
	if ((ioctl(device_fd,IOCTL_CIPHER,1)) == -1) { // On success zero is returned. A few ioctl() requests use the return value as an output parameter and return a nonnegative value on success. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,"IOCTL_CIPHER start failed:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	if (close(device_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_CLOSE_MSG,strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int private_stop(void) { // Stop actual encryption/decryption. Send IOCTL_CIPHER command to the kernel module, and 0 as the argument.
	int device_fd;
	if ((device_fd = open(CONST_DEVICE_FILE_PATH,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,CONST_DEVICE_FILE_PATH,strerror(errno));
		return EXIT_FAILURE;
	}
	if ((ioctl(device_fd,IOCTL_CIPHER,0)) == -1) { // On success zero is returned. A few ioctl() requests use the return value as an output parameter and return a nonnegative value on success. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,"IOCTL_CIPHER stop failed:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	if (close(device_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_CLOSE_MSG,strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int private_rm(void) {
	int source_fd;			// Input file descriptors
	int  destination_fd;		// Output file descriptors
	ssize_t ret_in;			// Number of bytes returned by read()
	char buffer[BUF_LEN];		// Character buffer
	char log_old_path[strlen(CONST_DEBUGFS_PATH)+strlen(CONST_DEBUGFS_FOLDER)+strlen(CONST_DEBUGFS_FILE)+2];
	char log_new_path[strlen(CONST_DEBUGFS_FILE)+3];

	// It copies the log file (under /sys/kernel/debug/kcikmod/calls) into a file in the current directory with the same name (calls).
	if (sprintf(log_old_path,"%s%s/%s",CONST_DEBUGFS_PATH,CONST_DEBUGFS_FOLDER,CONST_DEBUGFS_FILE) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
		fprintf(stderr,F_ERROR_FUNCTION_SPRINTF_MSG);
		return EXIT_FAILURE;
	}
	if (sprintf(log_new_path,"./%s",CONST_DEBUGFS_FILE) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
		fprintf(stderr,F_ERROR_FUNCTION_SPRINTF_MSG);
		return EXIT_FAILURE;
	}
	if ((source_fd = open(log_old_path,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,log_old_path,strerror(errno));
		return EXIT_FAILURE;
	}
	if ((destination_fd = open(log_new_path,O_WRONLY|O_CREAT,0666)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_OPEN_MSG,log_new_path,strerror(errno));
		return EXIT_FAILURE;
	}
	while ((ret_in = read(source_fd,&buffer,BUF_LEN)) > 0){ // Upon successful completion, ... return a non-negative integer .... Otherwise, the functions shall return -1 and set errno to indicate the error.
		if (write(destination_fd,&buffer,(ssize_t)ret_in) != ret_in) { // Upon successful ... return the number of bytes actually written .... Otherwise, -1 shall be returned and errno set to indicate the error.
			fprintf(stderr,"[Error] Copy log file to '%s': %s\n",log_new_path,strerror(errno));
			return EXIT_FAILURE;
		}
	}
	if (close(destination_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_CLOSE_MSG,strerror(errno));
		return EXIT_FAILURE;
	}
	if (close(source_fd) == -1) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_DEVICE_CLOSE_MSG,strerror(errno));
		return EXIT_FAILURE;
	}
	// It removes the device file created by init.
	if (unlink(CONST_DEVICE_FILE_PATH) == -1) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,"[Error] unlink device:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	// Remove the kernel module from the kernel space (man 2 delete_module).
	// delete_module (man 2) – removes the kernel module, equivalent to rmmod. This call doesn't have a system call wrapper as well! Use the following function call instead:
	if (syscall(__NR_delete_module,"kci_kmod",O_NONBLOCK) == -1) { // The return value is defined by the system call being invoked. In general, a 0 return value indicates success. A -1 return value indicates an error, and an error code is stored in errno.
		fprintf(stderr,"[Error] delete_module() failed:%s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
int main(int argc, char *argv[]) {
	// Function variables
	int optind;
	int res = 0;
	// Check correct call structure
	for (optind=1;optind<argc;optind++) { // Foreach argument
		if (argv[optind][0] == '-') { // If he start with '-'
			if (strcmp(argv[optind],"-init") == 0) {
				res += private_init(argv[optind+1]); // Command may require further arguments (which you may assume are provided)
			} else if (strcmp(argv[optind],"-pid") == 0) {
				res += private_pid(argv[optind+1]); // Command may require further arguments (which you may assume are provided)
			} else if (strcmp(argv[optind],"-fd") == 0) {
				res += private_fd(argv[optind+1]); // Command may require further arguments (which you may assume are provided)
			} else if (strcmp(argv[optind],"-start") == 0) {
				res += private_start();
			} else if (strcmp(argv[optind],"-stop") == 0) {
				res += private_stop();
			} else if (strcmp(argv[optind],"-rm") == 0) {
				res += private_rm();
			} else if (strcmp(argv[optind],"--help") == 0) {
				if (argc == 2) { // ./kci_ctrl --help
					printf(HELP_MSG,argv[0]);
					return EXIT_FAILURE; // If and only if there are no other commands!!!
				} else { // If there are more commands do not print help message
					fprintf(stderr, "Invalid call for help\n");
				}
			} else {
				if (argc == 2) { // Print this message only if there are no other commands
					fprintf(stderr, "%s: invalid option -- '%s'\nTry '%s --help' for more information.\n",argv[0],argv[optind],argv[0]);
					return EXIT_FAILURE; // If and only if there are no other commands!!!
				} // else - Ignore that command
			}
		}
	}
	if (res == 0) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}
