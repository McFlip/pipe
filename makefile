all:  trans.c
	gcc -std=c99 -Wall -Wextra -ggdb -D_XOPEN_SOURCE=700 -o trans trans.c -lrt
clean:
	rm -f *.o trans
