#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <sys/time.h>
#include "multi-lookup.h"
#include "util.h"

sem_t mutex;

void* consumer(void *args) {
	struct cthread_arg_struct *arguments = (struct cthread_arg_struct*) args; //cast to usable format
	FILE* logfile = fopen(arguments->consLog, "ab+"); //open logfile, create if nonexistant

	char* hostnameBuf = malloc(MAX_NAME_LENGTH);
	char* ipStr = malloc(MAX_IP_LENGTH);
	int hostnamesResolved = 0;
	while (*(arguments->filesServiced) < *(arguments->numFiles) || *(arguments->itemsInBuffer) > 0) { //while producer threads are still working or the buffer is not empty
		while (*(arguments->itemsInBuffer) == 0 && *(arguments->filesServiced) < *(arguments->numFiles)); //wait while buffer is empty and producers are still working
		sem_wait(&mutex);
		if (*(arguments->itemsInBuffer) > 0) { //ensures consumer has updated info inside of lock
			strncpy(hostnameBuf, arguments->buffer + 255 * (*(arguments->itemsInBuffer) - 1), MAX_NAME_LENGTH); //copy hostname to hostnameBuf
			hostnameBuf[strlen(hostnameBuf) - 1] = 0; //remove newline
			if (dnslookup(hostnameBuf, ipStr, MAX_IP_LENGTH) != 0) { //if dnslookup unsuccessful
				fprintf(logfile, "%s, NOT_RESOLVED\n", hostnameBuf);
			} else {
				fprintf(logfile, "%s, %s\n", hostnameBuf, ipStr);
				hostnamesResolved++;
			}
			*(arguments->itemsInBuffer) = *(arguments->itemsInBuffer) - 1; //update number of items in buffer
		}
		sem_post(&mutex);
	}
	fclose(logfile);
	printf("thread <%lu> resolved %d hostnames\n", pthread_self(), hostnamesResolved);
	free(arguments);
	free(hostnameBuf);
	free(ipStr);
	pthread_exit(NULL);
}

