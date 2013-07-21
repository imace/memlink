# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
from memlinkclient import *

home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

READ_PORT  = 21011
WRITE_PORT = 21012

 
def testq(m, name, key, t):
    pushfunc = getattr(m, t + 'push')
    popfunc  = getattr(m, t + 'pop')

    num = 500
    for i in xrange(0, num):
        v = '%010d' % i
        
        ret = pushfunc(name, key, v)
        if ret != MEMLINK_OK:
            print 'lpush error, ret:%d, v:%s' % (ret, v)
            return -1

    ret, stat = m.stat(name, key)
    if stat:
        if stat.data_used != num:
            print 'stat data_used error!', stat
            return -1
    else:
        print 'test_result stat error:', stat, ret
        return -1
    
    ret, result = m.range(name, key, MEMLINK_VALUE_ALL, 0, num)
    if ret != MEMLINK_OK:
        print 'range error, ret:%d, key:%s' % (ret, key)
        return -1
    item = result.items 
    if t == 'l':
        i = num - 1
        while item:
            v = '%010d' % i
            if item.value != v:
                print 'value compare error, item value:%s, test value:%s' % (item.value, v)
            i -= 1
            item = item.next
    else:
        i = 0
        while item:
            v = '%010d' % i
            if item.value != v:
                print 'value compare error, item value:%s, test value:%s' % (item.value, v)
            i += 1
            item = item.next

    for i in xrange(num-1, -1, -1):
        ret, result = popfunc(name, key)        
        if ret != MEMLINK_OK:
            print 'lpop error, ret:%d' % ret
            return -1
        v = '%010d' % i 
        if result.count != 1:
            print 'lpop num error:', result.count
            return -1
        if result.items.value != v:
            print 'pop value error, pop value:%s, test value:%s' % (result.items.value, v)
            return -1

    ret, result = popfunc(name, key)        
    if ret != MEMLINK_OK:
        print 'lpop error, ret:%d' % ret
        return -1

    if result.count != 0:
        print 'result count error:', result.count
        return -1

    return 0

def test():
    global home

    name = "test"
    key = 'haha'
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
   
    ret = m.create_table_queue(name, 10)
    if ret != MEMLINK_OK:
        print 'create queue error, ret:%d, name:%s' % (ret, name)
        return -1
    
    ret = testq(m, name, key, 'l')
    if ret < 0:
        return ret
    ret = testq(m, name, key, 'r')
    if ret < 0:
        return ret

    m.destroy()
    
    return 0

if __name__ == '__main__':
    sys.exit(test())

