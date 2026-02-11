
comment_wasm: main.c
	$(CC) -o comment_wasm -g -Wall -Wextra  main.c

clean:
	rm comment_wasm

.PNONY: clean
