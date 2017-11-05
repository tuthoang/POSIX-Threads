# Simply execute "make" in working directory to create executable "shell"
# Running "make clean" will remove the executable

all:
	gcc -m32 -c -o threads.o threads.c

clean:
	rm threads.o