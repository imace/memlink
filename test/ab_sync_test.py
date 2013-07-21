#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
#import memlinkclient
from memlinkclient import *
from synctest import *

def test():
    client2master = MemLinkClient('127.0.0.1', MASTER_READ_PORT, MASTER_WRITE_PORT, 30);
    client2slave  = MemLinkClient('127.0.0.1', SLAVE_READ_PORT, SLAVE_WRITE_PORT, 30);
    
    test_init()
    
    change_synclog_indexnum(1000);

    cmd = 'killall memlink_master'
    print cmd
    os.system(cmd)

    cmd = 'killall memlink_slave'
    print cmd
    os.system(cmd)

    cmd = 'bash clean.sh'
    os.system(cmd)

    print 
    print '=============================== test a ============================'
    #start a new master
    x1 = start_a_new_master()
    time.sleep(5)

    name = 'test'
    key = 'haha'
    attrstr = "8:1:1"
    ret = client2master.create_table_list(name , 12, "4:3:1")
    if ret != MEMLINK_OK:
        print 'create error:', ret, name
        return -1
    print 'create 1 key'

    #insert 3000
    num = 3000
    for i in xrange(0, num):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', name, key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num

    #start a new slave
    x2 = start_a_new_slave()
    time.sleep(10)
    
    print 'dump.....'
    client2master.dump();
    
    #delete val 1000
    num2 = 1000
    for i in range(0, num2):
        val = '%012d' % i
        ret = client2master.delete(name, key, val)
        if ret != MEMLINK_OK:
        	print 'delete val error:', ret, val
        	return -1
    print 'delete val %d' % num2

    time.sleep(10)

    if 0 != stat_check(client2master, client2slave):
        print 'testa error!'
        return -1

    #x1.kill()
    #x2.kill()
    client2master.close()
    client2slave.close()
    print 'test a ok'
    sync_test_clean()

    print 
    print '============================= test b =============================='
    #start a new master
    cmd = 'killall memlink_master'
    os.system(cmd)
    cmd = 'killall memlink_slave'
    os.system(cmd)

    x1 = start_a_new_master()
    time.sleep(5)
    
    ret = client2master.create_table_list(name, 12, "4:3:1")
    if ret != MEMLINK_OK:
        print 'create error:', ret, name
        return -1
    print 'create 1 key'

    #insert 1000
    num = 1000
    for i in xrange(0, num):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', name, key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num

    print 'dump.....'
    client2master.dump();
    
    #insert 2000
    num2 = 3000
    for i in xrange(num, num2):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', name, key, val, attrstr, ret
            return -2;
    print 'insert %d val' % (num2 - num)
    
    #start a new slave
    x2 = start_a_new_slave()
    time.sleep(10)
    
    #delete val 1000
    num2 = 1000
    for i in range(0, num2):
        val = '%012d' % i
        ret = client2master.delete(name, key, val)
        if ret != MEMLINK_OK:
        	print 'delete val error:', ret, val
        	return -1
    print 'delete val %d' % num2

    time.sleep(10)
    
    if 0 != stat_check(client2master, client2slave):
        print 'testb error!'
        return -1

    print 'test b ok'
        
    #x1.kill()
    #x2.kill()
    
    client2master.destroy()
    client2slave.destroy()
    sync_test_clean()

    return 0

if __name__ == '__main__':
    sys.exit(test())