void* producer(void *args) {
	struct pthread_arg_struct *arguments = (struct pthread_arg_struct*) args; //cast generic args pointer to arg struct pointer
	int thread_filesServiced = 0;
	char* linebuf = NULL;
	if (arguments->currFileNum == -1) { //if thread has no file assigned
		while (*(arguments->filesServiced) < *(arguments->numFiles)); //wait until all files have been serviced
		free(arguments);
		free(linebuf);
		pthread_exit(NULL); //exit gracefully
	}
	size_t len = 0;
	FILE* readfile;
	sem_wait(&mutex);
	FILE *logfile = fopen(arguments->prodLog, "ab+"); //open logfile, create if nonexistant
	sem_post(&mutex);
	while (*(arguments->filesServiced) < *(arguments->numFiles)) { //while there are still files to be processed
		sem_wait(&mutex);
		*(arguments->filesAssigned) = *(arguments->filesAssigned) + 1;
		sem_post(&mutex);
		readfile = fopen(arguments->files[arguments->currFileNum], "r");

		linebuf = NULL;
		len = 0;
		while (getline(&linebuf, &len, readfile) != -1) { //read input file line by line
			if (len <= MAX_NAME_LENGTH) { //verify that length of hostname is valid
				while (*(arguments->itemsInBuffer) > 9 && *(arguments->filesServiced) < *(arguments->numFiles)); //wait for space to open up in shared array
				sem_wait(&mutex);
				if (*(arguments->itemsInBuffer) <= 9) { //make sure producer working with updated info in lock
					fprintf(logfile, "%s", linebuf);
					strncpy(arguments->buffer + 255 * *(arguments->itemsInBuffer), linebuf, len); //if space exists in buffer, print hostname to proper "slot"
					*(arguments->itemsInBuffer) = *(arguments->itemsInBuffer) + 1; //increment itemsInBuffer accordingly
				}
				sem_post(&mutex);
			} else {
				fprintf(stderr, "Hostname length of %lu is longer than maximum allowed length %d\n", len, MAX_NAME_LENGTH);
			}

		}
		sem_wait(&mutex);
		fclose(readfile);
		if (*(arguments->filesServiced) < *(arguments->numFiles)) {
			*(arguments->filesServiced) = *(arguments->filesServiced) + 1;
			thread_filesServiced++;
		}
		arguments->currFileNum = *(arguments->filesServiced);
		sem_post(&mutex);
	}
	fclose(logfile);
	sem_wait(&mutex);
	printf("thread <%lu> serviced %d files\n", pthread_self(), thread_filesServiced);
	sem_post(&mutex);
	free(linebuf);
	free(arguments);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	sem_init(&mutex, 0, 1); //initialize mutex
	struct timeval startTime, endTime;
	gettimeofday(&startTime, NULL); //record start time

	///////////// Value/Bounds Checking //////////////////
	int numProd; int numCons; char* prodLog; char* consLog; int numFiles = argc - 5; char* files[numFiles];
	if (argc < 6) {printf("Correct usage: multi-lookup <# requester> <# resolver> <requester log> <resolver log> [<data file> ...]\n"); exit(-1);}
	if (atoi(argv[1]) > MAX_REQUESTER_THREADS || atoi(argv[1]) < 1) {fprintf(stderr, "Number of requester threads must be between 1 and 10 (inclusive)\n"); exit(-1);}
	if (atoi(argv[2]) > MAX_RESOLVER_THREADS || atoi(argv[2]) < 1) {fprintf(stderr, "Number of resolver threads must be between 1 and 10 (inclusive)\n"); exit(-1);}
	if (numFiles > MAX_INPUT_FILES) {fprintf(stderr, "Exceeded maximum input files. Max is 100.\n");}
	numProd = atoi(argv[1]);
	numCons = atoi(argv[2]);
	prodLog = argv[3];
	consLog = argv[4];
	/////////////////////////////////////////////////////
	remove(prodLog); remove(consLog); //erase logfiles if they exist
	for (int i = 0; i < numFiles; ++i) {files[i] = argv[i+5];} //parse provided files into an array

	pthread_t producer_threads[numProd]; //name producer and consumer threads
	pthread_t consumer_threads[numCons];
	void* buf = malloc(4096);
	int numItemsInBuffer = 0;
	int filesServiced = 0;
	int pass_numFiles = numFiles;
	int filesAssigned = 0;
	int hostnamesResolved = 0;

	for (int i = 0; i < numProd; ++i) { //create producer threads
		struct pthread_arg_struct *arguments = malloc(sizeof(struct pthread_arg_struct)); //allocate space for argument struct
		arguments->prodLog = prodLog; //set arguments within arg struct
		arguments->files = files;
		arguments->buffer = buf;
		arguments->itemsInBuffer = &numItemsInBuffer;
		arguments->filesServiced = &filesServiced;
		arguments->numFiles = &pass_numFiles;
		arguments->filesAssigned = &filesAssigned;
		if (i < numFiles) {arguments->currFileNum = i;} //if there are still producers to assign files to, do so
		else {arguments->currFileNum = -1;} //Otherwise assign -1
		pthread_create(&(producer_threads[i]), NULL, producer, arguments); //create thread operating on producer func, with specified args
	}

	for (int i = 0; i < numCons; ++i) { //create consumer threads
		struct cthread_arg_struct *cons_arguments = malloc(sizeof(struct cthread_arg_struct));
		cons_arguments->buffer = buf;
		cons_arguments->consLog = consLog;
		cons_arguments->itemsInBuffer = &numItemsInBuffer;
		cons_arguments->hostnamesResolved = &hostnamesResolved;
		cons_arguments->numFiles = &pass_numFiles;
		cons_arguments->filesServiced = &filesServiced;
		pthread_create(&(consumer_threads[i]), NULL, consumer, cons_arguments);
	}

	for (int i = 0; i < numProd; ++i) {
		pthread_join(producer_threads[i], NULL); //wait for producer threads to finish
	}
	for (int i = 0; i < numCons; ++i) {pthread_join(consumer_threads[i], NULL);}

	gettimeofday(&endTime, NULL); //record end time
	printf("./multi-lookup: total time is %f seconds\n", ((endTime.tv_sec * 1000000 + endTime.tv_usec - startTime.tv_sec * 1000000 - startTime.tv_usec)/1000000.0));
	sem_destroy(&mutex); //destroy mutex
	free(buf);
	return 0;
}
