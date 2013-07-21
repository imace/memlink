#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
from memlinkclient import *
from synctest import *

def test():
    client2master = MemLinkClient('127.0.0.1', MASTER_READ_PORT, MASTER_WRITE_PORT, 30);
    client2slave  = MemLinkClient('127.0.0.1', SLAVE_READ_PORT, SLAVE_WRITE_PORT, 30);
    
    test_init()

    print 
    print '============================= test e  =============================='
    #start a new master
    x1 = start_a_new_master()
    
    #start a new slave
    x2 = start_a_new_slave()   
    time.sleep(10)
    
    name = 'test'
    key = 'haha'
    attrstr = "8:1:1"
    ret = client2master.create_table_list(name , 12, "4:3:1")
    if ret != MEMLINK_OK:
        print 'create error:', ret, name
        return -1
    print 'create 1 key'

    #1 insert 1000
    num = 1800
    for i in xrange(0, num):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, attrstr, i)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num

    #2 kill slave
    time.sleep(10)
    print 'kill slave'
    x2.kill()

    #insert 2500, dump , del 1500
    num2 = 3500
    for i in xrange(num, num2):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, attrstr, i)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % (num2 - num)
    
    print 'dump.....'
    client2master.dump();
    
    #delete val 1500
    num3 = 1500
    for i in range(0, num3):
        val = '%012d' % i
        ret = client2master.delete(name, key, val)
        if ret != MEMLINK_OK:
        	print 'delete val error:', ret, val
        	return -1
    print 'delete val %d' % num3

    #4 remove binlog 
    cmd = 'rm data/bin.log.1 data/bin.log.2 data/bin.log.3'
    print cmd
    os.system(cmd)
    time.sleep(10)

    #5 restart slave
    x2 = restart_slave()
    time.sleep(10)
    
    if 0 != stat_check(client2master, client2slave):
        print 'test e error!'
        return -1

    print 'test e ok'

    x1.kill()
    x2.kill()
    
    client2master.destroy()
    client2slave.destroy()

    return 0

if __name__ == '__main__':
    sys.exit(test())

