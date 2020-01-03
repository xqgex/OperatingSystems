#define _POSIX_SOURCE
#include <errno.h> // errno
#include <fcntl.h> // O_RDONLY, open
//#include <limits.h>
#include <signal.h> // SIG_IGN, SIGINT, struct sigaction, sigaction, sigemptyset
#include <stdio.h> // printf, fprintf, snprintf, stdout, stderr, fflush
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <string.h> // strlen, strcpy, strcat, strerror, memset
//#include <sys/stat.h>
#include <sys/time.h> // gettimeofday, struct timeval
#include <unistd.h> // sleep, read, close
//#include <sys/mman.h>

#define TMP_FOLDER "./tmp"
#define FILE_NAME "osfifo"

#define MAX_BUF 4096

// Define printing strings
#define BENCHMARK_MSG			"%d were read in %f milliseconds through FIFO\n"
#define OPERANDS_SURPLUS_MSG		"Too many operands\nUsage: %s\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define ERROR_SIGACTION_INIT_MSG	"[Error] Failed to init signal handler (sigaction).\n"
#define ERROR_SIGACTION_RESTORE_MSG	"[Error] Failed to restore signal handler (sigaction).\n"
#define F_ERROR_CLOSE_PIPE_MSG		"[Error] Close pipe file '%s': %s\n"
#define F_ERROR_OPEN_PIPE_MSG		"[Error] Open pipe file '%s': %s\n"
#define F_ERROR_READ_PIPE_MSG		"[Error] Read from pipe file '%s': %s\n"

struct sigaction sigint_old_handler;

int program_end(int error, int fd, char *location) {
	int res = 0;
	if ((fd > 0)&&(close(fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_CLOSE_PIPE_MSG,location,strerror(errno));
		res = errno;
	}
	if (sigaction(SIGINT,&sigint_old_handler,NULL) == -1) { // Returns 0 on success and -1 on error.
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
	char buf[MAX_BUF+1];
	char pipe_location[strlen(TMP_FOLDER)+strlen(FILE_NAME)+2]; // The location of the pipe file in the file system
	double elapsed_time;
	int chars_read = 0;
	int open_delay_counter;
	int pipe_fd = -1; // The descriptor of the pipe file
	int pipe_size = 0;
	struct timeval t_start,t_end;
	struct sigaction sigint_new_handler;
	// Create signal handlers
	//memset(&sigint_new_handler, 0, sizeof(sigint_new_handler)); // We were not allowed to use memset,
	sigemptyset(&sigint_new_handler.sa_mask);
	sigint_new_handler.sa_handler = SIG_IGN;
	if (sigaction(SIGINT,&sigint_new_handler,&sigint_old_handler) == -1) { // Returns 0 on success and -1 on error.
		fprintf(stderr,ERROR_SIGACTION_INIT_MSG);
		fprintf(stderr,ERROR_EXIT_MSG);
		fflush(stderr);
		return (EXIT_FAILURE);
	}
	// Check correct call structure
	if (argc != 1) {
		printf(OPERANDS_SURPLUS_MSG,argv[0]);
		fflush(stdout);
		return (program_end(-1,pipe_fd,pipe_location)); // Close pipe file & Restore signal handler.
	}
	// 1. Open /tmp/osfifo for reading
	snprintf(pipe_location,sizeof(pipe_location),"%s/%s",TMP_FOLDER,FILE_NAME); // Set 'pipe_location' to be the path to the communication file
	open_delay_counter = 0;
	do {
		if ((pipe_fd = open(pipe_location,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
			if (errno == 2) { // No such file or directory
				sleep(1); // Wait for the writer to finish init (Only once), sleep 1 sec
				open_delay_counter += 1;
			} else { // Another error (not 'No such file or directory')
				open_delay_counter = 2;
			}
		} else {
			open_delay_counter = 3; // Exit while loop
		}
	} while (open_delay_counter < 2); // Wait MAX 2 sec
	if (open_delay_counter == 2) { // No such file or directory
		fprintf(stderr,F_ERROR_OPEN_PIPE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
		return (program_end(errno,pipe_fd,pipe_location)); // Close pipe file & Restore signal handler.
	}
	// 2. Start the time measurement
	gettimeofday(&t_start,NULL);
	// 3. Read data and count the number of 'a' bytes read
	do {
		if ((chars_read = read(pipe_fd, buf, MAX_BUF)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, the functions shall return -1 and set errno to indicate the error.
			fprintf(stderr,F_ERROR_READ_PIPE_MSG,pipe_location,strerror(errno)); // No need to call fflush(stderr);
			return (program_end(errno,pipe_fd,pipe_location)); // Close pipe file & Restore signal handler.
		}
		pipe_size += chars_read;
	} while (chars_read == MAX_BUF);
	// 4. Finish the time measurement
	gettimeofday(&t_end,NULL);
	elapsed_time = ((t_end.tv_sec-t_start.tv_sec)*1000.0) + ((t_end.tv_usec-t_start.tv_usec)/1000.0);
	// 5. Print the measurement result along with the number of bytes read
	printf(BENCHMARK_MSG,pipe_size,elapsed_time);
	fflush(stdout);
	// 6. Cleanup. Exit gracefully
	return (program_end(0,pipe_fd,pipe_location)); // Close pipe file & Restore signal handler.
}
