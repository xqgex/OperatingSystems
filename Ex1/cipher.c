#include <dirent.h> // DIR, opendir, readdir, closedir
#include <errno.h> // ENOENT, errno
#include <fcntl.h> // O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, open
#include <linux/limits.h> // PATH_MAX, NAME_MAX
#include <stdio.h> // printf, snprintf, stdout
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <string.h> // strerror, strcmp
#include <sys/stat.h> // stat, mkdir
#include <unistd.h> // lseek, read, write, close

#ifndef NAME_MAX
#	define NAME_MAX 255 // 255 in ext4, Number of chars in a file name, Source: http://serverfault.com/questions/9546/filename-length-limits-on-linux
#endif
#ifndef PATH_MAX
#	define PATH_MAX 4096 // 4096 in ext4, Number of chars in a path name including null, Source: http://serverfault.com/questions/9546/filename-length-limits-on-linux
#endif

// Optimal buffer size should be the HDD block size
// http://stackoverflow.com/questions/13433286/optimal-buffer-size-for-reading-file-in-c
// http://www.unix.com/programming/177585-maximum-buffer-size-read.html
// http://stackoverflow.com/questions/236861/how-do-you-determine-the-ideal-buffer-size-when-using-fileinputstream
#define MAX_IO_SIZE 4*1024 // 4 KB block size is the default in ext4

// Define printing strings
#define OPERANDS_MISSING_MSG		"Missing operands\nUsage: %s input_dir key_file output_dir\nExiting...\n"
#define OPERANDS_SURPLUS_MSG		"Too many operands\nUsage: %s input_dir key_file output_dir\nExiting...\n"
#define ERROR_INPUT_FOLDER_MSG		"[Error] Input folder '%s': %s\nExiting...\n"
#define ERROR_IO_MSG			"[Error] I/O error\nExiting...\n"
#define ERROR_KEY_FILE_MSG		"[Error] Key file '%s': %s\nExiting...\n"
#define ERROR_KEY_OPEN_FAILED_MSG	"[Error] Could not open key file '%s'\nExiting...\n"
#define ERROR_MOVING_POINTER_MSG	"[Error] There was an error moving the pointer to %d\nExiting...\n"
#define ERROR_OUTPUT_FILE_MSG		"[Error] Output file '%s': %s\nExiting...\n"
#define ERROR_OUTPUT_FOLDER_MSG		"[Error] Output folder '%s': %s\nExiting...\n"
#define ERROR_OUTPUT_CREATE_FAILED_MSG	"[Error] Failed to create output directory\nExiting...\n"
#define ERROR_PARTIAL_READ_MSG		"[Error] Partial read\nExiting...\n"
#define ERROR_UNEXPECTED_EOF_MSG	"[Error] Unexpected EOF\nExiting...\n"
#define WORKING_ON_FILE_MSG		"[Working] File_name: \"%s\", File Size: %lld bytes\n"
#define DONE_MSG			"Done.\n"

/* Valid "System call" functions (Functions learned in rec 1 & lseek permitted in the HW forum)
 * FILES:
 *	int open(const char *pathname, int flags, mode_t mode)
 *	int close(int fd)
 *	int creat(const char *path, mode_t mode)
 *	ssize_t read(int fd, void* buf, size_t count)
 *	ssize_t write(int fd, void* buf, size_t count)
 *	int stat(const char *path, struct stat *buf)
 *	off_t lseek(int fd, off_t offset, int whence);
 * DIRECTORY:
 *	int mkdir(const char *pathname, mode_t mode)
 *	DIR *opendir(const char *name)
 *	struct dirent *readdir(DIR *dirp)
 */
