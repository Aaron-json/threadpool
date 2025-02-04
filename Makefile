CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pthread -O3

threadpool.o: threadpool.c
	$(CC) $(CFLAGS) -c threadpool.c

atomic-count.o: ./tests/atomic-count.c
	$(CC) $(CFLAGS) -c ./tests/atomic-count.c

atomic-count: atomic-count.o threadpool.o
	$(CC) $(CFLAGS) atomic-count.o threadpool.o -o atomic-count

run-atomic-count: atomic-count
	./atomic-count

atomic-count-memcheck: atomic-count
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --fair-sched=yes ./atomic-count

atomic-count-lockcheck: atomic-count
	valgrind --tool=helgrind --track-lockorders=yes ./atomic-count

clean:
	rm -f *.o *.out *.exe ./*.txt atomic-count
