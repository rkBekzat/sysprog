GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

all: libcoro.c solution.c
	gcc $(GCC_FLAGS) libcoro.c solution.c

all_mem_leak: libcoro.c solution.c
	gcc $(GCC_FLAGS_MEM_LEAK) libcoro.c solution.c ../utils/heap_help/heap_help.c -ldl -rdynamic

test:
	./a.out 1 3 test1.txt test2.txt test3.txt test4.txt test5.txt
	python3 checker.py -f result.txt

clean:
	rm a.out
	rm result.txt


