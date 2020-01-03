#define _POSIX_SOURCE
#include <errno.h> // errno
#include <fcntl.h> // O_RDWR, O_CREAT, O_TRUNC, open
#include <limits.h> // LONG_MAX, LONG_MIN
#include <signal.h> // SIG_IGN, SIGUSR1, SIGTERM, struct sigaction, sigaction, sigemptyset, kill
#include <stdio.h> // printf, fprintf, snprintf, stdout, stderr, fflush, perror
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS, strtol
#include <string.h> // strlen, strerror, memset
#include <sys/stat.h> // chmod
#include <sys/time.h> // gettimeofday, struct timeval
#include <unistd.h> // EACCES, ENOENT, R_OK, W_OK, lseek, write, close, access
#include <sys/mman.h> // PROT_WRITE, MAP_SHARED, MAP_FAILED, MS_SYNC, mmap, munmap, msync

#define TMP_FOLDER "./tmp"
#define FILE_NAME "mmapped.bin"

// Define printing strings
#define BENCHMARK_MSG			"%d were written in %f milliseconds through MMAP\n"
#define NUM_LESS_THEN_TWO_MSG		"The input value '%d' is too small\n"
#define OPERANDS_MISSING_MSG		"Missing operands\nUsage: %s <NUM> <RPID>\n"
#define OPERANDS_SURPLUS_MSG		"Too many operands\nUsage: %s <NUM> <RPID>\n"
#define PID_INVALID_MSG			"The process id '%d' is invalid\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define ERROR_SIGACTION_INIT_MSG	"[Error] Failed to init signal handler (sigaction).\n"
#define ERROR_SIGACTION_RESTORE_MSG	"[Error] Failed to restore signal handler (sigaction).\n"
#define F_ERROR_ACCESS_FILE_MSG		"[Error] Accesee FIFO file '%s': %s\n"
#define F_ERROR_CHMOD_FILE_MSG		"[Error] Chmod FIFO file '%s': %s\n"
#define F_ERROR_CLOSE_MMAP_MSG		"[Error] Close mmap file '%s': %s\n"
#define F_ERROR_OPEN_MMAP_MSG		"[Error] Open mmap file '%s': %s\n"
#define P_ERROR_LSEAK_MSG		"[Error] Calling lseek() to 'stretch' the file"
#define P_ERROR_MMAPPING_MSG		"[Error] Mmapping the file"
#define P_ERROR_MSYNC_MSG		"[Error] Msync failed with error"
#define P_ERROR_STRTOL_MSG		"[Error] Strtol failed with error"
#define P_ERROR_UNMMAPPING_MSG		"[Error] Un-mmapping the file"
#define P_ERROR_WRITE_LAST_BYTE_MSG	"[Error] Writing last byte of the file"

struct sigaction sigterm_old_handler;

