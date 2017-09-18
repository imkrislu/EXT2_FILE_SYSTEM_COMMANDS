all: mkdir cp ln rm restore

mkdir: helpers
	gcc -Wall ext2_mkdir.c helpers.o -o ext2_mkdir -g

cp: helpers
	gcc -Wall ext2_cp.c helpers.o -o ext2_cp -g

ln: helpers
	gcc -Wall ext2_ln.c helpers.o -o ext2_ln -g

restore: helpers
	gcc -Wall ext2_restore.c helpers.o -o ext2_restore -g

rm: helpers
	gcc -Wall ext2_rm.c helpers.o -o ext2_rm -g

checker: helpers
	gcc -Wall ext2_checker.c helpers.o -o ext2_checker -g

helpers: helpers.c helpers.h
	gcc -Wall helpers.c -c -g

clean:
	rm *.o
