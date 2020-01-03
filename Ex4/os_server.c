#define _FILE_OFFSET_BITS 64
#define _POSIX_SOURCE
#include <errno.h>	// ERANGE, errno
#include <signal.h>	// SIG_IGN, SIGINT, SIGPIPE, struct sigaction, sigaction, sigemptyset
#include <stdio.h>	// printf, printf, fprintf, sprintf, stderr, sscanf
#include <stdlib.h>	// EXIT_FAILURE, EXIT_SUCCESS, strtol
#include <string.h>	// strlen, strcmp, strerror, memset
#include <unistd.h>	// lseek, close, read, write
#include <limits.h>	// LONG_MAX, LONG_MIN
#include <sys/stat.h>	// S_ISDIR, stat
#include <fcntl.h>	// O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, open
#include <arpa/inet.h> 	// AF_INET, SOCK_STREAM, socket, htons, inet_addr, connect, sockaddr_in

#define MAX_BUF		4096
#define RANDOM_FILE	"/dev/urandom"

// Define printing strings
#define KEYLEN_INVALID_MSG		"The key length must be a positive integer\n"
#define PORT_INVALID_MSG		"The port must be a positive integer between 1 to 65535\n"
#define USAGE_OPERANDS_MISSING_MSG	"Missing operands\nUsage: %s <PORT> <KEY> [<KEYLEN>]\n"
#define USAGE_OPERANDS_SURPLUS_MSG	"Too many operands\nUsage: %s <PORT> <KEY> [<KEYLEN>]\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define F_ERROR_FILE_CLOSE_MSG		"[Error] Close file: %s\n"
#define F_ERROR_FUNCTION_FORK_MSG	"[Error] fork() failed with an error: %s\n"
#define F_ERROR_FUNCTION_LSEEK_MSG	"[Error] lseek() failed with an error: %s\n"
#define F_ERROR_FUNCTION_SPRINTF_MSG	"[Error] sprintf() failed with an error\n"
#define F_ERROR_FUNCTION_STRTOL_MSG	"[Error] strtol() failed with an error: %s\n"
#define F_ERROR_KEY_EMPTY_MSG		"[Error] The key file %s is empty\n"
#define F_ERROR_KEY_FILE_MSG		"[Error] Key file '%s': %s\n"
#define F_ERROR_KEY_IS_FOLDER_MSG	"[Error] Key file '%s': Is a directory\n"
#define F_ERROR_KEY_OPEN_MSG		"[Error] Could not open key file '%s': %s\n"
#define F_ERROR_KEY_READ_MSG		"[Error] Reading from key file: %s\n"
#define F_ERROR_KEY_WRITE_MSG		"[Error] Writing to key file %s: %s\n"
#define F_ERROR_RANDOM_CLOSE_MSG	"[Error] Close random file: %s\n"
#define F_ERROR_RANDOM_FILE_MSG		"[Error] Random file '%s': %s\n"
#define F_ERROR_RANDOM_READ_MSG		"[Error] Reading from random file %s: %s\n"
#define F_ERROR_SIGACTION_INIT_MSG	"[Error] Failed to init signal handler (sigaction): %s\n"
#define F_ERROR_SIGACTION_RESTORE_MSG	"[Error] Failed to restore signal handler (sigaction): %s\n"
#define F_ERROR_SOCKET_ACCEPT_MSG	"[Error] Accept socket: %s\n"
#define F_ERROR_SOCKET_BIND_MSG		"[Error] Socket bind failed: %s\n"
#define F_ERROR_SOCKET_CLOSE_MSG	"[Error] Close socket: %s\n"
#define F_ERROR_SOCKET_CREATE_MSG	"[Error] Could not create socket: %s\n"
#define F_ERROR_SOCKET_LISTEN_MSG	"[Error] Listen to socket failed: %s\n"
#define F_ERROR_SOCKET_READ_MSG		"[Error] Reading from socket: %s\n"

int conn_fd = -1;			// The connection file 'file descriptor' (FD)1
int key_fd = -1;			// The key file 'file descriptor' (FD)
int listen_fd = -1;			// The listen file 'file descriptor' (FD) 
int rand_fd = -1;			// The random file 'file descriptor' (FD)
struct sigaction sigint_old_handler;

