.phony all:
all: compile_all

compile_all: diskinfo.c disklist.c diskget.c diskput.c
	gcc -Wall common_functions.c diskinfo.c -o diskinfo
	gcc -Wall common_functions.c disklist.c -o disklist
	gcc -Wall common_functions.c diskget.c -o diskget
	gcc -Wall common_functions.c diskput.c -o diskput

.PHONY clean:
clean:
	-rm -rf *.o *.exe diskinfo disklist diskget diskput