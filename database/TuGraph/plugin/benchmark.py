import os
import json
import requests
import logging
from timeit import default_timer as timer
from sys import argv, exit

from tornado.httpclient import *
from tornado.ioloop import *
from tornado.httputil import url_concat

# Global values
url_base = 'http://127.0.0.1:7070'
username = 'admin'
password = '73@TuGraph'
output_dir = 'results'
node_label = 'user'
edge_label = 'weight'
node_field = 'name'

class BaseBenchmark:
    def __init__(self):
        pass

    def get(self, url):
        res = requests.get(url=url, headers=self.headers)
        return res

    def patch(self, url, data):
        start = timer()
        res = requests.patch(url=url, headers=self.headers, data=data)
        end = timer()
        return res, end - start

    def post(self, url, data):
        start = timer()
        res = requests.post(url=url, headers=self.headers, data=data)
        end = timer()
        return res, end - start

    def get_random_nodes_seed_file(self, seed_file_path, count):
        if not os.path.exists(seed_file_path):
            print("Seed file does not exists: " + seed_file_path)
            exit()
        # open seed file
        if os.path.isfile(seed_file_path):
            pre_nodes = open(seed_file_path, 'r').read().split()
            if len(pre_nodes) >= count:
                print("Use random seeds from " + seed_file_path)
                return pre_nodes[0:count]
            else:
                print("Seed file does not contain enough seeds.")
                exit()


class Benchmark(BaseBenchmark):
    def __init__(self, url_base, user, password, output_dir='./result'):
        BaseBenchmark.__init__(self)
        self.session = requests.Session()
        self.url_base = url_base
        self.user = user
        self.password = password
        self.headers = self.get_headers()
        self.output_dir = output_dir

    def init_log(self, path='', print_screen=False):
        self.logger = logging.getLogger('Benchmark')
        if path:
            file_handler = logging.FileHandler(path, mode='w')
            self.logger.addHandler(file_handler)
        if print_screen:
            stream_handler = logging.StreamHandler()
            self.logger.addHandler(stream_handler)
        self.logger.setLevel(logging.INFO)

    def get_headers(self):
        headers = {'Content-Type': 'application/json'}
        r = requests.post(url=self.url_base + '/login',
                          data=json.dumps({'user': self.user, 'password': self.password}),
                          headers=headers)
        if (r.status_code != 200):
            logging.error('error logging in: code={}, text={}'.format(r.status_code, r.text))
            sys.exit(1)
        else:
            headers['Authorization'] = 'Bearer ' + json.loads(r.text)['jwt']
            return headers

    def handle_tugraph_request(self, response):
        if len(logging.root.handlers) > 0:
            logging.root.removeHandler(logging.root.handlers[0])
        self.recv += 1
        if response.error:
            self.bad += 1
            self.logger.error(str(self.recv) + " bad request:error")
            self.logger.error(response)
        else:
            try:
                res = json.loads(response.body)
                size = res["size"]
                root = json.loads(json.loads(response.request.body)['data'])['root']
                self.total_size += size
                self.completed += 1
                self.logger.info('%s %s %s' % (str(self.recv), str(root), str(size)))
            except:
                self.bad += 1
                self.logger.error(str(self.recv) + " bad request:except")
        if self.recv >= self.total_requests:
            IOLoop.instance().stop()

    def run_khop_latency(self, plugin_name, roots, depth):
        total_time = 0.0
        total_size = 0
        query_size = len(roots)
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        for i in range(query_size):
            data = json.dumps({'root': roots[i], 'depth': int(depth), 'label': node_label, 'field': node_field})
            data = json.dumps({'data': str(data)})
            res, time = self.post(url, data)
            neighbor_size = json.loads(res.json()['result'])['size']
            total_size += neighbor_size
            total_time += time
            self.logger.info('process result: %s' % res.text)
            self.logger.info('%s %s %s %s' % (i + 1, roots[i], neighbor_size, time))
        self.logger.info('------RESULT------')
        self.logger.info("correct request: %s/%s" % (query_size, query_size))
        self.logger.info("total size: %s" % str(total_size))
        self.logger.info("average size: %s" % str(total_size / query_size))
        self.logger.info("total query time: %s" % str(total_time))
        self.logger.info("average query time: %s" % str(total_time / query_size))
        self.logger.info("QPS(query per second): %s" % str(query_size / total_time))

    def run_khop_throughput(self, plugin_name, roots, depth, num_threads):
        AsyncHTTPClient.configure("tornado.curl_httpclient.CurlAsyncHTTPClient", max_clients=int(num_threads))
        http_client = AsyncHTTPClient()
        url = '%s/db/cpp_plugin/%s' % (self.url_base, plugin_name)
        for root in roots:
            data = json.dumps({
                'root': root,
                'depth': int(depth),
                'label': 'user',
                'field': 'name'})
            data = json.dumps({'data': str(data)})
            http_client.fetch(url, headers=self.headers, method="POST", body=data,
                              callback=self.handle_tugraph_request, connect_timeout=3600, request_timeout=3600)
        self.total_requests = len(roots)
        self.bad = self.completed = self.recv = 0
        self.total_size = 0
        start_time = time.time()
        IOLoop.instance().start()
        elapsed_seconds = time.time() - start_time
        self.logger.info('------RESULT------')
        self.logger.info("correct request: %s/%s" % (str(self.completed), str(self.total_requests)))
        self.logger.info("total size: %s" % str(self.total_size))
        self.logger.info("average size: %s" % str(self.total_size / self.total_requests))
        self.logger.info("total query time: %s" % str(elapsed_seconds))
        self.logger.info("average query time: %s" % str(elapsed_seconds / self.total_requests))
        self.logger.info("QPS(query per second): %s" % str(self.total_requests / elapsed_seconds))

    def run_khop(self, plugin_name, seed_file, count, depth, mode, num_threads=32):
        self.init_log('%s/%s-%s-%s-k%s' % (self.output_dir, plugin_name, mode, count, depth))
        roots = self.get_random_nodes_seed_file(seed_file, int(count))
        if mode == 'latency':
            self.run_khop_latency(plugin_name, roots, int(depth))
        else:
            self.run_khop_throughput(plugin_name, roots, int(depth), num_threads)

    def run_pagerank(self, plugin_name, num_iterations):
        self.init_log('%s/%s-%s' % (self.output_dir, plugin_name, num_iterations))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        data = json.dumps({'num_iterations': int(num_iterations)})
        data = json.dumps({'data': str(data)})
        res, time = self.post(url, data)
        self.logger.info('process result: %s' % res.text)
        self.logger.info('total_time: %s' % time)

    def run_wcc(self, plugin_name):
        self.init_log('%s/%s' % (self.output_dir, plugin_name))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        data = json.dumps({})
        data = json.dumps({'data': str(data)})
        res, time = self.post(url, data)
        self.logger.info('process result: %s' % res.text)
        self.logger.info('total_time: %s' % time)

    def run_lpa(self, plugin_name, num_iterations):
        self.init_log('%s/%s-%s' % (self.output_dir, plugin_name, num_iterations))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        data = json.dumps({'num_iterations': int(num_iterations)})
        data = json.dumps({'data': str(data)})
        res, time = self.post(url, data)
        self.logger.info('process result: %s' % res.text)
        self.logger.info('total_time: %s' % time)

    def run_sssp(self, plugin_name, seed_file, count):
        self.init_log('%s/%s-%s' % (self.output_dir, plugin_name, count))
        roots = self.get_random_nodes_seed_file(seed_file, int(count))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        total_time = 0
        query_size = len(roots)
        for i in range(query_size):
            data = json.dumps({'root_id': roots[i], 'label': 'user', 'field': 'name', 'output_to_file': False})
            data = json.dumps({'data': str(data)})
            res, time = self.post(url, data)
            print('seed %d: %s' % (i, res.json()))
            self.logger.info('process result: %s' % res.json())
            total_time += time
        self.logger.info('------RESULT------')
        self.logger.info("total query time: %s" % str(total_time))
        self.logger.info("average query time: %s" % str(total_time / query_size))
        self.logger.info("QPS(query per second): %s" % str(query_size / total_time))

    def run_bfs(self, plugin_name, seed_file, count):
        self.init_log('%s/%s-%s' % (self.output_dir, plugin_name, count))
        roots = self.get_random_nodes_seed_file(seed_file, int(count))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        total_time = 0
        query_size = len(roots)
        for i in range(query_size):
            data = json.dumps({'root_id': roots[i], 'label': 'user', 'field': 'name', 'output_to_file': False})
            data = json.dumps({'data': str(data)})
            res, time = self.post(url, data)
            print('seed %d: %s' % (i, res.json()))
            self.logger.info('process result: %s' % res.json())
            total_time += time
        self.logger.info('------RESULT------')
        self.logger.info("total query time: %s" % str(total_time))
        self.logger.info("average query time: %s" % str(total_time / query_size))
        self.logger.info("QPS(query per second): %s" % str(query_size / total_time))

    def run_lcc(self, plugin_name):
        self.init_log('%s/%s' % (self.output_dir, plugin_name))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        data = json.dumps({})
        data = json.dumps({'data': str(data)})
        res, time = self.post(url, data)
        self.logger.info('process result: %s' % res.text)
        self.logger.info('total_time: %s' % time)

    def run_scan_graph(self, plugin_name):
        self.init_log('%s/%s' % (self.output_dir, plugin_name))
        url = '%s/db/default/cpp_plugin/%s' % (self.url_base, plugin_name)
        data = json.dumps({'scan_edges': True, 'times': 10})
        data = json.dumps({'data': str(data)})
        res, time = self.post(url, data)
        self.logger.info('process result: %s' % res.text)
        self.logger.info('total_time: %s' % time)


