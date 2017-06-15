Run: testcase.o hw2.o Disk.o Mount.o FileSystem.o
	gcc -g -ggdb -o Run testcase.o hw2.o Disk.o Mount.o FileSystem.o
testcase.o: testcase.c hw2.h Disk.h FileSystem.h
	gcc -c testcase.c
FileSystem.o: FileSystem.c FileSystem.h hw2.h Disk.h
	gcc -c FileSystem.c
Mount.o : Mount.c FileSystem.h hw2.h Disk.h
	gcc -c Mount.c
hw2.o: hw2.c hw2.h Disk.h
	gcc -c hw2.c
Disk.o: Disk.c Disk.h
	gcc -c Disk.c

