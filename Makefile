all:
	gcc -Wall -Werror main.c -o proj

run-server:
	gcc -Wall -Werror main.c -o proj
	./proj

run-client:
	gcc -Wall -DCLIENT main.c -o proj
	./proj

git-commit:
	git add .
	git commit -m "$(msg)"

git-push:
	git push origin master
