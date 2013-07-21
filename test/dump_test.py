#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
from memlinkclient import *
import testbase

home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
READ_PORT, WRITE_PORT = testbase.memlink_addr()
name = 'test'

def test_result():
    global home
    
    key = 'haha000'
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    
    ret, stat = m.stat(name, key)
    if stat:
        if stat.data_used != 500:
            print 'stat data_used error!', stat
            return -3
    else:
        print 'test_result stat error:', stat, ret
        return -3

    ret, result = m.range(name, key, MEMLINK_VALUE_VISIBLE, 0, 1000)
    if not result or result.count != 250:
        print 'range error!', result, ret
        return -4
    #print 'count:', result.count
    item = result.items

    i = 250
    while i > 0:
        i -= 1
        v = '%010d' % (i*2 + 1)
        if v != item.value:
            print 'range item error!', item.value, v
            return -5
        item = item.next

    if i != 0:
        print 'ranged items are not right!'
    m.destroy()

    return 0

def test():
    global home
    memlinkfile  = os.path.join(home, 'memlink')
    memlinkstart = '%s %s/test/memlink.conf' % (memlinkfile, home)

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    vnum = 0   
    key = 'haha000'
    ret = m.create_table_list(name, 10, "4:3:1")
    if ret != MEMLINK_OK:
        print 'create error:', ret, name
        return -1

    num = 200
    print 'insert 0-200 %010d'
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(name, key, val, 0, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', name, key, val, attrstr, ret
            return -2
    vnum += 200

    m.dump()

    num = 1000
    print 'insert 200-1000 %010d'
    for i in range(200, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(name, key, val, i, attrstr)
        if ret != MEMLINK_OK:
            print 'insert error!', name, key, val, attrstr, ret
            return -2
    vnum += num - 200
    
    print 'create 1-200 %03d'
    for i in range(1, 200):
        keynm = 'haha%03d' % i
        #ret = m.create_node(key, 6, "4:3:1")
        ret = m.create_node(name, keynm)
        if ret != MEMLINK_OK:
            print 'create error:', ret, keynm
            return -1

    print 'rmkey 100-200 %03d'
    for i in range(100, 200):
        key = 'haha%03d' % (i)
        ret = m.rmkey(name, key)
    	if ret != MEMLINK_OK:
        	print 'rmkey error:', ret, name, key
        	return -1
	
	# delete 500
    num2 = num/2
    key = 'haha000'
    print 'delete 0-%d %%010d' % num2
    for i in range(0, num2):
        val = '%010d' % (i * 2)
        ret = m.delete(name, key, val)
        if ret != MEMLINK_OK:
        	print 'delete val error:', ret, val
        	return -1
    vnum -= num2	
    
    # move 500
    for i in range(0, num2):
        val = '%010d' % (i * 2 + 1)
        ret = m.move(name, key, val, 0)
        if ret != MEMLINK_OK:
            print 'move val error:', ret, val
            return -1

    # set all the values' attr = 4:4:1
    for i in range(0, 500):
        val = '%010d' % (i*2 + 1)
        ret = m.attr(name, key, val, "4:4:1")
        if ret != MEMLINK_OK:
            print 'attr val error:', ret, val
            return -1

    #tag del 250 - 499
    for i in range(250, 500):
        val = '%010d' % (i*2 + 1)
        ret = m.tag(name, key, val, 1)
        if ret != MEMLINK_OK:
            print 'tag val error:', ret, val
            return -1

    """ret = m.clean(name, key)
    if ret != MEMLINK_OK:
        print 'clean error:', ret"""
        
    ret, stat = m.stat(name, key)
    if ret != MEMLINK_OK:
        print 'stat error:', ret

    #print 'value count:', vnum, 'stat count:', stat.data_used 
    m.destroy()
    cmd = "killall memlink"
    print cmd
    os.system(cmd)
   
    time.sleep(1)
    print memlinkstart
    #x = subprocess.Popen(memlinkstart, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
    #                         shell=True, env=os.environ, universal_newlines=True) 
    x = subprocess.Popen(memlinkstart, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True) 
    time.sleep(3)
    ret = test_result()
    x.kill()
    return ret

if __name__ == '__main__':
    #sys.exit(test_result())
    sys.exit(test())

