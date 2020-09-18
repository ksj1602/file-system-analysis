CC=gcc
CFLAGS= -Wall -Wextra

default: extract_info.c
	$(CC) extract_info.c -o extract-info $(CFLAGS)
