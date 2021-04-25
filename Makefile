srcDir = src
# testDir = testing

all: kilo

kilo: $(srcDir)/kilo.c
	$(CC) $(srcDir)/kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

# kiloScrollTest: src/test/kilo.scroll.test.c
# 	$(CC) src/test/kilo.scoll.c -o $(testDir)/kilo -Wall -Wextra -pedantic -std=c99

clean:
	rm kilo