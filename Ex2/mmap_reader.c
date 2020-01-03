#define _POSIX_SOURCE
#include <errno.h> // errno
#include <fcntl.h> // O_RDONLY, open
//#include <limits.h>
#include <signal.h> // SIG_IGN, SIGUSR1, SIGTERM, struct sigaction, sigaction, sigemptyset
#include <stdio.h> // printf, fprintf, snprintf, stdout, stderr, fflush, perror
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <string.h> // strlen, strerror, memset
#include <sys/stat.h> // stat
#include <sys/time.h> // gettimeofday, struct timeval
#include <unistd.h> // sleep, close, unlink
#include <sys/mman.h> // PROT_READ, MAP_SHARED, MAP_FAILED, mmap, munmap

#define TMP_FOLDER "./tmp"
#define FILE_NAME "mmapped.bin"

// Define printing strings
#define BENCHMARK_MSG			"%d were read in %f milliseconds through MMAP\n"
#define OPERANDS_SURPLUS_MSG		"Too many operands\nUsage: %s\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define ERROR_SIGACTION_INIT_MSG	"[Error] Failed to init signal handler (sigaction).\n"
#define ERROR_SIGACTION_RESTORE_MSG	"[Error] Failed to restore signal handler (sigaction).\n"
#define F_ERROR_CLOSE_MMAP_MSG		"[Error] Close mmap file '%s': %s\n"
#define F_ERROR_INVALID_CHAR_MSG	"[Error] Got invalid char: %d\n"
#define F_ERROR_OPEN_MMAP_MSG		"[Error] Open mmap file '%s': %s\n"
#define F_ERROR_STAT_MSG		"[Error] Getting information for file '%s': %s\n"
#define F_ERROR_UNLINK_FAILED_MSG	"[Error] Failed to delete file '%s': %s\n"
#define P_ERROR_MAPPING_MSG		"[Error] Error mmapping the file"
#define P_ERROR_UNMMAPPING_MSG		"[Error] Error un-mmapping the file"