int getFileDetails(char* fileLocation, int* returned_fd, long long* returned_size) { // Wrapper for 'open' function
	struct stat tmp;
	if ((*returned_fd = open(fileLocation, O_RDONLY)) == -1) {
		printf(ERROR_KEY_FILE_MSG,fileLocation,strerror(errno));
		fflush(stdout);
		return 0; // false
	} else if (stat(fileLocation, &tmp) == -1) { // File size === tmp.st_size
		printf(ERROR_KEY_OPEN_FAILED_MSG,fileLocation);
		fflush(stdout);
		return 0; // false
	}
	*returned_size = tmp.st_size;
	return 1; // true
}
int getFileContent(int fd, int offset, size_t length, char* returned_buf) { // Wrapper for 'read' function
	size_t lengthReaded;
	if (lseek(fd,offset,SEEK_SET) != offset) { // Move the reading pointer with selected offset
		printf(ERROR_MOVING_POINTER_MSG,offset);
		fflush(stdout);
		return 0; // false
	}
	lengthReaded = read(fd, returned_buf, length);
	if ((lengthReaded == (unsigned int)-1)&&(errno == 0)) {		// * EOF		(N. of bytes read == -1, errno == 0)
		printf(ERROR_UNEXPECTED_EOF_MSG);
	} else if ((lengthReaded == (unsigned int)-1)&&(errno != 0)) {	// * I/O error		(N. of bytes read == -1, errno != 0)
		printf(ERROR_IO_MSG);
	} else if (lengthReaded < length) {				// * Partial read	(N. of bytes read <  N. of bytes expected)
		printf(ERROR_PARTIAL_READ_MSG);
	} else {							// * Successful read	(N. of bytes read == N. of bytes expected)
		return 1; // true
	}
	fflush(stdout);
	return 0; // false
}
int main(int argc, char *argv[]) {
	// General variable
	int i; // Temp loop index
	int lastCall; // "1" == Last Loop call ; "0" == Keep reading input file
	struct dirent *dp; // Directory pointer
	DIR* inputFolder;
	DIR* outputFolder;
	// loopInput variables:
	char loopInput_location[PATH_MAX+NAME_MAX+1]; // The location of the current input file in the loop
	int loopInput_fd; // The descriptor of the current input file in the loop
	int loopInput_offset; // From where to start reading the current input file in the loop (run from 0 to file size with addition of the reading window each time)
	size_t loopInput_window; // The sliding window of the current input file in the loop
	long long loopInput_size; // File size of the current input file in the loop
	char loopInput_buf[MAX_IO_SIZE+1]; // The content read from the current input file in the loop
	// keyFile variables:
	int keyFile_fd; // The location of the key file
	int keyFile_offset; // From where to start reading the key file (run from 0 to key file size with addition of the reading window each time)
	size_t keyFile_window; // The sliding window of the key file
	long long keyFile_size; // File size of the key file
	char keyFile_buf[MAX_IO_SIZE+1]; // The content read from the key file
	// loopOutput variables:
	char loopOutput_location[PATH_MAX+NAME_MAX+1]; // The location of the current output file in the loop
	int loopOutput_fd; // The descriptor of the current output file in the loop
	char loopOutput_buf[MAX_IO_SIZE+1]; // The content that need to be written to the current output file in the loop
	// Check correct call structure
	if (argc != 4) {
		if (argc < 4) {
			printf(OPERANDS_MISSING_MSG,argv[0]);
		} else {
			printf(OPERANDS_SURPLUS_MSG,argv[0]);
		}
		fflush(stdout);
		return (EXIT_FAILURE);
	}
	// Check that the "input folder" is valid
	if ((inputFolder = opendir(argv[1])) == NULL) {
		printf(ERROR_INPUT_FOLDER_MSG,argv[1],strerror(errno));
		fflush(stdout);
		return (EXIT_FAILURE);
	}
	// Check that a valid "key file" received
	if (getFileDetails(argv[2], &keyFile_fd, &keyFile_size) == 0) {
		closedir(inputFolder);
		return (EXIT_FAILURE);
	}
	// Check that the "output folder" is valid
	if ((outputFolder = opendir(argv[3])) == NULL) {
		if (errno == ENOENT) {
			if (mkdir(argv[3], 0777) == -1) { // Create the directory
				printf(ERROR_OUTPUT_CREATE_FAILED_MSG);
				fflush(stdout);
				closedir(inputFolder);
				close(keyFile_fd);
				return (EXIT_FAILURE);
			}
		} else {
			printf(ERROR_OUTPUT_FOLDER_MSG,argv[3],strerror(errno));
			fflush(stdout);
			closedir(inputFolder);
			close(keyFile_fd);
			return (EXIT_FAILURE);
		}
	}
	// Loop through all files in "input' folder
	while ((dp=readdir(inputFolder)) != NULL) {
		snprintf(loopInput_location, sizeof(loopInput_location), "%s/%s",argv[1],dp->d_name); // Set 'loopInput_location' to be the path to the current input file
		snprintf(loopOutput_location, sizeof(loopOutput_location), "%s/%s",argv[3],dp->d_name); // Set 'loopOutput_location' to be the path to the current output file
		if ((strcmp(dp->d_name, ".") != 0) && (strcmp(dp->d_name, "..")  != 0)) { // Ignore "." and ".." - All other 'items' are files and not folders
			// Init input file
			if (getFileDetails(loopInput_location, &loopInput_fd, &loopInput_size) == 0) { // Get current file descriptor and size
				closedir(inputFolder);
				closedir(outputFolder);
				close(keyFile_fd);
				return (EXIT_FAILURE);
			}
			// Init output file
			if ((loopOutput_fd = open(loopOutput_location,  O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
				printf(ERROR_OUTPUT_FILE_MSG,loopOutput_location,strerror(errno));
				fflush(stdout);
				closedir(inputFolder);
				closedir(outputFolder);
				close(keyFile_fd);
				close(loopInput_fd);
				return (EXIT_FAILURE);
			}
			// Now lets encrypt/decrypt, The ~~general~~ idea for the next lines is:
			// * Read MAX_IO_SIZE bytes from "loopInput_fd" and from "keyFile_fd".
			// * If we read all bytes from "keyFile_fd" => start reading him from the beginning.
			// * Stop only when we read all bytes from "loopInput_fd".
			// When reading, We will distinguish between:
			// * Successful read	(N. of bytes read == N. of bytes expected)
			// * Partial read	(N. of bytes read <  N. of bytes expected)
			// * EOF		(N. of bytes read == -1, errno == 0)
			// * I/O error		(N. of bytes read == -1, errno != 0)
			loopInput_offset = 0; // Start reading current input file from the beginning
			keyFile_offset = 0; // Start reading key file from the beginning
			lastCall = 0; // This var represent that we are now reaching the EOF
			while (lastCall == 0) {
				// How much to read from the current input file
				if ((loopInput_size-loopInput_offset) < MAX_IO_SIZE) { // If current input file size is smaller then the maximum reading window
					loopInput_window = loopInput_size-loopInput_offset;
					lastCall = 1;
				} else {
					loopInput_window = MAX_IO_SIZE;
				}
				// How much to read from the key file
				if ((keyFile_size-keyFile_offset) < loopInput_window) { // If key file size is smaller then the current reading window
					if (keyFile_offset == 0) { // Key file pointer is at the beginning of the file
						keyFile_window = keyFile_size;
					} else { // If the key file is smaller and we are not reading him from the beginning resize windows so in the next call we will
						loopInput_window = keyFile_size-keyFile_offset;
						keyFile_window = keyFile_size-keyFile_offset;
						if ((lastCall == 1)&&(loopInput_offset+loopInput_window < loopInput_size)) {
							lastCall = 0; // Uncheck lastCall if after the input window resize we still have data to read
						}
					}
				} else {
					keyFile_window = loopInput_window; // Read as much as we can from the key file
				}
				// Read current input file
				if (getFileContent(loopInput_fd, loopInput_offset, loopInput_window, loopInput_buf) == 0) {
					closedir(inputFolder);
					closedir(outputFolder);
					close(keyFile_fd);
					close(loopInput_fd);
					close(loopOutput_fd);
					return (EXIT_FAILURE);
				}
				loopInput_offset += loopInput_window; // Make the input file offset variable ready for the next itiration
				// Read key file
				if (getFileContent(keyFile_fd, keyFile_offset, keyFile_window, keyFile_buf) == 0) {
					closedir(inputFolder);
					closedir(outputFolder);
					close(keyFile_fd);
					close(loopInput_fd);
					close(loopOutput_fd);
					return (EXIT_FAILURE);
				}
				for (i=keyFile_window; i<(int)loopInput_window; i++) { // Rewrite key file content fo fill the input window size
					keyFile_buf[i] = keyFile_buf[i-keyFile_window];
				}
				keyFile_offset = loopInput_window % keyFile_window; // Make the key file offset variable ready for the next itiration
				// Calculate Bitwise XOR
				for (i=0; i<(int)loopInput_window; i++) {
					loopOutput_buf[i] = (char)(loopInput_buf[i] ^ keyFile_buf[i]);
				}
				// Write result
				if (write(loopOutput_fd, loopOutput_buf, loopInput_window) != (int)loopInput_window) {
					printf(ERROR_OUTPUT_FILE_MSG,loopOutput_location,strerror(errno));
					fflush(stdout);
					closedir(inputFolder);
					closedir(outputFolder);
					close(keyFile_fd);
					close(loopInput_fd);
					close(loopOutput_fd);
					return (EXIT_FAILURE);
				}
			}
			close(loopInput_fd);
			close(loopOutput_fd);
			printf(WORKING_ON_FILE_MSG,loopInput_location,(long long)loopInput_size);
			fflush(stdout);
		}
	}
	// Close open files and folders
	closedir(inputFolder);
	closedir(outputFolder);
	close(keyFile_fd);
	// All done, exit
	printf(DONE_MSG);
	fflush(stdout);
}
