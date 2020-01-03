#define _FILE_OFFSET_BITS 64
#include <errno.h>	// ERANGE, errno
#include <stdio.h>	// printf, printf, fprintf, sprintf, stderr, sscanf
#include <stdlib.h>	// EXIT_FAILURE, EXIT_SUCCESS, strtol
#include <string.h>	// strlen, strcmp, strerror, memset
#include <unistd.h>	// lseek, close, read, write
#include <limits.h>	// LONG_MAX, LONG_MIN
#include <sys/stat.h>	// S_ISDIR, stat
#include <fcntl.h>	// O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, open
#include <arpa/inet.h> 	// AF_INET, SOCK_STREAM, socket, htons, inet_addr, connect, sockaddr_in

#define MAX_BUF 4096

// Define printing strings
#define IP_INVALID_MSG			"%s is not a valid IPv4 address\n"
#define PORT_INVALID_MSG		"The port must be a positive integer between 1 to 65535\n"
#define USAGE_OPERANDS_MISSING_MSG	"Missing operands\nUsage: %s <IP> <PORT> <IN> <OUT>\n"
#define USAGE_OPERANDS_SURPLUS_MSG	"Too many operands\nUsage: %s <IP> <PORT> <IN> <OUT>\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define F_ERROR_FUNCTION_LSEEK_MSG	"[Error] lseek() failed with an error: %s\n"
#define F_ERROR_FUNCTION_SPRINTF_MSG	"[Error] sprintf() failed with an error\n"
#define F_ERROR_FUNCTION_STRTOL_MSG	"[Error] strtol() failed with an error: %s\n"
#define F_ERROR_INPUT_CLOSE_MSG		"[Error] Close input file: %s\n"
#define F_ERROR_INPUT_FILE_MSG		"[Error] Input file '%s': %s\n"
#define F_ERROR_INPUT_IS_FOLDER_MSG	"[Error] Input file '%s': Is a directory\n"
#define F_ERROR_INPUT_OPEN_MSG		"[Error] Could not open input file '%s': %s\n"
#define F_ERROR_INPUT_READ_MSG		"[Error] Reading from input file %s: %s\n"
#define F_ERROR_OUTPUT_CLOSE_MSG	"[Error] Close output file: %s\n"
#define F_ERROR_OUTPUT_FILE_MSG		"[Error] Output file '%s': %s\n"
#define F_ERROR_OUTPUT_WRITE_MSG	"[Error] Writing to output file %s: %s\n"
#define F_ERROR_SOCKET_CLOSE_MSG	"[Error] Close socket: %s\n"
#define F_ERROR_SOCKET_CONNECT_MSG	"[Error] Connect failed: %s\n"
#define F_ERROR_SOCKET_CREATE_MSG	"[Error] Could not create socket: %s\n"
#define F_ERROR_SOCKET_DISCONNECT_MSG	"[Error] You have been disconnected form the server unexpectedly\n"
#define F_ERROR_SOCKET_READ_MSG		"[Error] Reading from socket: %s\n"
#define F_ERROR_SOCKET_WRITE_MSG	"[Error] Writing to socket: %s\n"

