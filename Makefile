make:
	gcc -Wall cproxy.c message.c -o cproxy
	gcc -Wall sproxy.c message.c -o sproxy
