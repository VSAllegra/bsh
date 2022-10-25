CFLAGS = -Wall -Wextra -Werror

bash: bsh.c list.h mu.c mu.h
	gcc -o $@ $^

clean:
	rm -f mcron

.PHONY: all clean