int program_end(int error, int in_fd, int out_fd, int sock_fd) {
	int res = 0;
	if ((0 < in_fd)&(close(in_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_INPUT_CLOSE_MSG,strerror(errno));
		res = errno;
	}
	if ((0 < out_fd)&&(close(out_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_OUTPUT_CLOSE_MSG,strerror(errno));
		res = errno;
	}
	if ((0 < sock_fd)&&(close(sock_fd) == -1)) { // Upon successful completion, 0 shall be returned; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_SOCKET_CLOSE_MSG,strerror(errno));
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
int validateIP4Dotted(const char *s) { // http://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ip-address-in-c#answer-14181669
	char tail[16];
	int c,i;
	int len = strlen(s);
	unsigned int d[4];
	if (len < 7 || 15 < len) {
		return -1;
	}
	tail[0] = 0;
	c = sscanf(s,"%3u.%3u.%3u.%3u%s",&d[0],&d[1],&d[2],&d[3],tail);
	if (c != 4 || tail[0]) {
		return -1;
	}
	for (i=0;i<4;i++) {
		if (d[i] > 255) {
			return -1;
		}
	}
	return 0;
}
int main(int argc, char *argv[]) {
	// Function variables
	int input_port = 0;		// The server port (type == int)
	int in_fd = 0;			// The input file 'file descriptor' (FD)
	int out_fd = 0;			// The output file 'file descriptor' (FD)
	int sock_fd = 0;		// The socket file descriptor (FD)
	int counter_dst = 0;		// The number of bytes we wrote to output
	int counter_src = 0;		// The number of bytes we read from input
	int counter_srvr = 0;		// The number of bytes we got from the server
	int counter_tmp = 0;		// Temporery loop var
	char char_buf[MAX_BUF+1];	// The string buffer (From the input, To the server, From the server, To the output)
	char input_port_char[6];	// The server port (type == string)
	char* endptr_PORT;		// strtol() for 'input_port'
	struct sockaddr_in serv_addr;	// The data structure for the server
	struct stat in_stat;		// The data structure for the key file
	// Init variables
	memset(char_buf,'0',sizeof(char_buf));
	memset(&serv_addr,'0',sizeof(serv_addr));
	// Check correct call structure
	if (argc != 5) {
		if (argc < 5) {
			printf(USAGE_OPERANDS_MISSING_MSG,argv[0]);
		} else {
			printf(USAGE_OPERANDS_SURPLUS_MSG,argv[0]);
		}
		return EXIT_FAILURE;
	}
	// Check input ip
	if (validateIP4Dotted(argv[1]) == -1) { // The function return 0 if input string is a valid IPv4 address, Otherwise return -1
		printf(IP_INVALID_MSG,argv[1]);
		return EXIT_FAILURE;
	}
	// Check input port
	input_port = strtol(argv[2],&endptr_PORT,10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (input_port == (int)LONG_MAX || input_port == (int)LONG_MIN)) || (errno != 0 && input_port == 0)) {
		fprintf(stderr,F_ERROR_FUNCTION_STRTOL_MSG,strerror(errno));
		return errno;
	} else if ( (endptr_PORT == argv[2])||(input_port < 1)||(input_port > 65535) ) { // (Empty string) or (not in range [1,65535])
		printf(PORT_INVALID_MSG);
		return EXIT_FAILURE;
	} else if (sprintf(input_port_char,"%d",input_port) < 0) { // sprintf(), If an output error is encountered, a negative value is returned.
		fprintf(stderr,F_ERROR_FUNCTION_SPRINTF_MSG);
		return EXIT_FAILURE;
	} else if (strcmp(input_port_char,argv[2]) != 0) { // Contain invalid chars
		printf(PORT_INVALID_MSG);
		return EXIT_FAILURE;
	}
	// Check input file - ||You may not assume anything regarding the size of IN.||
	if ((in_fd = open(argv[3],O_RDONLY)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_INPUT_FILE_MSG,argv[3],strerror(errno)); // IN must exist, otherwise output an error and exit.
		return program_end(errno,in_fd,out_fd,sock_fd);
	} else if (stat(argv[3],&in_stat) == -1) { // On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_INPUT_OPEN_MSG,argv[3],strerror(errno));
		return program_end(errno,in_fd,out_fd,sock_fd);
	} else if (S_ISDIR(in_stat.st_mode)) {
		fprintf(stderr,F_ERROR_INPUT_IS_FOLDER_MSG,argv[3]);
		return program_end(-1,in_fd,out_fd,sock_fd);
	} else if (lseek(in_fd,SEEK_SET,0) == -1) { // Upon successful completion, lseek() returns the resulting offset ... from the beginning of the file. On error, the value (off_t) -1 is returned and errno is set to indicate the error.
		fprintf(stderr,F_ERROR_FUNCTION_LSEEK_MSG,strerror(errno));
		return program_end(errno,in_fd,out_fd,sock_fd);
	}
	// Check output file
	if ((out_fd = open(argv[4],O_WRONLY | O_CREAT | O_TRUNC,0777)) == -1) { // Upon successful completion, ... return a non-negative integer .... Otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_OUTPUT_FILE_MSG,argv[4],strerror(errno));// The path to the OUT file must exist, otherwise output an error and exit. (i.e., no need to check the folder, just try to open/create the file).
		return program_end(errno,in_fd,out_fd,sock_fd); // If OUT does not exist, the client should create it. If it exists, it should truncate it.
	}
	// Open connection to the server // Data should be sent via a TCP socket, to the server specified by IP:PORT.
	if((sock_fd = socket(AF_INET,SOCK_STREAM,0)) == -1) { // On success, a file descriptor for the new socket is returned. On error, -1 is returned, and errno is set appropriately.
		fprintf(stderr,F_ERROR_SOCKET_CREATE_MSG,strerror(errno));
		return program_end(errno,in_fd,out_fd,sock_fd);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(input_port);
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	if (connect(sock_fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1) { // Upon successful completion, connect() shall return 0; otherwise, -1 shall be returned and errno set to indicate the error.
		fprintf(stderr,F_ERROR_SOCKET_CONNECT_MSG,strerror(errno));
		return program_end(errno,in_fd,out_fd,sock_fd);
	}
	// All inputs variables are valid, Stsrt working
	while (1) {
		// Read input file IN
		if ((counter_src = read(in_fd,char_buf,MAX_BUF)) == -1) { // On success, the number of bytes read is returned (zero indicates end of file), .... On error, -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_INPUT_READ_MSG,argv[3],strerror(errno));
			return program_end(errno,in_fd,out_fd,sock_fd);
		} else if (counter_src == 0) { // Received EOF, Exit the infinity loop
			break;
		}
		// Sending data from the input file IN to the server,
		// You may assume that if X bytes are successfully sent to the server, then it replies with X bytes.
		if ((counter_src = write(sock_fd,char_buf,counter_src)) == -1) { // On success, the number of bytes written is returned (zero indicates nothing was written). On error, -1 is returned, and errno is set appropriately.
			fprintf(stderr,F_ERROR_SOCKET_WRITE_MSG,strerror(errno));
			return program_end(errno,in_fd,out_fd,sock_fd);
		} else if (counter_src == 0) {
			break;
		}
		counter_srvr = 0; // Init to 0 the number of bytes we got from the server
		while (counter_srvr < counter_src) {
			if ((counter_tmp = read(sock_fd,char_buf+counter_srvr,counter_src-counter_srvr)) == -1) { // On success, the number of bytes read is returned (zero indicates end of file), .... On error, -1 is returned, and errno is set appropriately.
				fprintf(stderr,F_ERROR_SOCKET_READ_MSG,strerror(errno));
				return program_end(errno,in_fd,out_fd,sock_fd);
			} else if (counter_tmp == 0) {
				fprintf(stderr,F_ERROR_SOCKET_DISCONNECT_MSG);
				return program_end(-1,in_fd,out_fd,sock_fd);
			}
			counter_srvr += counter_tmp;
		}
		// writing the result (the server's answer) into the output file OUT.
		counter_dst = 0; // Init to 0 the number of bytes we wrote to output
		while (counter_dst < counter_srvr) {
			if ((counter_tmp = write(out_fd,char_buf+counter_dst,counter_srvr-counter_dst)) == -1) { // On success, the number of bytes written is returned (zero indicates nothing was written). On error, -1 is returned, and errno is set appropriately.
				fprintf(stderr,F_ERROR_OUTPUT_WRITE_MSG,argv[4],strerror(errno));
				return program_end(errno,in_fd,out_fd,sock_fd);
			}
			counter_dst += counter_tmp;
		}
	}
	// Once all data is sent, and the client finished reading and handling any remaining data received from the server,
	// The client closes the connection and finishes.
	return program_end(EXIT_SUCCESS,in_fd,out_fd,sock_fd);
}
