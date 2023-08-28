set -e

PLUGIN_PATH="/root/tugraph/LightningGraph/plugins"
SERVER="127.0.0.1:7071"
INCLUDE_DIR="/usr/local/include"
LIBLGRAPH="/usr/local/lib64/liblgraph.so"
python=$(which python)

function compile_and_install() {
  g++ -fno-gnu-unique -fPIC -g --std=c++14 -I$INCLUDE_DIR -rdynamic -O3 -fopenmp -o $1.so $PLUGIN_PATH/$1.cpp $LIBLGRAPH -shared
  $python lgraph_plugin.py -a $SERVER -c Remove -n $1
  $python lgraph_plugin.py -a $SERVER -c Load -n $1 -p $1.so
}

compile_and_install khop
compile_and_install pagerank
compile_and_install weakly_connected_components
compile_and_install label_propagation
compile_and_install single_source_shortest_path
compile_and_install breadth_first_search
compile_and_install clustering_coefficient
