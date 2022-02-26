server:
	gcc -Wall -Werror main.c -o server

client:
	gcc -Wall -DCLIENT main.c -o client
