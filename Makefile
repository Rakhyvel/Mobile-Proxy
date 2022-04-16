# -- Authors --
# Joseph Shimel
# Austin James Connick
make:
	gcc -Wall main.c message.c queue.c -o cproxy -DCLIENT
	gcc -Wall main.c message.c queue.c -o sproxy
