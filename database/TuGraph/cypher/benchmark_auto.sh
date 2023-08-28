SEED_FILE="../../../seeds/twitter_rv.net-seed"

if [ ! -d "results" ]; then
    mkdir results
fi

python3 cypher-benchmark.py khop-single ${SEED_FILE} 1 300
python3 cypher-benchmark.py khop-single ${SEED_FILE} 2 300
python3 cypher-benchmark.py khop-single ${SEED_FILE} 3 10
python3 cypher-benchmark.py khop-single ${SEED_FILE} 4 10
python3 cypher-benchmark.py khop-single ${SEED_FILE} 5 10
python3 cypher-benchmark.py khop-single ${SEED_FILE} 6 10