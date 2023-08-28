set -e

SERVER="127.0.0.1:7070"
INCLUDE_DIR="/usr/local/include"
LIBLGRAPH="/usr/local/lib64/liblgraph.so"
TUGRAPH_PROJECT="/root/tugraph-db/"
python=$(which python3)

function install() {
  echo "reinstalling $1"
  $python $TUGRAPH_PROJECT/plugins/lgraph_plugin.py -a $SERVER -c Remove -n $1
  $python $TUGRAPH_PROJECT/plugins/lgraph_plugin.py -a $SERVER -c Load -n $1 -p $1.so
}

function compile_and_install_khop() {
  echo "Compiling khop"
  g++ -fno-gnu-unique -fPIC -g --std=c++17 -I$INCLUDE_DIR -rdynamic -O3 -fopenmp -o khop.so $TUGRAPH_PROJECT/plugins/cpp/khop_kth.cpp $LIBLGRAPH -shared
  install khop
}

function compile_and_install() {
  echo "Compiling $1"
  g++ -fno-gnu-unique -fPIC -g --std=c++17 -I$INCLUDE_DIR -rdynamic -O3 -fopenmp -o $1.so  $TUGRAPH_PROJECT/plugins/cpp/$1_procedure.cpp $TUGRAPH_PROJECT/plugins/cpp/$1_core.cpp $LIBLGRAPH -shared
  install $1
}

compile_and_install_khop
compile_and_install pagerank
compile_and_install wcc
compile_and_install lpa
compile_and_install sssp
compile_and_install bfs
compile_and_install lcc