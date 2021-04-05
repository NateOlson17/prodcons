# Makefile v1 for CSCI3753-S21 PA3
# Do not modify anything other MSRCS & MHDRS

CC = gcc
CFLAGS = -Wextra -Wall -g -std=gnu99
INCLUDES = 
LFLAGS = 
LIBS = -lpthread

MAIN = multi-lookup

# Add any additional .c files to MSRCS and .h files to MHDRS
MSRCS = multi-lookup.c
MHDRS = multi-lookup.h

SRCS = $(MSRCS) util.c
HDRS = $(MHDRS) util.h

OBJS = $(SRCS:.c=.o) 

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean: 
	$(RM) *.o *~ $(MAIN)

SUBMITFILES = $(MSRCS) $(MHDRS) Makefile README
submit: 
	@read -r -p "Enter your identikey username: " username; \
	echo; echo Bundling the following files for submission; \
	tar --transform "s|^|PA3-$$username/|" -cvf PA3-$$username.txt $(SUBMITFILES); \
	echo; echo Please upload the file PA3-$$username.txt to Canvas to complete your submission; echo