// This file is based on:
// 1) File name 'memory mapped file demo' on the course module site. (http://moodle.tau.ac.il/course/view.php?id=368216201)
// 2) Youtube video: https://www.youtube.com/watch?v=F3z-SIxu1Tw
int program_end(int error, int fd, char *location, int mmap_size, char *arr) {
	int res = 0;
	if ((0 < mmap_size)&&(munmap(arr,mmap_size) == -1)) { // returns 0, on failure -1, and errno is set (probably to EINVAL)
		perror(P_ERROR_UNMMAPPING_MSG);
		res = errno;
	}
	if ((0 < fd)&&(close(fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_CLOSE_MMAP_MSG,location,strerror(errno));
		res = errno;
	}
	if (sigaction(SIGTERM,&sigterm_old_handler,NULL) == -1) { // Returns 0 on success and -1 on error.
		fprintf(stderr,ERROR_SIGACTION_RESTORE_MSG);
		res = -1;
	}
	if ((error != 0)||(res != 0)) {
		fprintf(stderr,ERROR_EXIT_MSG);
		if (error != 0) { // If multiple error occurred, Print the error that called 'program_end' function.
			res = error;
		}
	}
	fflush(stderr);
	return res;
}
int main(int argc, char *argv[]) {
	// General variable
	char *arr;
	char *endptr; // strtol var
	char mmap_location[strlen(TMP_FOLDER)+strlen(FILE_NAME)+2]; // The location of the mmap file in the file system
	mmap_location[0] = '\0';
	double elapsed_time;
	int i;
	int mmap_fd = -1; // The descriptor of the mmap file
	long mmap_size;
	long reader_pid;
	struct timeval t_start,t_end;
	struct sigaction sigterm_new_handler;
	// Create signal handlers
	//memset(&sigterm_new_handler, 0, sizeof(sigterm_new_handler)); // We were not allowed to use memset,
	sigemptyset(&sigterm_new_handler.sa_mask);
	sigterm_new_handler.sa_handler = SIG_IGN;
	if (sigaction(SIGTERM,&sigterm_new_handler,&sigterm_old_handler) < 0) { // Returns 0 on success and -1 on error.
		fprintf(stderr,ERROR_SIGACTION_INIT_MSG);
		fprintf(stderr,ERROR_EXIT_MSG);
		fflush(stderr);
		return (EXIT_FAILURE);
	}
	// Check correct call structure
	if (argc != 3) {
		if (argc < 3) {
			printf(OPERANDS_MISSING_MSG,argv[0]);
		} else {
			printf(OPERANDS_SURPLUS_MSG,argv[0]);
		}
		fflush(stdout);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	mmap_size = strtol(argv[1], &endptr, 10); // If an underflow occurs. strtol() returns LONG_MIN.  If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (mmap_size == LONG_MAX || mmap_size == LONG_MIN)) || (errno != 0 && mmap_size == 0)) {
		perror("P_ERROR_STRTOL_MSG");
		return (program_end(errno,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	if (endptr == argv[1]) { // Empty string
		printf(OPERANDS_MISSING_MSG,argv[0]);
		fflush(stdout);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	if (mmap_size < 2) {
		printf(NUM_LESS_THEN_TWO_MSG,mmap_size);
		fflush(stdout);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	reader_pid = strtol(argv[2], &endptr, 10); // If an underflow occurs. strtol() returns LONG_MIN.  If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (reader_pid == LONG_MAX || reader_pid == LONG_MIN)) || (errno != 0 && reader_pid == 0)) {
		perror("P_ERROR_STRTOL_MSG");
		return (program_end(errno,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	if (endptr == argv[2]) { // Empty string
		printf(OPERANDS_MISSING_MSG,argv[0]);
		fflush(stdout);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	if (reader_pid < 1) {
		printf(PID_INVALID_MSG,reader_pid);
		fflush(stdout);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	// 1. Create a file under the /tmp directory named mmapped.bin
	// 2. Set the file permissions to 0600 (man 2 chmod)
	snprintf(mmap_location,sizeof(mmap_location),"%s/%s",TMP_FOLDER,FILE_NAME); // Set 'mmap_location' to be the path to the communication file
	if (access(mmap_location,R_OK | W_OK) == -1) {// On success ..., zero is returned. On error ..., -1 is returned, and errno is set appropriately.
		if (errno == EACCES) { // The requested access would be denied to the file, or search permission is denied for one of the directories in the path prefix of pathname.
			if (chmod(mmap_location, 0600) == -1) { // On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
				fprintf(stderr,F_ERROR_CHMOD_FILE_MSG,mmap_location,strerror(errno)); // No need to call fflush(stderr);
				return (program_end(errno,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
			}
		} else if (errno != ENOENT) { // A component of pathname does not exist or is a dangling symbolic link.
			fprintf(stderr,F_ERROR_ACCESS_FILE_MSG,mmap_location,strerror(errno)); // No need to call fflush(stderr);
			return (program_end(errno,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
		}
	}
	if ((mmap_fd = open(mmap_location,O_RDWR | O_CREAT | O_TRUNC,0600)) == -1) { // Upon successful completion ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error....
		fprintf(stderr,F_ERROR_OPEN_MMAP_MSG,mmap_location,strerror(errno)); // No need to call fflush(stderr);
		return (program_end(errno,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	if (lseek(mmap_fd,mmap_size-1,SEEK_SET) != (mmap_size-1)) { // Change file size to be NUM (argv[1])
		perror(P_ERROR_LSEAK_MSG);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	if (write(mmap_fd,"",1) != 1) {
		perror(P_ERROR_WRITE_LAST_BYTE_MSG);
		return (program_end(-1,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	// 3. Create a memory map for the file
	if ((arr = (char*) mmap(NULL,mmap_size,PROT_WRITE,MAP_SHARED,mmap_fd,0)) == MAP_FAILED) { // On success, mmap() returns a pointer to the mapped area. On error, the value MAP_FAILED ... is returned, and errno is set appropriately.
		perror(P_ERROR_MMAPPING_MSG);
		return (program_end(errno,mmap_fd,mmap_location,0,"")); // Unmap and close mmap file & Restore signal handler.
	}
	// 4. Start the time measurement
	gettimeofday(&t_start,NULL);
	// 5. Fill the array with NUM-1 sequential 'a' bytes and then NULL (i.e., '\0')
	for (i = 0; i < mmap_size-1; i++) { // Write to the file
		arr[i] = 'a';
	}
	arr[mmap_size-1] = '\0';
	if (msync(arr,mmap_size,MS_SYNC) == -1) { // On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
		perror(P_ERROR_MSYNC_MSG);
		return (program_end(errno,mmap_fd,mmap_location,mmap_size,arr)); // Unmap and close mmap file & Restore signal handler.
	}
	// 6. Send a signal (SIGUSR1) to the reader process (man 2 kill)
	kill(reader_pid, SIGUSR1);
	// 7. Print the measurement result together with the number of bytes written
	gettimeofday(&t_end,NULL);
	elapsed_time = ((t_end.tv_sec-t_start.tv_sec)*1000.0) + ((t_end.tv_usec-t_start.tv_usec)/1000.0);
	printf(BENCHMARK_MSG,mmap_size,elapsed_time);
	fflush(stdout);
	// 8. Cleanup. Exit gracefully
	return (program_end(0,mmap_fd,mmap_location,mmap_size,arr)); // Unmap and close mmap file & Restore signal handler.
}
