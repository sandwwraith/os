all: hello_world
hello_world: hello_world.o
	gcc -s $< -o $@
hello_world.o: hello_world.c
	gcc -c $< -o $@
clean:
	rm -f hello_world hello_world.o
