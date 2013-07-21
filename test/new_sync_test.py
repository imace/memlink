#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
#import memlinkclient
from memlinkclient import *

READ_PORT  = 11011
WRITE_PORT = 11012

def test_result():
    key = 'haha000'
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    
    ret, stat = m.stat(key)
    if stat:
        if stat.data_used != 500:
            print 'stat data_used error!', stat
            return -3
        """if stat.data != 500:
            print 'stat data error!', stat
            return -3"""
    else:
        print 'stat error:', stat, ret
        return -3

    ret, result = m.range(key, MEMLINK_VALUE_ALL, 0, 1000)
    if not result or result.count != 250:
        print 'range error!', result, ret
        return -4

    #print 'count:', result.count
    item = result.items;

    i = 250
    while i > 0:
        i -= 1
        v = '%010d' % (i*2 + 1)
        if v != item.value:
            print 'range item error!', item.value, v
            return -5
        item = item.next

    print '---------', i

    if i != 0:
        print 'ranged items are not right!'

    m.destroy()

    return 0

def testb():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);

    num = 50000
    for i in xrange(0, num):
        key = 'haha%05d' % i
        ret = m.create_list(key, 10, "4:3:1")
        if ret != MEMLINK_OK:
            print 'create error:', ret
            return -1
    print 'create %d key' % num

    num = 100000
    #num = 200
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num

def testc():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);

    num = 100000
    for i in xrange(0, num):
        key = 'haha%05d' % i
        ret = m.create_list(key, 10, "4:3:1")
        if ret != MEMLINK_OK:
            print 'create error:', ret
            return -1
    print 'create %d key' % num

    num = 750000
    #num = 200
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num

    i = num-1;
    while i >= 700000:
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.delete(key, val)
        if ret != MEMLINK_OK:
            print 'del error!', key, val
            return -2
        i -= 1
    num = 50000
    print 'del %d val' % num
    
    m.dump()
    print 'dump'

    while i >= 600000:
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.delete(key, val)
        if ret != MEMLINK_OK:
            print 'del error!', key, val
            return -2
        i -= 1
    num = 100000
    print 'del %d val' % num

def testd():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);

    num = 100000
    for i in xrange(0, num):
        key = 'haha%05d' % i
        ret = m.create_list(key, 10, "4:3:1")
        if ret != MEMLINK_OK:
            print 'create error:', ret
            return -1
    print 'create %d key' % num

    num1 = 250000
    for i in xrange(0, num1):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num1

    m.dump()
    print 'dump'

    num2 = 500000
    num = num1 + num2
    for i in xrange(num1, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num2
    
    i = num-1;
    while i >= 600000:
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.delete(key, val)
        if ret != MEMLINK_OK:
            print 'del error!', key, val
            return -2
        i -= 1
    num = 150000
    print 'del %d val' % num

def teste():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    
    key = 'haha999999'
    ret = m.create_list(key, 10, "4:3:1")
    if ret != MEMLINK_OK and ret != MEMLINK_ERR_EKEY:
        print 'create error:', ret
        return -1
        
    num = 400000
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num
    
    i = num - 1
    num1 = 100000
    num = num - num1
    while i >= num:
        val = '%010d' % i
        ret = m.delete(key, val)
        if ret != MEMLINK_OK:
            print 'del error!', key, val
            return -2
        i -= 1
    print 'del %d val' % num1

    num = 300000
    i = num - 1
    while i >= 290000:
        val = '%010d' % i
        ret = m.move(key, val, 0)
        if ret != MEMLINK_OK:
            print 'move error!', key, val
            return -2
        i -= 1
    num = 10000
    print 'move %d val' % num

    #return 0
    
    num = 300000
    for i in xrange(290000, num):
        val = '%010d' % i
        ret = m.attr(key, val, '3:3:1')
        if ret != MEMLINK_OK:
            print 'attr error!', key, val
            return -2
    print 'attr %d val' % num

    i = 290000
    while i < num:
        val = '%010d' % i
        ret = m.tag(key, val, 1)
        if ret != MEMLINK_OK:
            print 'tag error!', key, val
            return -2
        i += 2
    print 'tag %d val' % num

def testg():

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    key = 'haha999999'
    ret = m.create_list(key, 10, "4:3:1")
    if ret != MEMLINK_OK and ret != MEMLINK_ERR_EKEY:
        print 'create error:', ret
        return -1

    '''num = 25000
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num

    c = raw_input()'''

    key = 'haha999999'
    ret = m.create_list(key, 10, "4:3:1")
    if ret != MEMLINK_OK and ret != MEMLINK_ERR_EKEY:
        print 'create error:', ret
        return -1
    
    num = 200000
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num

    m.dump()
    print 'dump'
    
    num = 100000
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num

def testi():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    key = 'haha888888'
    ret = m.create_list(key, 10, "4:3:1")
    if ret != MEMLINK_OK and ret != MEMLINK_ERR_EKEY:
        print 'create error:', ret
        return -1

    num = 10000000
    for i in xrange(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2
    print 'insert %d val' % num
        
def test():
    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(home)

    memlinkfile  = os.path.join(home, 'memlink')
    memlinkstart = memlinkfile + ' test/memlink.conf'

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    
    key = 'haha777777'
    ret = m.create_list(key, 10, "4:3:1")
    if ret != MEMLINK_OK and ret != MEMLINK_ERR_EKEY:
        print 'create error:', ret
        return -1
    
    #insert 800 val
    num = 1000
    for i in range(0, num):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, i)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num
    
    c = raw_input()

    num2 = 2000
    for i in range(num, num + num2):
        val = '%010d' % i
        attrstr = "8:1:1"
        ret = m.insert(key, val, attrstr, i)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, attrstr, ret
            return -2;
    print 'insert %d val' % num2
    return 0

if __name__ == '__main__':
    #sys.exit(test_result())
    action = sys.argv[1]
    func = globals()[action]
    func()
    
