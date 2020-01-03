//
//	                           |---------------------|
//	                           |    intlist_list     |
//	                           |------|-------|------|
//	          -<---<---<---<-  | head | count | tail |  ->--->--->--->---
//	          |                |------|-------|------|                  |  
//	         \ /                                                       \ /
//	          *                                                         *
//	|-------------------|       |-------------------|         |-------------------|
//	|   intlist_node    |       |   intlist_node    |         |   intlist_node    |
//	|------|-----|------|       |------|-----|------|         |------|-----|------|
//	| NULL | val | next |  <->  | prev | val | next |  >...<  | prev | val | NULL |
//	|------|-----|------|       |------|-----|------|         |------|-----|------|
//
#define _GNU_SOURCE
#include <errno.h>	// ERANGE, errno
#include <stdio.h>	// printf, fprintf, stderr
#include <stdlib.h>	// EXIT_FAILURE, srand, rand, exit, mallo, free, strtol
#include <string.h>	// strlen, strcpy, strerror
#include <unistd.h>	// sleep
#include <pthread.h>	// PTHREAD_MUTEX_RECURSIVE, PTHREAD_CREATE_JOINABLE, 
			// pthread_cond_init, pthread_cond_wait, pthread_cond_signal, pthread_cond_destroy
			// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock, pthread_mutex_destroy
			// pthread_attr_init, pthread_attr_setdetachstate, pthread_attr_destroy
			// pthread_mutexattr_init, pthread_mutexattr_settype, pthread_mutexattr_destroy
			// pthread_create, pthread_join, pthread_exit
#include <limits.h>	// LONG_MAX, LONG_MIN

// Define printing strings
#define LIST_SIZE_MSG			"The size of the list is:%d\n"
#define USAGE_NUM_LESS_THEN_ONE_MSG	"The input value must be a positive integer\n"
#define USAGE_OPERANDS_MISSING_MSG	"Missing operands\nUsage: %s <WNUMc> <RNUM> <MAX> <TIME>\nExiting...\n"
#define USAGE_OPERANDS_SURPLUS_MSG	"Too many operands\nUsage: %s <WNUMc> <RNUM> <MAX> <TIME>\nExiting...\n"
#define ERROR_EXIT_MSG			"Exiting...\n"
#define F_ERROR_MALLOC_LIST_MSG		"[Error] Failed to allocate memory to list.\n"
#define F_ERROR_MALLOC_NODE_MSG		"[Error] Failed to allocate memory to node.\n"
#define F_ERROR_MALLOC_THREADS_MSG	"[Error] Failed to allocate memory to threads.\n"
#define F_ERROR_GENERAL_MSG		"[Error] Error in %s: %s\n"
#define F_ERROR_STRTOL_MSG		"[Error] Strtol failed with error: %s\n"
// Define data types
typedef struct intlist_node {
	int val;
	struct intlist_node *prev; // Previously node (Double linked list)
	struct intlist_node *next; // Next node
} intlist_node_t;
typedef struct intlist_list {
	int count; // How many nodes there are in the list
	struct intlist_node *head; // First node in the list
	struct intlist_node *tail; // Last node in the list
	struct intlist_node *nil; // Pointer to the nil object
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
	pthread_cond_t cond_new_insert;
} intlist;
// Define global variables
int threads_gc_run = 1;
int threads_writers_run = 1;
int threads_readers_run = 1;
int threads_writers_finish = 0;
int threads_readers_finish = 0;
int global_writers = 0;
int global_readers = 0;
int global_max = 0;
int global_time = 0;
pthread_attr_t attr;
pthread_cond_t count_garbage_collector;
// Function declaration
void intlist_init(intlist* list);
void intlist_destroy(intlist** list);
void intlist_push_head(intlist* list, int value);
int intlist_pop_tail(intlist* list);
void intlist_remove_last_k(intlist* list, int k);
int intlist_size(intlist* list);
pthread_mutex_t* intlist_get_mutex(intlist* list);

