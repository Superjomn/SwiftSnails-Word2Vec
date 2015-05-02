# SwiftSnails-Word2Vec
SwiftSnails-Word2Vec 是一个高性能的分布式Word2Vec分布式实现。

如果你需要从大量的文本数据（大于30G)，词向量，可以使用本项目， 修改一些配置就可以直接运行。

也可以访问 http://yun.baidu.com/s/1gdgYkiZ 获取（这是我们之前生成的一套词向量，64G数据+43w词库）。

## 编译方法
本项目直接依赖SwiftSnails参数服务器，请提前下载SwiftSnails，并安装所有的第三方依赖。

之后在Makefile中作如下配置:

```
# 下载的SwiftSnails的src目录
SwiftSnails_src=...

# SwiftSnails所有第三方依赖库安装目录
LOCAL_ROOT...
```

之后编译：
```
$ mkdir bin
$ make
```

所有产出的可执行文件在 `bin/` 目录中，具体内容如下

```
swift_worker    # 计算节点
swift_server    # 参数服务器节点
swift_lworker   # 内存优化的word2vec计算节点
swift_master    # 中央控制节点
```

一个完整的任务执行需要包括 *worker* ， *server* ， *master* 三种节点的协作。

其中 worker 节点有两种实现： 

* swift_worker : 常规的CBOW+Negative-Sampling 实现
* swift_worker : 内存优化的版本，通过minibatch的方式控制内存消耗，一般可以降一个数量级

## 节点配置
SwiftSnails中需要三类节点的配置，此处基准配置在 `config/` 目录中

### common.conf
```
# 是否对传输消息进行压缩
# 0 表示不压缩，数值越大，压缩比例越大
zlib : 0~9 数值
```

### master.conf
```
# master 节点侦听地址
listen_addr: tcp://127.0.0.1:16831

# master 守候进程数
async_exec_num: 4

# worker节点数 + server节点数（必须要配置）
expected_node_num: 4

# 初始化等待时长，超时后，master将不再接受节点登记，单位为秒
master_time_out: 120

# 参数分块数，便于参数拆分，可以设置为 server数目 * 3
frag_num: 50
```

### worker.conf
```
# worker守候地址，可以不配置，节点会自动获取本机ip及随机端口
listen_addr:

# 守候线程数目
async_exec_num: 2

# 初始化超时 最好和 master_time_out 设置相同时间
init_timeout: 60

# master的监听地址，需要和 master.conf中的listen_addr 相同
master_addr: tcp://127.0.0.1:16831

# 计算线程数目，最好设置为CPU核数
async_channel_thread_num

# 迭代次数 如果数据比较大，只需要1轮迭代
num_iters: 1

# 文件读取缓存，单位为行
line_buffer_queue_capacity : 100
```

### server.conf
类似配置参考 *worker.conf*
```
# 单个server上的参数分块数（由于采用了读写锁，拆分多个shard后可以提升性能)
# 可以设置多一点 30+ 300+ 都可以
shard_num: 5

# 执行过程中的参数备份的周期 
# 备份的周期单位为PUSH的次数
# 一轮迭代的PUSH次数可以通过 单个节点训练数据行数 / minibatch长度
param_backup_period: 0
param_backup_root: ./param_back/

# 是否使用AdaGrad，0表示非, 1表示是
adagrad: 1
```

### word2vec.conf
```
# 词向量维度
len_vec: 100

# 初始的学习率
learning_rate: 0.5

# CBOW 窗口长度
window: 4

# 负例个数
negative: 20

# 高频采样
sample: 0.001

# minibatch 长度
num_sents_to_cache: 10000
```


## 集群配置
本项目不包含任何执行任务之外的功能实现，无法：

* 分发数据
* 分发任务
* 任务控制

但提供了一些简单的脚本，我们在实际的词向量生成任务中也是使用的这套脚本。

**集群配置**

需要在集群中一个节点上执行 *swift_master* ，此节点称为控制节点。

为了方便脚本使用 **ssh** 的方式批量执行命令，需要建立控制节点与其他节点的ssh_key 免密码登陆。

之后可以使用 *tools * 中的脚本工具。

在 *common.sh* 中配置集群中的节点IP列表，以及一系列的环境变量

之后的集群执行包括：

* copy_exec.sh 复制二进制文件到当前目录
* dist_demo.sh 在common.sh中配置的节点中分发任务
* dist_kill_demo.sh kill集群任务
* dist_log.sh 搜集最新的执行
* dist_collect_parameter.sh 任务执行完毕，汇总集群中各server产生的参数
