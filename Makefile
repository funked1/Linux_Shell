all:
	gcc -o main main.c

clean:
	rm main

debug:
	gcc -g main.c
