all: cat

mycat: cat.c
	gcc $< -o $@

clean: 
	rm -f cat

tests:
	./cat < cat.c > cat2.c && diff cat.c cat2.c && echo OK
	curl neerc.ifmo.ru/~os/blob.bin | ./cat > blob.bin && curl neerc.ifmo.ru/~os/blob.bin > blob2.bin && diff blob.bin blob2.bin && echo OK
