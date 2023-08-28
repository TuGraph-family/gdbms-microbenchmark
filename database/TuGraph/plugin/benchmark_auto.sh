SEED_FILE="../../../seeds/twitter_rv.net-seed"
num_iterations=10

if [ ! -d "results" ]; then
    mkdir results
fi

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
python3 benchmark.py lpa ${num_iterations}

# wcc
echo "test wcc..."
python3 benchmark.py wcc

# sssp
echo "test single source shortest path"
python3 benchmark.py sssp ${SEED_FILE} 300

# bfs
echo "test breadth first search"
python3 benchmark.py bfs ${SEED_FILE} 300

# lcc
echo "test clustering coefficient"
python3 benchmark.py lcc
