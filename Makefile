CC = gcc
CFLAGS = -std=c11 -Wall -Werror -pthread

threadpool.o: threadpool.c
	$(CC) $(CFLAGS) -c threadpool.c

clean:
	rm -f *.o *.out *.exe ./*.txt
