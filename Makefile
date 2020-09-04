all: shell

shell: shell.c
	gcc -Wall -o shell shell.c

clean:
	rm -f shell
