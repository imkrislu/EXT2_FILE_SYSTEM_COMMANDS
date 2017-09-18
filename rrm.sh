cp ../twolevel.img ./
make rm
ext2_rm twolevel.img /afile
rm -f comp
./ext2_dump twolevel.img > comp
diff comp A4-self-test/case2-rm-afile.txt  