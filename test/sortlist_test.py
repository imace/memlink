# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time, struct, random
import subprocess
from memlinkclient import *
import testbase

READ_PORT, WRITE_PORT = testbase.memlink_addr()

def test_int(m):
    name = 'test1'
    key = 'haha1'
    ret = m.create_sortlist(name, key, 4, MEMLINK_VALUE_INT)
    if ret != MEMLINK_OK:
        print 'create queue error, ret:%d, key:%s' % (ret, key)
        return -1
    
    num = 1000

    vs = range(0, num) 
    random.shuffle(vs) 

    for v in vs:
        vv = struct.pack('I', v)
        ret = m.sortlist_insert(name, key, vv)
        if ret != MEMLINK_OK:
            print 'sortlist insert error, ret:%d, value:%s' % (ret, vv)
            return -1
    
    ret, result = m.range(name, key, MEMLINK_VALUE_ALL, 0, num) 
    if ret != MEMLINK_OK:
        print 'sortlist range error, ret:%d' % (ret)
        return -1

    item = result.items
    while item:
        #print repr(item.value), len(item.value)
        #print struct.unpack('I', item.value)[0]
        item = item.next

    return 0


def test_string(m):
    name = 'test'
    key = 'haha2'
    ret = m.create_table_sortlist(name, 10, MEMLINK_VALUE_STRING)
    if ret != MEMLINK_OK:
        print 'create queue error, ret:%d, name:%s' % (ret, name)
        return -1
    
    num = 1000
    vs = range(0, num) 
    random.shuffle(vs) 

    count = 0
    for v in vs:
        vv = '%010d' % v 
        ret = m.sortlist_insert(name, key, vv)
        if ret != MEMLINK_OK:
            print 'sortlist insert error, ret:%d, value:%s' % (ret, vv)
            return -1
        count += 1
        if count % 1000 == 0:
            print count
    
    for i in range(0, num/1000):
        ret, result = m.range(name, key, MEMLINK_VALUE_ALL, i*1000, 1000) 
        if ret != MEMLINK_OK:
            print 'range error, ret:%d' % (ret)
            return -1

        item = result.items
        start = i * 1000
        while item:
            if item.value != '%010d' % start:
                print 'range value error:', item.value, start
                return -1
            item = item.next
            start += 1
        
    test_string_range(m, name, key, num)
    test_string_count(m, name, key, num)
    test_string_del(m, name, key, num)


def test_string_range(m, name, key, count):
    # test range
    for i in xrange(0, 1000):
        v1, v2 = random.randint(0, 1000), random.randint(0, 1000)
        if v1 == v2:
            continue
        vmin = min(v1, v2)
        vmax = max(v1, v2)
        vminstr = '%010d' % vmin
        vmaxstr = '%010d' % vmax
        ret, result = m.sortlist_range(name, key, MEMLINK_VALUE_ALL, vminstr, vmaxstr)
        if ret != MEMLINK_OK:
            print 'sortlist range:', vmin, vmax
            print 'sortlist range error:', ret, vmin, vmax
            return -1
        if result.count != vmax - vmin:
            print 'sortlist range:', vmin, vmax
            print 'sortlist range count error:', result.count, vmax-vmin
            return -1
        item = result.items
        i = vmin
        while item:
            if item.value != '%010d' % i:
                print 'sortlist range value error:', item.value, i
                return -1
            item = item.next
            i += 1

def test_string_count(m, name, key, count):
    # test count
    for i in xrange(0, 1000):
        v1, v2 = random.randint(0, 1000), random.randint(0, 1000)
        if v1 == v2:
            continue
        vmin = min(v1, v2)
        vmax = max(v1, v2)
        vminstr = '%010d' % vmin
        vmaxstr = '%010d' % vmax
        #print 'count:', vminstr, vmaxstr
        ret, result = m.sortlist_count(name, key, vminstr, vmaxstr)
        if ret != MEMLINK_OK:
            print 'sortlist range:', vmin, vmax
            print 'sortlist range error:', ret, vmin, vmax
            return -1
        if result.visible_count + result.tagdel_count != vmax - vmin:
            print 'sortlist range:', vmin, vmax
            print 'sortlist range count error:', result.visible_count, result.tagdel_count, vmax-vmin
            return -1



def test_string_del(m, name, key, num):
    # test del
    #num = 1000
    resultck = range(0, num)
    # create some delete point 
    point = []
    for i in range(0, 60):
        while True:
            v = random.randint(1, num-1)
            if v not in point:
                point.append(v)
                break
    point.sort()
    #print 'points:', point 
    points = [(0, point[0])]
    for i in range(1, len(point)):  
        points.append((point[i-1], point[i]))
    points.append((point[-1], num))
    #print 'points:', points


    count = num
    for x in points:
        vmin = '%010d' % x[0]
        vmax = '%010d' % x[1]
        #print 'del:', vmin, vmax
        ret = m.sortlist_del(name, key, MEMLINK_VALUE_ALL, vmin, vmax)
        #print 'del ret:', ret
        if ret != MEMLINK_OK:
            print 'sortlist del error:', ret #, vmin, vmax
            return -1

        ret, result = m.range(name, key, MEMLINK_VALUE_ALL, 0, 1000)
        if ret != MEMLINK_OK:
            print 'range error:', ret
            return -1
        
        if result.count != num - x[1]:
            print 'count error:', result.count, num-x[1]
            return

        item = result.items
        i = x[1]
        while item:
            #print 'check:', i, int(item.value)
            if i != int(item.value):
                print 'value error:', i, item.value
                return -1
            item = item.next
            i += 1
    return 0


def test():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    
    '''
    ret = test_int(m)
    if ret < 0:
        return ret
    '''
    ret = test_string(m)
    if ret < 0:
        return ret
    
    m.destroy()
    
    return 0

def test_del():
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30);
    test_string_del(m, 'haha2', 1000)
    m.destroy()

if __name__ == '__main__':
    sys.exit(test())
    #sys.exit(test_del())
    #test_string_del(None)



