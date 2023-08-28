# 项目说明

本项目提供一个benchmark对TuGraph以及进行测试

# 目录说明

- database：包括了图数据库的测试说明文档和测试程序、配置文件等，每个图数据库如何测试请参考每个图数据库下面的readme.md文档
- seeds：测试用的数据随机种子
  - twitter_rv.net-seed：twitter2010数据集的种子

# 测试数据

- twitter数据：
  - 点数据：https://tugraph-web.oss-cn-beijing.aliyuncs.com/tugraph/datasets/twitter/twitter_rv.net_unique_node
  - 边数据：twitter数据分卷存储，将下面2个分卷分别下载之后，使用`cat twitter_rv.tar.gz.a* > witter_rv.tar.gz`再行解压即可
    - https://tugraph-web.oss-cn-beijing.aliyuncs.com/tugraph/datasets/twitter/twitter_rv.tar.gz.aa
    - https://tugraph-web.oss-cn-beijing.aliyuncs.com/tugraph/datasets/twitter/twitter_rv.tar.gz.ab