warmup2: warmup2.o my402list.o
	gcc -o warmup2 -g warmup2.o my402list.o -lpthread -lm

my402list.o: my402list.c my402list.h 
	gcc -g -c -Wall my402list.c

warmup2.o: warmup2.c my402list.h
	gcc -g -c -Wall warmup2.c

clean:
	rm -f *.o warmup2
