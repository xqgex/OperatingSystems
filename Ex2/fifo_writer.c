#define _POSIX_SOURCE
#include <errno.h> // errno
#include <fcntl.h> // O_WRONLY, open
#include <limits.h> // LONG_MAX, LONG_MIN
#include <signal.h> // SIG_IGN, SIGINT, SIGPIPE, struct sigaction, sigaction, sigemptyset
#include <stdio.h> // printf, fprintf, snprintf, stdout, stderr, fflush
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS, strtol, exit
#include <string.h> // strlen, strcpy, strcat, strerror, memset
#include <sys/stat.h> // mkfifo, chmod
#include <sys/time.h> // gettimeofday, struct timeval
#include <unistd.h> // EACCES, ENOENT, R_OK, W_OK, write, close, unlink, access
//#include <sys/mman.h>

#define TMP_FOLDER "./tmp"
#define FILE_NAME "osfifo"

#define MAX_BUF 4096

// Define printing strings
#define BENCHMARK_MSG			"%d were written in %f milliseconds through FIFO\n"
#define NUM_LESS_THEN_ONE_MSG		"The input value '%d' is too small\n"
#define OPERANDS_MISSING_MSG		"Missing operands\nUsage: %s <NUM>\n"
#define OPERANDS_SURPLUS_MSG		"Too many operands\nUsage: %s <NUM>\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define ERROR_SIGACTION_INIT_MSG	"[Error] Failed to init signal handler (sigaction).\n"
#define ERROR_SIGACTION_RESTORE_MSG	"[Error] Failed to restore signal handler (sigaction).\n"
#define ERROR_SIGPIPE_MSG		"[Error] There is no one on the other side of the pipe.\n"
#define F_ERROR_ACCESS_FILE_MSG		"[Error] Accesee FIFO file '%s': %s\n"
#define F_ERROR_CHMOD_FILE_MSG		"[Error] Chmod FIFO file '%s': %s\n"
#define F_ERROR_CLOSE_PIPE_MSG		"[Error] Close pipe file '%s': %s\n"
#define F_ERROR_MKFIFO_FILE_MSG		"[Error] Make a FIFO file '%s': %s\n"
#define F_ERROR_OPEN_PIPE_MSG		"[Error] Open pipe file '%s': %s\n"
#define F_ERROR_UNLINK_FAILED_MSG	"[Error] Failed to delete file '%s': %s\n"
#define F_ERROR_WRITE_PIPE_MSG		"[Error] Write to pipe file '%s': %s\n"

struct sigaction sigint_old_handler;
struct sigaction sigpipe_old_handler;

