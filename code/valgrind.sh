valgrind --leak-check=full ./CiCTrie insert scripts/inserts_sample.bin lookup scripts/lookups_sample.bin remove scripts/removes_sample.bin 2>&1 | tee output.txt | grep "ERROR SUMMARY"
