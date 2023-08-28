import sys
from multiprocessing import Process, Value, Pool
from timeit import default_timer as timer
from datetime import datetime
from TuGraphClient import TuGraphClient


class CypherTest:
    def __init__(self):
        # host_port = "127.0.0.1:7073"
        host_port = "127.0.0.1:7071"
        username = "admin"
        password = "73@TuGraph"
        self.client = TuGraphClient(host_port, username, password)
        self.khop_cluases = [
            # "MATCH (n:user {name: %s}) -[:weight]-> (b) return count(distinct b)",
            # "MATCH (n:user {name: %s}) -[:weight]-> () -[:weight]-> (b) return count(distinct b)",
            "MATCH (n:user {name: %s}) -[:weight *0..1]-> (b) return count(distinct b)",
            "MATCH (n:user {name: %s}) -[:weight *0..2]-> (b) return count(distinct b)",
            "MATCH (n:user {name: %s}) -[:weight *0..3]-> (b) return count(distinct b)",
            "MATCH (n:user {name: %s}) -[:weight *0..4]-> (b) return count(distinct b)",
            "MATCH (n:user {name: %s}) -[:weight *0..5]-> (b) return count(distinct b)",
            "MATCH (n:user {name: %s}) -[:weight *0..6]-> (b) return count(distinct b)",
        ]
        self.outputfile = None

    def add_record(self, addition):
        print(addition)
        self.outputfile.write(addition + '\n')

    def run_cypher(self, cypher):
        return self.client.call_cypher(cypher, True, timeout=7200)

    def run_khop(self, depth, seeds):
        self.outputfile = open("./results/TuGraph-Cypher-%s-hop" % str(depth), 'w+')
        self.add_record("---------- %s-hop %s ----------" % (str(depth), datetime.now().strftime('%Y-%m-%d %H:%M:%S')))
        self.add_record("> " + self.khop_cluases[depth - 1])
        self.add_record("count root neigbor_size time")

        count = 0
        corr_count = 0
        elapsed_time = 0.0
        neighbor_count = 0.0
        for seed in seeds:
            count += 1
            if seed == "":
                continue
            clause = str(self.khop_cluases[depth - 1] % seed)
            try:
                res_code, res = self.run_cypher(clause)
                if res == "Task killed.":
                    info = "%d %s N/A Timeout" % (count, seed)
                else:
                    info = "%d %s %d %f" % (count, seed, res['result'][0][0], res['elapsed'])
                    elapsed_time += res['elapsed']
                    neighbor_count += res['result'][0][0]
                    corr_count += 1
                self.add_record(info)
            except Exception as e:
                info = "%d %s %s" % (count, seed, e)
                self.add_record(info)
                break
        self.add_record("response: %d/%d" % (corr_count, count))
        if corr_count == 0:
            self.add_record("average time: timeout all.")
            self.add_record("auverage neighbor_num: NULL")
        else:
            self.add_record("average time: %f" % (elapsed_time / corr_count))
            self.add_record("average neighbor_num: %f" % (neighbor_count / corr_count))

    def run_khop_throughput(self, depth, seed):
        clause = self.khop_cluases[depth - 1] % seed
        try:
            start_time = timer()
            res_code, res = self.run_cypher(clause)
            end_time = timer()
            info = "%s %d %f" % (seed, res['result'][0][0], res['elapsed'])
            elapsed_time = end_time - start_time
            neighbor_count = res['result'][0][0]
            return 1, seed, elapsed_time, neighbor_count
        except Exception as e:
            info = "%s %s" % (seed, e)
            print(info)
            return 0, seed, 0, 0


def get_seeds(seed_file):
    t = open(seed_file, 'r').read()
    return t.strip('\n').split(' ')


def usage():
    print("Usage: ")
    print("    python %s khop-single [seed_file] [depth] [root_count]" % sys.argv[0])
    print("    python %s khop [seed_file] [depth] [root_count] [thread_number]" % sys.argv[0])
    exit()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        usage()
        exit()
    if sys.argv[1] == "khop-single":
        if len(sys.argv) < 5:
            print("Usage: python %s khop-single [seed_file] [depth] [root_count]" % sys.argv[0])
            exit()
        seed_file = sys.argv[2]
        depth = int(sys.argv[3])
        count = int(sys.argv[4])
        ct = CypherTest()
        seeds = get_seeds(seed_file)
        if count <= len(seeds):
            ct.run_khop(depth, seeds[:count])
        else:
            print("No enough seeds. Max seeds number: %d", len(seeds))
            exit()
    elif sys.argv[1] == "khop-throughput":
        if len(sys.argv) < 6:
            print("Usage: python %s khop [seed_file] [depth] [root_count] [thread_number]" % sys.argv[0])
            exit()
        seed_file = sys.argv[2]
        depth = int(sys.argv[3])
        root_count = int(sys.argv[4])
        thread_number = int(sys.argv[5])
        ct = CypherTest()
        seeds = get_seeds(seed_file)
        with Pool(thread_number) as p:
            res = p.starmap(ct.run_khop_throughput, [(depth, seed) for seed in seeds[:root_count]])
            corr_count_sum = 0
            elapsed_time_sum = 0
            neighbor_count_sum = 0
            for success, seed, elapsed_time, neighbor_count in res:
                corr_count_sum += success
                elapsed_time_sum += elapsed_time
                neighbor_count_sum += neighbor_count
            print("response: %d/%d" % (corr_count_sum, root_count))
            print("sum time: %f" % (elapsed_time))
            if corr_count_sum > 0:
                print("average time: %f" % (elapsed_time / corr_count_sum))
                print("average neighbor_num: %f" % (neighbor_count / corr_count_sum))
        exit()
    else:
        usage()
        exit()
