cfile=*.c
main:${cfile}
	gcc -Wall -Wextra -Wconversion -ggdb -fsanitize=address,leak,undefined -o main $^ -lreadline
