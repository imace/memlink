特性
支持普通List/排序List/双向队列三种List类型。
List中的数据项（Item）支持一个主字段（整型、二进制）+若干个附属属性（bit）,附属属性支持各种过滤操作
强一致性，任何写操作实时刷新bin-log，保证数据不丢失
采用块链和跳表精简内存使用，提升查找效率
支持多表定义(Ver0.5)，不同表存储为不同数据文件便于迁移
支持主从同步
支持客户端
支持的客户端SDK都采用swig生成

C/C++
PHP
Python
Java
性能指标
采用自带perf（memlink/performance/perf）工具在4核 32G内存服务器上进行的测试,该测试是一个本地的单客户端压力测试，测试结果只能算是管中窥豹。

Create 28386.82reqs/s

./perf -h 127.0.0.1 -t 100 -n 10000 -d create -k xyz -s 4

Insert 29033.73reqs/s

./perf -h 127.0.0.1 -t 90 -n 10000 -d insert -k xyz0 -s 4

Range 31384.88reqs/s

./perf -h 127.0.0.1 -t 90 -n 10000 -d range -k xyz0 -f 50 -l 100

Lpop 30930.09reqs/s

./perf -h 127.0.0.1 -t 50 -n 10000 -d lpop -k xyz4 -l 10

Sharding
It supports client-side sharding.

Who is using
天涯 问问

Road map
Sponsor
sponsored by tianya.inc