make:
	gcc -Wall cproxy.c message.c queue.c -o cproxy
	gcc -Wall sproxy.c message.c queue.c -o sproxy