int program_end(int error) {
	int res = 0;
	if ((0 < conn_fd)&&(close(conn_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_SOCKET_CLOSE_MSG,strerror(errno));
		res = errno;
	}
	if ((0 < key_fd)&&(close(key_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_FILE_CLOSE_MSG,strerror(errno));
		res = errno;
	}
	if ((0 < listen_fd)&&(close(listen_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_SOCKET_CLOSE_MSG,strerror(errno));
		res = errno;
	}

	if ((0 < rand_fd)&&(close(rand_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_FILE_CLOSE_MSG,strerror(errno));
		res = errno;
	}
	if (sigaction(SIGINT,&sigint_old_handler,NULL) == -1) { // returns 0 on success; on error, -1 is returned, and errno is set to indicate the error.
		fprintf(stderr,F_ERROR_SIGACTION_RESTORE_MSG,strerror(errno));
		res = errno;
	}
	if ((error != 0)||(res != 0)) {
		fprintf(stderr,ERROR_EXIT_MSG);
		if (error != 0) { // If multiple error occurred, Print the error that called 'program_end' function.
			res = error;
		}
	}
	return res;
}
void sigpipe_handler(int signum) {
	// a. If SIGINT is received, the server should cleanup and exit gracefully.
	program_end(0);
	exit(signum);
}
int ChildProcess(char* key_file) { // In the new process:
	// General variables
	int i = 0;				// Temporery loop var
	// Function variables
	int counter_client = 0;			// The number of bytes we read from the client
	int counter_key = 0;			// The number of bytes we got from the key file
	int counter_tmp = 0;			// Temporery loop var
	char char_buf[MAX_BUF+1];		// The string buffer (From the client, To the server, To the client)
	char key_buf[MAX_BUF+1];		// The key buffer
	// Init variables
	memset(char_buf,'0',sizeof(char_buf));
	memset(key_buf,'0',sizeof(key_buf));
	// a. Open the key file.
	// Each forked process opens the key file separately. The main (parent) process does not open the key file at all, except when initializing it (if KEYLEN is provided).
	if ((key_fd = open(key_file,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_KEY_OPEN_MSG,key_file,strerror(errno));
		return program_end(errno);
	}
	while (1) {
		memset(char_buf,'0',sizeof(char_buf));
		// b. Read data from the client until EOF.
		// You cannot assume anything regarding the overall size of the key file or the input.
		if ((counter_client = read(conn_fd,char_buf,sizeof(char_buf)-1)) == -1) { // On success, the number of bytes read is returned (zero indicates end of file), .... On error, -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_SOCKET_READ_MSG,strerror(errno));
			return program_end(errno);
		} else if (counter_client == 0) {
			break;
		}
		counter_key = 0; // Set number of bytes read from the key file to 0
		while (counter_key < counter_client) {
			if ((counter_tmp = read(key_fd,key_buf+counter_key,counter_client-counter_key)) == -1) { // On success, the number of bytes read is returned (zero indicates end of file), .... On error, -1 is returned, and errno is set appropriately.
				fprintf(stderr,F_ERROR_KEY_READ_MSG,strerror(errno));
				return program_end(errno);
			} else if (counter_tmp == 0) { // Reached end of key, Reset and read from start
				if (lseek(key_fd,SEEK_SET,0) == -1) { // Upon successful completion, lseek() returns the resulting offset ... from the beginning of the file. On error, the value (off_t) -1 is returned and errno is set to indicate the error.
					fprintf(stderr,F_ERROR_FUNCTION_LSEEK_MSG,strerror(errno));
					return program_end(errno);
				}
			} else { // Success - increase our counter of bytes read from key
				counter_key += counter_tmp;
			}
		}
		// c. Whenever data is read, encrypt, and send back to the client.
		// You should try to be as efficient as possible in receiving, encrypting, and sending data; Namely, when any data block is received, then encrypt it immediately and send it back, don't wait for more data from the client, i.e., don't aggregate encryption requests.
		for (i=0;i<counter_client;++i) { // Perform encryption operation
			char_buf[i] = char_buf[i]^key_buf[i];
		}
		if ((counter_client = write(conn_fd,char_buf,counter_client)) == -1) { // On success, the number of bytes written is returned (zero indicates nothing was written). On error, -1 is returned, and errno is set appropriately.
			break;
		}
	}
	// d. When finished, close the socket, cleanup, and exit gracefully.
	return program_end(EXIT_SUCCESS);
}
int main(int argc, char *argv[]) {
	// Function variables
	int input_keylen = 0;			// The key length (type == int)
	int input_port = 0;			// The server port (type == int)
	int counter_rand = 0;			// The number of bytes we got from the random file
	int counter_key = 0;			// The number of bytes we got from the key file
	int counter_tmp = 0;			// Temporery loop var
	int pid = 0;				// The pid from the fork() function
	char key_buf[MAX_BUF+1];		// The key buffer
	char input_keylen_char[11];		// The key length (type == string)
	char input_port_char[6];		// The server port (type == string)
	char* endptr_KEYLEN;			// strtol() for 'input_keylen'
	char* endptr_PORT;			// strtol() for 'input_port'
	struct sigaction sigint_new_handler;	// The data structure for the signal handler
	struct sockaddr_in serv_addr;		// The data structure for the server
	struct stat key_stat;			// The data structure for the key file
	// Init variables
	memset(key_buf,'0',sizeof(key_buf));
	memset(&serv_addr,'0',sizeof(serv_addr));
	memset(&sigint_new_handler,0,sizeof(sigint_new_handler));
	// Check correct call structure
	if (argc < 3) {
		printf(USAGE_OPERANDS_MISSING_MSG,argv[0]);
		return EXIT_FAILURE;
	} else if (argc > 4) {
		printf(USAGE_OPERANDS_SURPLUS_MSG,argv[0]);
		return EXIT_FAILURE;
	}
	// Check input port
	input_port = strtol(argv[1],&endptr_PORT,10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (input_port == (int)LONG_MAX || input_port == (int)LONG_MIN)) || (errno != 0 && input_port == 0)) {
		fprintf(stderr,F_ERROR_FUNCTION_STRTOL_MSG,strerror(errno));
		return errno;
	} else if ( (endptr_PORT == argv[1])||(input_port < 1)||(input_port > 65535) ) { // (Empty string) or (not in range [1,65535])
		printf(PORT_INVALID_MSG);
		return EXIT_FAILURE;
	} else if (sprintf(input_port_char,"%d",input_port) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
		fprintf(stderr,F_ERROR_FUNCTION_SPRINTF_MSG);
		return EXIT_FAILURE;
	} else if (strcmp(input_port_char,argv[1]) != 0) { // Contain invalid chars
		printf(PORT_INVALID_MSG);
		return EXIT_FAILURE;
	}
	//  Check KEY file
	if (argc == 3) { // ./os_server <PORT> <KEY>
		// You cannot assume that the key file exists or has data. In either case, if KEYLEN is not provided, the server should fail starting (exit with an error).
		if (access(argv[2],R_OK) == -1) {// On success ..., zero is returned. On error ..., -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_KEY_FILE_MSG,argv[2],strerror(errno)); // IN must exist, otherwise output an error and exit.
			return program_end(errno); // ### PERSONAL NOTE ### - I used access() function in order to verify that the server will be able to actually read the file content.
		} else if (stat(argv[2],&key_stat) == -1) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_KEY_OPEN_MSG,argv[2],strerror(errno));
			return program_end(errno);
		} else if (S_ISDIR(key_stat.st_mode)) {
			fprintf(stderr,F_ERROR_KEY_IS_FOLDER_MSG,argv[2]);
			return program_end(EXIT_FAILURE);
		} else if (key_stat.st_size == 0) {
			fprintf(stderr,F_ERROR_KEY_EMPTY_MSG,argv[2]);
			return program_end(EXIT_FAILURE);
		}
	} else { // ./os_server <PORT> <KEY> <KEYLEN>
		// If KEYLEN is provided, initialize the key file:
		// a. Create a new file or truncate an existing file – KEY
		if ((key_fd = open(argv[2],O_WRONLY | O_CREAT | O_TRUNC,0777)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
			fprintf(stderr,F_ERROR_KEY_FILE_MSG,argv[2],strerror(errno));// The path to the OUT file must exist, otherwise output an error and exit. (i.e., no need to check the folder, just try to open/create the file).
			return program_end(errno); // If OUT does not exist, the client should create it. If it exists, it should truncate it.
		}
		// Check input KEYLEN
		input_keylen = strtol(argv[3],&endptr_KEYLEN,10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
		if ((errno == ERANGE && (input_keylen == (int)LONG_MAX || input_keylen == (int)LONG_MIN)) || (errno != 0 && input_keylen == 0)) {
			fprintf(stderr,F_ERROR_FUNCTION_STRTOL_MSG,strerror(errno));
			return program_end(errno);
		} else if ( (endptr_KEYLEN == argv[3])||(input_keylen < 1) ) { // (Empty string) or (not positive)
			printf(KEYLEN_INVALID_MSG);
			return program_end(EXIT_FAILURE);
		} else if (sprintf(input_keylen_char,"%d",input_keylen) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
			fprintf(stderr,F_ERROR_FUNCTION_SPRINTF_MSG);
			return program_end(EXIT_FAILURE);
		} else if (strcmp(input_keylen_char,argv[3]) != 0) { // Contain invalid chars
			printf(KEYLEN_INVALID_MSG);
			return program_end(EXIT_FAILURE);
		}
		// b. Use the /dev/urandom file to write KEYLEN random bytes into KEY
		if ((rand_fd = open(RANDOM_FILE,O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
			fprintf(stderr,F_ERROR_RANDOM_FILE_MSG,RANDOM_FILE,strerror(errno));// The path to the OUT file must exist, otherwise output an error and exit. (i.e., no need to check the folder, just try to open/create the file).
			return program_end(errno); // If OUT does not exist, the client should create it. If it exists, it should truncate it.
		}
		while (input_keylen > 0) {
			if ((counter_rand = read(rand_fd,key_buf,sizeof(key_buf))) == -1) { // On success, the number of bytes read is returned (zero indicates end of file), .... On error, -1 is returned, and errno is set appropriately.
				fprintf(stderr,F_ERROR_RANDOM_READ_MSG,RANDOM_FILE,strerror(errno));
				return program_end(errno);
			}
			if (counter_rand > input_keylen) {
				counter_rand = input_keylen;
			}
			counter_key = 0; // Init to 0 the number of bytes we got from the server
			while (counter_key < counter_rand) {
				if ((counter_tmp = write(key_fd,key_buf+counter_key,counter_rand-counter_key)) == -1) { // On success, the number of bytes written is returned (zero indicates nothing was written). On error, -1 is returned, and errno is set appropriately.
					fprintf(stderr,F_ERROR_KEY_WRITE_MSG,argv[2],strerror(errno));
					return program_end(errno);
				}
				counter_key += counter_tmp;
			}
			input_keylen -= counter_key;
		}
		if ((0 < rand_fd)&&(close(rand_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
			fprintf(stderr,F_ERROR_RANDOM_CLOSE_MSG,strerror(errno));
			return program_end(errno);
		}
		rand_fd = -1;
		if ((0 < key_fd)&&(close(key_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
			fprintf(stderr,F_ERROR_RANDOM_CLOSE_MSG,strerror(errno));
			return program_end(errno);
		}
		key_fd = -1;
	}
	// In this point there are no open file descriptor
	// Create a TCP socket that listens on PORT (use 10 as the parameter for listen).
	if((listen_fd = socket(AF_INET,SOCK_STREAM,0)) == -1) { // On success, a file descriptor for the new socket is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_SOCKET_CREATE_MSG,strerror(errno));
		return program_end(errno);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = any local machine address
	serv_addr.sin_port = htons(input_port);
	if(bind(listen_fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_SOCKET_BIND_MSG,strerror(errno));
		return program_end(errno);
	}
	if(listen(listen_fd,10) == -1) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_SOCKET_LISTEN_MSG,strerror(errno));
		return program_end(errno);
	}
	// Register a signal handler for SIGINT:
	sigint_new_handler.sa_handler = sigpipe_handler;
	if (sigaction(SIGINT,&sigint_new_handler,&sigint_old_handler) == -1) { // returns 0 on success; on error, -1 is returned, and errno is set to indicate the error.
		fprintf(stderr,F_ERROR_SIGACTION_INIT_MSG,strerror(errno));
		return program_end(errno);
	}
	// Wait for connections (i.e., accept in a loop).
	while(1) {
		if((conn_fd = accept(listen_fd,NULL,NULL)) == -1) { // On success, these system calls return a nonnegative integer that is a descriptor for the accepted socket. On error, -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_SOCKET_ACCEPT_MSG,strerror(errno));
			return program_end(errno);
		}
		// Whenever a client connects, fork a new process.
		if ((pid = fork()) == -1) { // On success, the PID of the child process is returned in the parent, and 0 is returned in the child. On failure, -1 is returned in the parent, no child process is created, and errno is set appropriately
			fprintf(stderr,F_ERROR_FUNCTION_FORK_MSG,strerror(errno));
			return program_end(errno);
		} else if (pid == 0) {
			if ((0 < listen_fd)&&(close(listen_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
				fprintf(stderr,F_ERROR_SOCKET_CLOSE_MSG,strerror(errno));
				return program_end(errno);
			}
			listen_fd = -1;
			return ChildProcess(argv[2]); // Errors in a child process should terminate the child process only.
		} else { // The parent process should continue accepting connections.
			if ((0 < conn_fd)&&(close(conn_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
				fprintf(stderr,F_ERROR_SOCKET_CLOSE_MSG,strerror(errno));
				return program_end(errno);
			}
			conn_fd = -1;
		}
	}
	return program_end(EXIT_SUCCESS);
}
