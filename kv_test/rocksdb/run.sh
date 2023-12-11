#! /bin/bash
set -e

echo "==================1=================="
for value_size in 16 64 256 1024 4096 16384 65536
do
rm lmdb_data/ rocksdb_data/ -rf
echo "value_size = $value_size"
./rocksdb_lmdb --test_mode=prepare_data --batch_insert=false --thread_num=10 --total_kv_number=1000000 --value_size=$value_size
du -sh lmdb_data
du -sh rocksdb_data
done

echo "==================2=================="
for value_size in 16 64 256 1024 4096 16384 65536
do
rm lmdb_data/ rocksdb_data/ -rf
echo "value_size = $value_size"
./rocksdb_lmdb --test_mode=prepare_data --batch_insert=true --batch_insert_num=10 --thread_num=10 --total_kv_number=1000000 --value_size=$value_size
du -sh lmdb_data
du -sh rocksdb_data
done

echo "==================3=================="
for value_size in 16 64 256 1024 4096 16384 65536
do
rm lmdb_data/ rocksdb_data/ -rf
echo "value_size = $value_size"
./rocksdb_lmdb --test_mode=prepare_data --batch_insert=true --batch_insert_num=100 --thread_num=10 --total_kv_number=1000000 --value_size=$value_size
du -sh lmdb_data
du -sh rocksdb_data
done

echo "==================4=================="
#------------------------------------------------------------------------------------------------------------------
for value_size in 16 64 256 1024 4096 16384 65536
do
rm lmdb_data/ rocksdb_data/ -rf
echo "value_size = $value_size"
./rocksdb_lmdb --test_mode=prepare_data --batch_insert=true --batch_insert_num=100 --thread_num=10 --total_kv_number=1000000 --value_size=$value_size
./rocksdb_lmdb --test_mode=single_kv_random_update --warm_up=true --thread_num=10 --total_kv_number=1000000 --random_test_num=1000000 --point_read_only=true --value_size=$value_size
./rocksdb_lmdb --test_mode=single_kv_random_update --warm_up=true --thread_num=10 --total_kv_number=1000000 --random_test_num=1000000 --iter_range_scan=true --iter_range_scan_num=10 --value_size=$value_size
./rocksdb_lmdb --test_mode=single_kv_random_update --warm_up=true --thread_num=10 --total_kv_number=1000000 --random_test_num=1000000 --point_write_only=true --value_size=$value_size
./rocksdb_lmdb --test_mode=single_kv_random_update --warm_up=true --thread_num=10 --total_kv_number=1000000 --random_test_num=1000000 --point_read_write=true --value_size=$value_size
./rocksdb_lmdb --test_mode=single_kv_random_update --warm_up=true --thread_num=10 --total_kv_number=1000000 --random_test_num=1000000 --read_write_scan=true --iter_range_scan_num=10 --value_size=$value_size
du -sh lmdb_data
du -sh rocksdb_data
done
