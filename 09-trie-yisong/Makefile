all: unitrie multitrie

unitrie: unitrie.c
	gcc -Iinclude/ -Wall -g unitrie.c -o unitrie

multitrie: multitrie.c
	gcc -Iinclude/ -Wall -g multitrie.c -o multitrie -lm

clean:
	rm unitrie multitrie