// Threads
void *thrd_writers(void *argStruct) {
	// Writers - writer threads push random integers to the list, in an infinite loop.
	int rc; // Variable for pthread_mutex_lock & pthread_mutex_unlock & pthread_cond_signal
	intlist* list = argStruct;
	// Push new nodes
	srand(time(NULL));
	while (threads_writers_run) {
		intlist_push_head(list, rand());
		if (intlist_size(list) >= global_max) { // Wakeup the garbage collector
			if ((rc = pthread_cond_signal(&count_garbage_collector)) != 0) { // If successful, the pthread_cond_signal() function shall return zero; otherwise, an error number shall be returned to indicate the error.
				fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_signal()",strerror(rc));
				exit(EXIT_FAILURE);
			}
		}
	}
	// Lock
	if ((rc = pthread_mutex_lock(intlist_get_mutex(list))) != 0) { // If successful, the pthread_mutex_lock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_lock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Inc counter
	threads_writers_finish += 1;
	// Unlock
	if ((rc = pthread_mutex_unlock(intlist_get_mutex(list))) != 0) { // If successful, the pthread_mutex_unlock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_unlock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Finish
	pthread_exit(NULL);
}
void *thrd_readers(void *argStruct) {
	// Readers – reader threads pop integers from the list, in an infinite loop.
	int rc; // Variable for pthread_mutex_lock & pthread_mutex_unlock
	intlist* list = argStruct;
	// Pop nodes
	while (threads_readers_run) {
		intlist_pop_tail(list);
	}
	// Lock
	if ((rc = pthread_mutex_lock(intlist_get_mutex(list))) != 0) { // If successful, the pthread_mutex_lock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_lock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Inc counter
	threads_readers_finish += 1;
	// Unlock
	if ((rc = pthread_mutex_unlock(intlist_get_mutex(list))) != 0) { // If successful, the pthread_mutex_unlock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_unlock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Finish
	pthread_exit(NULL);
}
void *thrd_garbage_collector(void *argStruct) {
	// Garbage Collector – the garbage collector waits until the list has more than MAX items. Once it
	// has, the garbage collector removes half of the elements in the list (from the tail, rounded up).
	// In addition, the garbage collector prints the number of items removed from the list. Output a
	// message like the following: "GC – 7 items removed from the list".
	int rc; // Variable for pthread_mutex_lock & pthread_mutex_unlock & pthread_cond_wait
	int k_to_remove = 0;
	intlist* list = argStruct;
	// Lock
	if ((rc = pthread_mutex_lock(intlist_get_mutex(list))) != 0) { // If successful, the pthread_mutex_lock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_lock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Wait & Clean
	while (threads_gc_run) { // If the list is empty (list->count == 0)
		// Sleep
		if ((rc = pthread_cond_wait(&count_garbage_collector, intlist_get_mutex(list))) != 0) { // Upon successful completion, a value of zero shall be returned; otherwise, an error number shall be returned to indicate the error.
			fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_wait()",strerror(rc));
			exit(EXIT_FAILURE);
		}
		// Clean
		if (intlist_size(list) >= global_max) {
			k_to_remove = (intlist_size(list)+1)/2;
			intlist_remove_last_k(list,k_to_remove);
			printf("GC - %d items removed from the list\n",k_to_remove);
		}
	}
	// Unlock
	if ((rc = pthread_mutex_unlock(intlist_get_mutex(list))) != 0) { // If successful, the pthread_mutex_unlock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_unlock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Finish
	pthread_exit(NULL);
}
void *thrd_timer() {
	int rc; // Variable for pthread_cond_signal
	// 6. Sleep for TIME seconds.
	sleep(global_time);
	// 7. Stop all running threads (safely, avoid deadlocks!)
	threads_readers_run = 0; // Stop readers threads
	while (threads_readers_finish < global_readers) { // Wait untill all pthreads die gracefully
		sleep(1);
	}
	threads_writers_run = 0; // Stop writers threads
	while (threads_writers_finish < global_writers) { // Wait untill all pthreads die gracefully
		sleep(1);
	}
	threads_gc_run = 0; // Stop Garbage Collector thread
	if ((rc = pthread_cond_signal(&count_garbage_collector)) != 0) { // If successful, the pthread_cond_signal() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_signal()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Finish
	pthread_exit(NULL);
}
// Functions
int program_end(int error, intlist* list, pthread_t* threads_writers, pthread_t* threads_readers) {
	int res = 0;
	int rc = 0;
	if (threads_writers) {
		free(threads_writers);
		threads_writers = NULL;
	}
	if (threads_readers) {
		free(threads_readers);
		threads_readers = NULL;
	}
	if (list) {
		intlist_destroy(&list);
	}
	if ((rc = pthread_cond_destroy(&count_garbage_collector)) != 0) { // If successful, the pthread_cond_destroy() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_destroy()",strerror(rc));
		res = -1;
	}
	if ((rc = pthread_attr_destroy(&attr)) != 0) { // On success, these functions return 0; on error, they return a nonzero error number.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_attr_destroy()",strerror(rc));
		res = -1;
	}
	if ((error == -1)||(res == -1)) {
		fprintf(stderr,ERROR_EXIT_MSG);
	}
	return res;
}
void intlist_init(intlist* list) { // init - initialize the list. You may assume the argument is not a previously initialized or destroyed list.
	// You may assume init() and destroy() are called once for each list and not concurrently with any other
	// calls for that list, i.e., these methods do not need to be thread-safe.
	if (list == NULL) {
		return;
	}
	// Init variables
	int rc; // Variable for pthread_mutex_init & pthread_cond_init
	intlist_node_t* nil;
	// Memory allocation
	if ((nil = (intlist_node_t *)malloc(sizeof(intlist_node_t))) == NULL) { // The malloc() function return a pointer to the allocated memory that is suitably aligned for any kind of variable. On error, these functions return NULL.
		fprintf(stderr,F_ERROR_MALLOC_NODE_MSG);
		exit(EXIT_FAILURE);
	}
	// Init the nil element
	nil->val = 2147483647;
	nil->prev = NULL;
	nil->next = NULL;
	// Init the list element
	if ((rc = pthread_mutexattr_init(&(list->attr))) != 0) { // Upon successful completion, pthread_mutexattr_init() shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutexattr_init()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	if ((rc = pthread_mutexattr_settype(&(list->attr),PTHREAD_MUTEX_RECURSIVE)) != 0) { // If successful, the pthread_mutexattr_settype() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutexattr_settype()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	if ((rc = pthread_mutex_init(&(list->lock), &(list->attr))) != 0) { // If successful, the pthread_mutex_init() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_init()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	if ((rc = pthread_cond_init(&(list->cond_new_insert), NULL)) != 0) { // If successful, the pthread_cond_init() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_init()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	list->nil = nil;
	list->count = 0;
	list->head = list->nil;
	list->tail = list->nil;
}
void intlist_destroy(intlist** list) { // destroy – frees all memory used by the list, including any of its items.
	// You may assume init() and destroy() are called once for each list and not concurrently with any other
	// calls for that list, i.e., these methods do not need to be thread-safe.
	// Replace to "intlist**" as aproved at: http://moodle.tau.ac.il/mod/forum/discuss.php?d=21815
	intlist* list_friendly = *list;
	if ((list_friendly == NULL)||(list_friendly->nil == NULL)) {
		return;
	}
	int rc; // Variable for pthread_mutex_destroy & pthread_cond_destroy
	if (list_friendly->head != list_friendly->nil) { // If the list is not empty (list->count > 0)
		intlist_remove_last_k(list_friendly,list_friendly->count); // Remove all items from the list
	}
	free(list_friendly->nil);
	list_friendly->head = NULL;
	list_friendly->tail = NULL;
	list_friendly->nil = NULL;
	// Destroy
	if ((rc = pthread_mutex_destroy(&(list_friendly->lock))) != 0) { // If successful, the pthread_mutex_destroy() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_destroy()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	if ((rc = pthread_mutexattr_destroy(&(list_friendly->attr))) != 0) { // Upon successful completion, pthread_mutexattr_destroy() shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutexattr_destroy()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	if ((rc = pthread_cond_destroy(&(list_friendly->cond_new_insert))) != 0) { // If successful, the pthread_cond_destroy() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_destroy()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	free(*list);
	*list = NULL;
	list = NULL;
}
void intlist_push_head(intlist* list, int value) { // push_head – receives an int, and adds it to the head of the list.
	if ((list == NULL)||(list->nil == NULL)) {
		return;
	}
	// Init variables
	int rc; // Variable for pthread_mutex_lock & pthread_mutex_unlock & pthread_cond_signal
	intlist_node_t* node;
	// Memory allocation
	if ((node = (intlist_node_t *)malloc(sizeof(intlist_node_t))) == NULL) { // The malloc() function return a pointer to the allocated memory that is suitably aligned for any kind of variable. On error, these functions return NULL.
		fprintf(stderr,F_ERROR_MALLOC_NODE_MSG);
		exit(EXIT_FAILURE);
	}
	// Prepare the new node
	node->val = value;
	node->prev = list->nil;
	// Lock
	if ((rc = pthread_mutex_lock(&(list->lock))) != 0) { // If successful, the pthread_mutex_lock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_lock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Push to head
	if (list->head != list->nil) { // If the list is not empty (list->count > 0)
		node->next = list->head;
		node->next->prev = node;
	} else { // First element
		node->next = list->nil;
		list->tail = node;
	}
	list->head = node;
	list->count += 1;
	// Signal if someone is waiting to pop from the tail
	if ((rc = pthread_cond_signal(&(list->cond_new_insert))) != 0) { // If successful, the pthread_cond_signal() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_signal()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Unlock
	if ((rc = pthread_mutex_unlock(&(list->lock))) != 0) { // If successful, the pthread_mutex_unlock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_unlock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
}
int intlist_pop_tail(intlist* list) { // pop_tail – removes an item from the tail, and returns its value.
	// The operation pop_tail() is blocking, i.e., if the list is empty – wait until an item is available, and then pop it.
	if ((list == NULL)||(list->nil == NULL)) {
		return -1;
	}
	// Init variables
	int ret = 0;
	int rc; // Variable for pthread_mutex_lock & pthread_mutex_unlock & pthread_cond_wait
	// Lock
	if ((rc = pthread_mutex_lock(&(list->lock))) != 0) { // If successful, the pthread_mutex_lock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_lock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Wait
	while (list->head == list->nil) { // If the list is empty (list->count == 0)
		if ((rc = pthread_cond_wait(&(list->cond_new_insert), &(list->lock))) != 0) { // Upon successful completion, a value of zero shall be returned; otherwise, an error number shall be returned to indicate the error.
			fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_wait()",strerror(rc));
			exit(EXIT_FAILURE);
		}
	}
	// Pop the tail
	ret = list->tail->val;
	if (list->head->next == list->nil) { // If there is only one item in the list (list->count == 1)
		free(list->tail); // Free the old node from the memory
		list->head = list->nil;
		list->tail = list->nil;
	} else { // There is more then one item in the list
		list->tail = list->tail->prev; // Move the tail pointer
		free(list->tail->next); // Free the old node from the memory
		list->tail->next = list->nil; // Delete the link to the old node
	}
	list->count -= 1;
	// Unlock
	if ((rc = pthread_mutex_unlock(&(list->lock))) != 0) { // If successful, the pthread_mutex_unlock() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_mutex_unlock()",strerror(rc));
		exit(EXIT_FAILURE);
	}
	// Return
	return ret;
}
void intlist_remove_last_k(intlist* list, int k) { // remove_last_k – removes k items from the tail, without returning any value.
	// When remove_last_k() is called with a k larger than the list size, it removes whatever items are in the list and finishes.
	if ((list == NULL)||(list->nil == NULL)||(k < 0)) { // http://moodle.tau.ac.il/mod/forum/discuss.php?d=22102
		return;
	}
	// Init variables
	int i = 0;
	// Remove k items
	while ((i < k)&&(0 < list->count)) {
		intlist_pop_tail(list);
		i += 1;
	}
}
int intlist_size(intlist* list) { // size – returns the number of items currently in the list.
	if ((list == NULL)||(list->nil == NULL)) {
		return -1;
	}
	return list->count;
}
pthread_mutex_t* intlist_get_mutex(intlist* list) { // get_mutex – returns the mutex used by this list.
	if ((list == NULL)||(list->nil == NULL)) {
		return NULL;
	}
	return &(list->lock);
}
int main(int argc, char *argv[]) {
	// General variable
	char* endptr_WNUM; // strtol for global_writers
	char* endptr_RNUM; // strtol for global_readers
	char* endptr_MAX; // strtol for global_max
	char* endptr_TIME; // strtol for global_time
	int i; // tmp loop var
	int tmpListSize = 0;
	int rc; // Variable for pthread_create & pthread_cond_init & pthread_cond_destroy
	int pthread_create_try = 0;
	intlist* list = NULL;
	pthread_t threads_support[2];
	pthread_t* threads_writers;
	pthread_t* threads_readers;
	// Check correct call structure
	if (argc != 5) {
		if (argc < 5) {
			printf(USAGE_OPERANDS_MISSING_MSG,argv[0]);
		} else {
			printf(USAGE_OPERANDS_SURPLUS_MSG,argv[0]);
		}
		return EXIT_FAILURE;
	}
	global_writers = strtol(argv[1], &endptr_WNUM, 10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (global_writers == LONG_MAX || global_writers == LONG_MIN)) || (errno != 0 && global_writers == 0)) {
		fprintf(stderr,F_ERROR_STRTOL_MSG,strerror(errno));
		return errno;
	}
	global_readers = strtol(argv[2], &endptr_RNUM, 10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (global_readers == LONG_MAX || global_readers == LONG_MIN)) || (errno != 0 && global_readers == 0)) {
		fprintf(stderr,F_ERROR_STRTOL_MSG,strerror(errno));
		return errno;
	}
	global_max = strtol(argv[3], &endptr_MAX, 10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (global_max == LONG_MAX || global_max == LONG_MIN)) || (errno != 0 && global_max == 0)) {
		fprintf(stderr,F_ERROR_STRTOL_MSG,strerror(errno));
		return errno;
	}
	global_time = strtol(argv[4], &endptr_TIME, 10); // If an underflow occurs. strtol() returns LONG_MIN. If an overflow occurs, strtol() returns LONG_MAX. In both cases, errno is set to ERANGE.
	if ((errno == ERANGE && (global_time == LONG_MAX || global_time == LONG_MIN)) || (errno != 0 && global_time == 0)) {
		fprintf(stderr,F_ERROR_STRTOL_MSG,strerror(errno));
		return errno;
	}
	if ( (endptr_WNUM == argv[1])||(endptr_RNUM == argv[2])||(endptr_MAX == argv[3])||(endptr_TIME == argv[4]) ) { // Empty string
		printf(USAGE_OPERANDS_MISSING_MSG,argv[0]);
		return EXIT_FAILURE;
	}
	if ( (global_writers < 1)||(global_readers < 1)||(global_max < 1)||(global_time < 1) ) { // Not positive
		printf(USAGE_NUM_LESS_THEN_ONE_MSG);
		return EXIT_FAILURE;
	}
	// Init threads array
	if ((threads_writers = (pthread_t *)malloc(sizeof(pthread_t)*global_writers)) == NULL) { // The malloc() function return a pointer to the allocated memory that is suitably aligned for any kind of variable. On error, these functions return NULL.
		fprintf(stderr,F_ERROR_MALLOC_LIST_MSG);
		return EXIT_FAILURE;
	}
	if ((threads_readers = (pthread_t *)malloc(sizeof(pthread_t)*global_readers)) == NULL) { // The malloc() function return a pointer to the allocated memory that is suitably aligned for any kind of variable. On error, these functions return NULL.
		fprintf(stderr,F_ERROR_MALLOC_THREADS_MSG);
		return EXIT_FAILURE;
	}
	// Init the attr var
	if ((rc = pthread_attr_init(&attr)) != 0) { // On success, these functions return 0; on error, they return a nonzero error number.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_attr_init()",strerror(rc));
		return program_end(-1,list,threads_writers,threads_readers);
	}
	if ((rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0) { // On success, pthread_join() returns 0; on error, it returns an error number.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_attr_setdetachstate()",strerror(rc));
		return program_end(-1,list,threads_writers,threads_readers);
	}
	// 1. Define and initialize a global doubly-linked list of integers.
	if ((list = (intlist *)malloc(sizeof(intlist))) == NULL) { // The malloc() function return a pointer to the allocated memory that is suitably aligned for any kind of variable. On error, these functions return NULL.
		fprintf(stderr,F_ERROR_MALLOC_LIST_MSG);
		return program_end(-1,list,threads_writers,threads_readers);
	}
	intlist_init(list);
	// 2. Create a condition variable for the garbage collector. (different than the condition variable used internally by the list's pop_tail operation)
	if ((rc = pthread_cond_init(&count_garbage_collector, NULL)) != 0) { // If successful, the pthread_cond_init() function shall return zero; otherwise, an error number shall be returned to indicate the error.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_cond_init()",strerror(rc));
		return program_end(-1,list,threads_writers,threads_readers);
	}
	// 3. Create a thread for the garbage collector.
	if ((rc = pthread_create(&threads_support[0], &attr, thrd_garbage_collector, list)) != 0) { // On success, pthread_create() returns 0; on error, it returns an error number, and the contents of *thread are undefined.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"GC pthread_create()",strerror(rc));
		return program_end(-1,list,threads_writers,threads_readers);
	}
	// 4. Create WNUM threads for the writers.
	for (i=0;i<global_writers;i++) {
		pthread_create_try = 0;
		while (pthread_create_try < 2) { // Give it two tries
			if ((rc = pthread_create(&threads_writers[i], &attr, thrd_writers, list)) != 0) { // On success, pthread_create() returns 0; on error, it returns an error number, and the contents of *thread are undefined.
				if (pthread_create_try == 0) { // pthread_create failed for the first time
					pthread_create_try += 1;
					sleep(1);
				} else { // pthread_create failed for the second time
					fprintf(stderr,F_ERROR_GENERAL_MSG,"writers pthread_create()",strerror(rc));
					return program_end(-1,list,threads_writers,threads_readers);
				}
			} else {
				pthread_create_try = 2;
			}
		}
	}
	// 5. Create RNUM threads for the readers.
	for (i=0;i<(int)global_readers;i++) {
		pthread_create_try = 0;
		while (pthread_create_try < 2) { // Give it two tries
			if ((rc = pthread_create(&threads_readers[i], &attr, thrd_readers, list)) != 0) { // On success, pthread_create() returns 0; on error, it returns an error number, and the contents of *thread are undefined.
				if (pthread_create_try == 0) { // pthread_create failed for the first time
					pthread_create_try += 1;
					sleep(1);
				} else { // pthread_create failed for the second time
					fprintf(stderr,F_ERROR_GENERAL_MSG,"readers pthread_create()",strerror(rc));
					return program_end(-1,list,threads_writers,threads_readers);
				}
			} else {
				pthread_create_try = 2;
			}
		}
	}
	// 6. Sleep for TIME seconds.
	// 7. Stop all running threads (safely, avoid deadlocks!)
	if ((rc = pthread_create(&threads_support[1], &attr, thrd_timer, NULL)) != 0) { // On success, pthread_create() returns 0; on error, it returns an error number, and the contents of *thread are undefined.
		fprintf(stderr,F_ERROR_GENERAL_MSG,"timer pthread_create()",strerror(rc));
		return program_end(-1,list,threads_writers,threads_readers);
	}
	// Join all threads
	for (i=0;i<2;i++) {
		if ((rc = pthread_join(threads_support[i], NULL)) != 0) { // On success, pthread_join() returns 0; on error, it returns an error number.
			fprintf(stderr,F_ERROR_GENERAL_MSG,"pthread_join()",strerror(rc));
			return program_end(-1,list,threads_writers,threads_readers);
		}
	}
	for (i=0;i<global_writers;i++) {
		if ((rc = pthread_join(threads_writers[i], NULL)) != 0) { // On success, pthread_join() returns 0; on error, it returns an error number.
			fprintf(stderr,F_ERROR_GENERAL_MSG,"writers pthread_join()",strerror(rc));
			return program_end(-1,list,threads_writers,threads_readers);
		}
	}
	for (i=0;i<global_readers;i++) {
		if ((rc = pthread_join(threads_readers[i], NULL)) != 0) { // On success, pthread_join() returns 0; on error, it returns an error number.
			fprintf(stderr,F_ERROR_GENERAL_MSG,"readers pthread_join()",strerror(rc));
			return program_end(-1,list,threads_writers,threads_readers);
		}
	}
	// 8. Print the size of the list as well as all items within it.
	tmpListSize = intlist_size(list);
	for (i=0;i<tmpListSize;i++) {
		printf("%d\n",intlist_pop_tail(list));
	}
	printf(LIST_SIZE_MSG,tmpListSize);
	// 9. Cleanup. Exit gracefully
	pthread_exit(NULL);
	return program_end(0,list,threads_writers,threads_readers);
}
