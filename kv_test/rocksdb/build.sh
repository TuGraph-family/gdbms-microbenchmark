#! /bin/bash

cd build

cmake ../ -DCMAKE_INSTALL_PREFIX=/home/xxx/install -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFORCE_SSE42=ON -DWITH_JEMALLOC=ON

make rocksdb_lmdb -j20
