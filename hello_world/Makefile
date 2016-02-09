all: hello
hello: hello.o
	gcc -s $< -o $@
hello.o: hello.c
	gcc -c $< -o $@
clean:
	rm -f hello hello.o
