srcDir = src
# testDir = testing

all: clean build

build: $(srcDir)/kilo.c
	$(CC) $(srcDir)/kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

# kiloScrollTest: src/test/kilo.scroll.test.c
# 	$(CC) src/test/kilo.scoll.c -o $(testDir)/kilo -Wall -Wextra -pedantic -std=c99

clean:
	test -f kilo && rm kilo || echo kilo binary does not exist