int exit_with_error = 0; // Init to 'EXIT_SUCCESS'
int finish_loop_and_exit = 1; // Init to stay in the infinity loop
struct sigaction sigterm_old_handler;
struct sigaction sigusr1_old_handler;

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
	if ((0 < strlen(location))&&(unlink(location) == -1)) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_UNLINK_FAILED_MSG,location,strerror(errno));
		res = errno;
	}
	if ((sigaction(SIGTERM,&sigterm_old_handler,NULL) == -1) || (sigaction(SIGUSR1,&sigusr1_old_handler,NULL) == -1)) { // Returns 0 on success and -1 on error.
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
	finish_loop_and_exit = 0; // Exit the infinity loop
	exit_with_error = res; // Set the exit code
	return res;
}
void sigusr1_handler(int signum) {
	// General variable
	char *arr;
	char mmap_location[strlen(TMP_FOLDER)+strlen(FILE_NAME)+2]; // The location of the mmap file in the file system
	double elapsed_time;
	int i;
	int char_count = 0;
	int mmap_fd = -1; // The descriptor of the mmap file
	int mmap_size;
	struct timeval t_start,t_end;
	struct stat mmap_stat;
	// Upon receiving a SIGUSR1 signal:
	if (signum == SIGUSR1) { // just to be one the safe side
		snprintf(mmap_location,sizeof(mmap_location),"%s/%s",TMP_FOLDER,FILE_NAME); // Set 'mmap_location' to be the path to the communication file
		// 1. Open the file /tmp/mmapped.bin
		if ((mmap_fd = open(mmap_location,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
			fprintf(stderr,F_ERROR_OPEN_MMAP_MSG,mmap_location,strerror(errno)); // No need to call fflush(stderr);
			program_end(errno,mmap_fd,mmap_location,0,""); // Unmap, close and unlink mmap file & Restore signal handler.
			return; // EXIT_FAILURE
		}
		// 2. Determine the file size (man 2 lseek, man 2 stat)
		if (stat(mmap_location, &mmap_stat) == -1) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_STAT_MSG,mmap_location,strerror(errno));
			program_end(errno,mmap_fd,mmap_location,0,""); // Unmap, close and unlink mmap file & Restore signal handler.
			return; // EXIT_FAILURE
		}
		mmap_size = mmap_stat.st_size;
		// 3. Start the time measurement
		gettimeofday(&t_start,NULL);
		// 4. Create a memory map for the file
		if ((arr = (char*) mmap(NULL,mmap_size,PROT_READ,MAP_SHARED,mmap_fd,0)) == MAP_FAILED) { // On success, mmap() returns a pointer to the mapped area. On error, the value MAP_FAILED ... is returned, and errno is set appropriately.
			perror(P_ERROR_MAPPING_MSG);
			program_end(errno,mmap_fd,mmap_location,mmap_size,arr); // Unmap, close and unlink mmap file & Restore signal handler.
			return; // EXIT_FAILURE
		}
		// 5. Count the number of 'a' bytes in the array until the first NULL ('\0')
		for (i = 0; i < mmap_size; i++) {
			if (arr[i] == 'a') {
				char_count +=1;
			} else if (arr[i] == '\0') {
				char_count +=1;
				break;
			} else {
				fprintf(stderr,F_ERROR_INVALID_CHAR_MSG,arr[i]);
				program_end(-1,mmap_fd,mmap_location,mmap_size,arr); // Unmap, close and unlink mmap file & Restore signal handler.
				return; // EXIT_FAILURE
			}
		}
		// 6. Finish the time measurement
		gettimeofday(&t_end,NULL);
		elapsed_time = ((t_end.tv_sec-t_start.tv_sec)*1000.0) + ((t_end.tv_usec-t_start.tv_usec)/1000.0);
		// 7. Print the measurement result along with the number of bytes counted
		printf(BENCHMARK_MSG,char_count,elapsed_time);
		fflush(stdout);
		// 8. Remove the file from the disk (man 2 unlink)
		// 9. Cleanup. Exit gracefully (man 3 exit)
		program_end(0,mmap_fd,mmap_location,mmap_size,arr); // Unmap, close and unlink mmap file & Restore signal handler.
		return; // EXIT_SUCCESS
	}
}
int main(int argc, char *argv[]) {
	// General variable
	int sig_creation_count;
	struct sigaction sigterm_new_handler;
	struct sigaction sigusr1_new_handler;
	// 1. Register a signal handler for SIGUSR1 (man 2 sigaction)
	//memset(&sigterm_new_handler, 0, sizeof(sigterm_new_handler)); // We were not allowed to use memset,
	//memset(&sigusr1_new_handler, 0, sizeof(sigusr1_new_handler)); // We were not allowed to use memset,
	sigemptyset(&sigterm_new_handler.sa_mask);
	sigemptyset(&sigusr1_new_handler.sa_mask);
	sigterm_new_handler.sa_handler = SIG_IGN;
	sigusr1_new_handler.sa_handler = sigusr1_handler;
	sig_creation_count =  sigaction(SIGTERM,&sigterm_new_handler,&sigterm_old_handler); // Returns 0 on success and -1 on error.
	sig_creation_count += sigaction(SIGUSR1,&sigusr1_new_handler,&sigusr1_old_handler); // Returns 0 on success and -1 on error.
	if (sig_creation_count < 0) { // if (sig_creation_count == -1) or (sig_creation_count == -2)
		fprintf(stderr,ERROR_SIGACTION_INIT_MSG);
		fprintf(stderr,ERROR_EXIT_MSG);
		fflush(stderr);
		return (EXIT_FAILURE);
	}
	// Check correct call structure
	if (argc != 1) {
		printf(OPERANDS_SURPLUS_MSG,argv[0]);
		fflush(stdout);
		return (program_end(-1,-1,"",0,"")); // Unmap, close and unlink mmap file & Restore signal handler.
	}
	// 2. Enter an infinite loop, sleeping 2 seconds in each iteration
	while (finish_loop_and_exit) {
		sleep(2);
	}
	return exit_with_error;
}
