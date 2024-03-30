CC = gcc
CFLAGS = -Wall -Werror -g

system_monitor: stats_functions.o system_monitor_concur.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c stats_functions.h
	$(CC) $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm *.o

.PHONY: help

help:
	@echo "make: compile code to system_monitor."
	@echo "make clean: remove auto-generated files."