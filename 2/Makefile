all:
	gcc -Wall solution.c parser.c -o shell


all_mem_leak: parser.c solution.c
	gcc $(GCC_FLAGS_MEM_LEAK) parser.c solution.c ../utils/heap_help/heap_help.c -ldl -rdynamic -o shell

clean:
	rm -f shell