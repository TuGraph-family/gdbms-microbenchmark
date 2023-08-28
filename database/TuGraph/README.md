# 说明

该部分说明了在twitter2010数据集上对TuGraph分支进行benchmark测试.

## 测试资源

- `run-docker-daemon.sh`: 启动TuGraph的脚本
## 测试步骤

### 启动Docker

参考`run-docker-daemon.sh`的内容，设置相应的变量并以Daemon方式启动Docker
- PROJECT_ROOT指向gdbms-microbenchmark的目录
- TWITTER_DATA_ROOT指向Twitter数据的目录

### 导入数据

参考`import/import.sh`完成twitter数据的导入，注意检查import.json中的数据路径正确

### 运行测试


- 基于Plugin的Khop以及算法测试：进入`database/TuGraph/plugin`目录，执行benchmark_auto.sh脚本，注意检查`benchmark.py`中的url_base是否为TuGraph
  监听的端口
- 基于Cypher的Khop测试：进入`database/TuGraph/cypher`目录，执行benchmark_auto.sh脚本，注意检查`cypher-benchmark.py`中的host_port是否为TuGraph
  监听的端口