if __name__ == '__main__':
    lgb = Benchmark(url_base=url_base,
                              user=username,
                              password=password,
                              output_dir=output_dir)
    if len(argv) >= 6 and argv[1] == ('khop'):
        lgb.run_khop(*argv[1:])
    elif len(argv) == 4 and argv[1] == 'sssp':
        lgb.run_sssp(*argv[1:])
    elif len(argv) == 4 and argv[1] == 'bfs':
        lgb.run_bfs(*argv[1:])
    elif len(argv) == 3 and argv[1] == 'pagerank':
        lgb.run_pagerank(*argv[1:])
    elif len(argv) == 3 and argv[1] == 'lpa':
        lgb.run_lpa(*argv[1:])
    elif len(argv) == 2 and argv[1] == 'wcc':
        lgb.run_wcc(*argv[1:])
    elif len(argv) == 2 and argv[1] == 'lcc':
        lgb.run_lcc(*argv[1:])
    elif len(argv) == 2 and argv[1] == 'scan_graph':
        lgb.run_scan_graph(*argv[1:])
    else:
        print('usage:')
        print('python3 benchmark.py khop [seed_file] [count] [depth] [mode:latency/throughput] [num_threads=32]')
        print('python3 benchmark.py pagerank [num_iterations]')
        print('python3 benchmark.py lpa [num_iterations]')
        print('python3 benchmark.py wcc')
        print('python3 benchmark.py lcc')
        print('python3 benchmark.py sssp [seed_file] [count]')
        print('python3 benchmark.py bfs [seed_file] [count]')
