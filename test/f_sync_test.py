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
    change_synclog_indexnum(1000);

    cmd = 'bash clean.sh'
    os.system(cmd)
    print 
    print '============================= test f  =============================='
    x1 = start_a_new_master()
    x2 = start_a_new_slave()
    time.sleep(1)

    name = 'test'
    key = 'haha'
    attrstr = "8:1:1"
    ret = client2master.create_table_list(name , 12, "4:3:1")
    if ret != MEMLINK_OK:
        print 'create error:', ret, name
        return -1
    print 'create 1 key'

    #1 insert 1000
    num = 1000
    for i in xrange(0, num):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num

    #2 kill slave
    time.sleep(1)
    print 'kill slave'
    x2.kill()

    #insert 500
    num2 = 1500
    for i in xrange(num, num2):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % (num2 - num)


    #insert 1500
    num3 = 3000
    for i in xrange(num2, num3):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % (num3 - num2)
    
    #move 1500
    num = 1500
    for i in range(0, num):
        val = '%012d' % (i*2)
        ret = client2master.move(name, key, val, 0)
        if ret != MEMLINK_OK:
        	print 'move error:', val, ret
        	return -1
    print 'move %d val' % num


    #attr 1000
    num = 1000
    attrstr1 = '6:2:1'
    for i in xrange(0, num):
        val = '%012d' % (i*3) 
        ret = client2master.attr(name, key, val, attrstr1)
        if ret != MEMLINK_OK:
            print 'attr error!', val, attrstr, ret
            return -2;
    print 'attr %d val' % num

    x2 = restart_slave()
    time.sleep(1)
    #tag 300
    num = 300
    for i in xrange(0, num):
        val = '%012d' % (i*10) 
        ret = client2master.tag(name, key, val, 1)
        if ret != MEMLINK_OK:
            print 'tag error!', val, ret
            return -2;
    print 'tag %d val' % num

    #del 1000
    num = 1000
    ret, result = client2master.range(name, key, MEMLINK_VALUE_VISIBLE, 0, num)
    #print 'count:', result.count
    item = result.items;
    while item:
        ret = client2master.delete(name, key, item.value)
        if ret != MEMLINK_OK:
            print 'del error! ', item.value, ret
            return -1
        item = item.next
        
    print 'del %d val' % num

    time.sleep(2)
    
    if 0 != stat_check(client2master, client2slave):
        print 'test f error!'
        return -1
    print 'stat ok'

    if 0 != result_check(client2master, client2slave, name, key):
        print 'test f error!'
        return -1
    print 'result ok'
    
    print 'test f ok'

    x1.kill()
    x2.kill()
    
    client2master.destroy()
    client2slave.destroy()
    sync_test_clean()

    return 0

if __name__ == '__main__':
    sys.exit(test())

