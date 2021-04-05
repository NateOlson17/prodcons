
#define ARRAY_SIZE 10
#define MAX_INPUT_FILES 100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS 10
#define MAX_NAME_LENGTH 255
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

void* producer(void *args);
void* consumer(void *args);

struct pthread_arg_struct {
	void* buffer;
	char* prodLog;
	int currFileNum;
	int* itemsInBuffer;
	int* filesServiced;
	int* numFiles;
	int* filesAssigned;
	char** files;
};

struct cthread_arg_struct {
	void* buffer;
	char* consLog;
	int* itemsInBuffer;
	int* hostnamesResolved;
	int* filesServiced;
	int* numFiles;
};
