all: kilo

kilo: src/kilo.c
	$(CC) src/kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

clean:
	rm kilo