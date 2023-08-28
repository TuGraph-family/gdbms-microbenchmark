SEED_FILE="../../../dataset/seeds/twitter_rv.net-seed"
num_iterations=10

# Khop
python3 benchmark.py khop ${SEED_FILE} 300 1 latency
python3 benchmark.py khop ${SEED_FILE} 300 2 latency
python3 benchmark.py khop ${SEED_FILE} 10 3 latency
python3 benchmark.py khop ${SEED_FILE} 10 4 latency
python3 benchmark.py khop ${SEED_FILE} 10 5 latency
python3 benchmark.py khop ${SEED_FILE} 10 6 latency

# pagerank
echo "test pagerank..."
python3 benchmark.py pagerank ${num_iterations}

# label propagation
echo "test label propagation..."
python3 benchmark.py label_propagation ${num_iterations}

# wcc
echo "test wcc..."
python3 benchmark.py weakly_connected_components

# sssp
echo "test singel source shortest path"
python3 benchmark.py sssp ${SEED_FILE} 300

# bfs
echo "test breadth first search"
python3 benchmark.py bfs ${SEED_FILE} 300

# lcc
echo "test clustering coefficient"
python3 benchmark.py lcc