int program_end(int error, int fd, char *location) {
	int res = 0;
	if ((0 < fd)&&(close(fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_CLOSE_PIPE_MSG,location,strerror(errno));
		res = errno;
	}
	if ((0 < strlen(location))&&(unlink(location) == -1)) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_UNLINK_FAILED_MSG,location,strerror(errno));
		res = errno;
	}
	if ((sigaction(SIGINT,&sigint_old_handler,NULL) == -1) || (sigaction(SIGPIPE,&sigpipe_old_handler,NULL) == -1)) { // Returns 0 on success and -1 on error.
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
void sigpipe_handler(int signum) {
	char pipe_location[strlen(TMP_FOLDER)+strlen(FILE_NAME)+2]; // The location of the pipe file in the file system
	snprintf(pipe_location,sizeof(pipe_location),"%s/%s",TMP_FOLDER,FILE_NAME); // Set 'pipe_location' to be the path to the communication file
	fprintf(stderr,ERROR_SIGPIPE_MSG);
	program_end(-1,-1,pipe_location); // Close and unlink pipe file & Restore signal handler.
	exit(signum);
}
int main(int argc, char *argv[]) {
	// General variable
	char *endptr; // strtol var
	char buf[MAX_BUF+1] = "";
	char full_buf[MAX_BUF+1] = "";
	char pipe_location[strlen(TMP_FOLDER)+strlen(FILE_NAME)+2]; // The location of the pipe file in the file system
	pipe_location[0] = '\0';
	double elapsed_time;
	int i;
	int pipe_fd = -1; // The descriptor of the pipe file
	int remaining_data;
	int sig_creation_count;
	long pipe_size;
	struct timeval t_start,t_end;
	struct sigaction sigint_new_handler;
	struct sigaction sigpipe_new_handler;
	// Create signal handlers
	//memset(&sigint_new_handler, 0, sizeof(sigint_new_handler)); // We were not allowed to use memset,
	//memset(&sigpipe_new_handler, 0, sizeof(sigpipe_new_handler)); // We were not allowed to use memset,
	sigemptyset(&sigint_new_handler.sa_mask);
	sigemptyset(&sigpipe_new_handler.sa_mask);
	sigint_new_handler.sa_handler = SIG_IGN;
	sigpipe_new_handler.sa_handler = sigpipe_handler;
	sig_creation_count =  sigaction(SIGINT,&sigint_new_handler,&sigint_old_handler); // Returns 0 on success and -1 on error.
	sig_creation_count += sigaction(SIGPIPE,&sigpipe_new_handler,&sigpipe_old_handler); // Returns 0 on success and -1 on error.
	if (sig_creation_count < 0) {
		fprintf(stderr,ERROR_SIGACTION_INIT_MSG);
		fprintf(stderr,ERROR_EXIT_MSG);
		fflush(stderr);
		return (EXIT_FAILURE);
	}
	// Check correct call structure
	if (argc != 2) {
		if (argc < 2) {
			printf(OPERANDS_MISSING_MSG,argv[0]);
		} else {
			printf(OPERANDS_SURPLUS_MSG,argv[0]);
		}
		fflush(stdout);
		return (program_end(-1,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
	}
	pipe_size = strtol(argv[1], &endptr, 10); // If an underflow occurs. strtol() returns LONG_MIN.  If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (pipe_size == LONG_MAX || pipe_size == LONG_MIN)) || (errno != 0 && pipe_size == 0)) {
		perror("P_ERROR_STRTOL_MSG");
		return (program_end(errno,pipe_fd,pipe_location)); // Unmap and close mmap file & Restore signal handler.
	}
	if (endptr == argv[1]) { // Empty string
		printf(OPERANDS_MISSING_MSG,argv[0]);
		fflush(stdout);
		return (program_end(-1,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
	}
	if (pipe_size < 1) {
		printf(NUM_LESS_THEN_ONE_MSG,pipe_size);
		fflush(stdout);
		return (program_end(-1,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
	}
	// 1. Create a named pipe file (man 3 mkfifo) under /tmp/osfifo, with 0600 file permissions
	snprintf(pipe_location,sizeof(pipe_location),"%s/%s",TMP_FOLDER,FILE_NAME); // Set 'pipe_location' to be the path to the communication file
	if (access(pipe_location,R_OK | W_OK) == -1) {// On success ..., zero is returned. On error ..., -1 is returned, and errno is set appropriately.
		if (errno == ENOENT) { // A component of pathname does not exist or is a dangling symbolic link.
			if (mkfifo(pipe_location, 0600) == -1) { // On success mkfifo() returns 0. In the case of an error, -1 is returned (in which case, errno is set appropriately).
				fprintf(stderr,F_ERROR_MKFIFO_FILE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
				return (program_end(errno,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
			}
		} else if (errno == EACCES) { // The requested access would be denied to the file, or search permission is denied for one of the directories in the path prefix of pathname.
			if (chmod(pipe_location, 0600) == -1) { // On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
				fprintf(stderr,F_ERROR_CHMOD_FILE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
				return (program_end(errno,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
			}
		} else {
			fprintf(stderr,F_ERROR_ACCESS_FILE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
			return (program_end(errno,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
		}
	}
	// 2. Open the created file for writing
	if ((pipe_fd = open(pipe_location,O_WRONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_OPEN_PIPE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
		return (program_end(errno,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
	}
	// 3. Start the time measurement
	gettimeofday(&t_start,NULL);
	// 4. Write NUM 'a' bytes to this named pipe file
	remaining_data = pipe_size;
	strcpy(buf, "a");
	for (i = 1; i < (pipe_size%MAX_BUF); i++) {
		strcat(buf, "a");
	}
	if (MAX_BUF < pipe_size) {
		strcpy(full_buf, "a");
		for (i = 1; i < MAX_BUF; i++) {
			strcat(full_buf, "a");
		}
	}
	while (0 < remaining_data) {
		if (MAX_BUF <= remaining_data) {
			if (write(pipe_fd, full_buf, MAX_BUF) != MAX_BUF) { // Upon successful ... return the number of bytes actually written .... Otherwise, -1 shall be returned and errno set to indicate the error.
				fprintf(stderr,F_ERROR_WRITE_PIPE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
				return (program_end(errno,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
			}
			remaining_data -= MAX_BUF;
		} else {
			if (write(pipe_fd, buf, strlen(buf)) != (int)strlen(buf)) { // Upon successful ... return the number of bytes actually written .... Otherwise, -1 shall be returned and errno set to indicate the error.
				fprintf(stderr,F_ERROR_WRITE_PIPE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
				return (program_end(errno,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
			}
			remaining_data -= strlen(buf);	
		}
	}
	// 5. Finish the time measurement
	gettimeofday(&t_end,NULL);
	elapsed_time = ((t_end.tv_sec-t_start.tv_sec)*1000.0) + ((t_end.tv_usec-t_start.tv_usec)/1000.0);
	// 6. Print the measurement result along with the number of bytes written
	printf(BENCHMARK_MSG,pipe_size,elapsed_time);
	fflush(stdout);
	// 7. Remove the file from the disk (man 2 unlink)
	// 8. Cleanup. Exit gracefully
	return (program_end(0,pipe_fd,pipe_location)); // Close and unlink pipe file & Restore signal handler.
}
