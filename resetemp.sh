cp ../emptydisk.img ./
make cp
ext2_cp emptydisk.img FILE_ONEBLK.txt /FILE_ONEBLK.txt
rm -f comp
./ext2_dump emptydisk.img > comp
diff comp A4-self-test/case1-cp.txt  