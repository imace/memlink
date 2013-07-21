# coding: utf-8
import os, sys
sys.path.append('../client/python')
import time
from memlinkclient import *

key = 'testmyhaha'
valuesize = 20

def clearkey():
    m = MemLinkClient('127.0.0.1', 11001, 11002, 30)
    ret = m.rmkey(key);
    if ret != MEMLINK_OK:
        print 'rmkey error: %d', ret
        return -1
    m.destroy()

    return 0

def test_insert(count):
    #print '====== test_insert ======'
    global key, valuesize
    vals = []
    formatstr = "%0" + str(valuesize) + "d"
    for i in xrange(0, count):
        #val = '%012d' % i
        val = formatstr % i
        vals.append(val)

    m = MemLinkClient('127.0.0.1', 11001, 11002, 30)
 
    ret = m.create(key, valuesize, "4:3:1")
    if ret != MEMLINK_OK:
        print 'create error!', ret
        return -1

    maskstr = "8:3:1"
    starttm = time.time()
    for val in vals:
        #print 'insert:', val
        ret = m.insert(key, val, maskstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', val, ret
            return -2

    endtm = time.time()
    speed = count / (endtm - starttm)
    print 'use time:', endtm - starttm, 'speed:', speed

    m.destroy()

    return speed

def test_insert_short(count):
    #print '====== test_insert ======'
    global key, valuesize
    vals = []
    formatstr = "%0" + str(valuesize) + "d"
    for i in xrange(0, count):
        #val = '%012d' % i
        val = formatstr % i
        vals.append(val)

    maskstr = "8:3:1"
    iscreate = 0

    starttm = time.time()
    for val in vals:
        m = MemLinkClient('127.0.0.1', 11001, 11002, 30)

        if iscreate == 0:
            ret = m.create(key, valuesize, "4:3:1")
            if ret != MEMLINK_OK:
                print 'create error!', ret
                return -1
            iscreate = 1

        ret = m.insert(key, val, maskstr, 0)
        if ret != MEMLINK_OK:
            print 'insert error!', val, ret
            return -2
        m.destroy()

    endtm = time.time()
    speed = count / (endtm - starttm)
    print 'use time:', endtm - starttm, 'speed:', speed

    return speed



def test_range(frompos, dlen, testcount):
    #print '====== test_range ======'
    global key

    ss = range(0, testcount)
    m = MemLinkClient('127.0.0.1', 11001, 11002, 30)

    starttm = time.time()
    for i in ss:
        ret, result = m.range(key, "", frompos, dlen)
        if not result or result.count != dlen:
            print 'result error!', ret
            return -1
         
    endtm = time.time()
    speed = testcount / (endtm - starttm)
    print 'use time:', endtm - starttm, 'speed:', speed

    m.destroy()

    return speed

def test_range_short(frompos, dlen, testcount):
    #print '====== test_range ======'
    global key

    ss = range(0, testcount)
    starttm = time.time()


    for i in ss:
        m = MemLinkClient('127.0.0.1', 11001, 11002, 30)

        ret, result = m.range(key, "", frompos, dlen)
        if not result or result.count != dlen:
            print 'result error!', ret
            return -1
        m.destroy()
         
    endtm = time.time()
    speed = testcount / (endtm - starttm)
    print 'use time:', endtm - starttm, 'speed:', speed

    return speed

def alltest():
    testnum = [10000, 100000, 1000000, 10000000]
    insertret = []
    insertfunc = [test_insert, test_insert_short]
    insert_test_count = 4
     
    for func in insertfunc:
        for t in testnum[:2]:
            insertret = []
            for i in range(0, insert_test_count):
                if func == test_insert:
                    print "====== insert long %d test: %d ======" % (t, i)
                else:
                    print "====== insert short %d test: %d ======" % (t, i)
                speed = func(t)  
                clearkey()
                insertret.append(speed)
            insertret.sort()

            ret = insertret[1:-1]
            print '\33[31m====== num:%d speed:%d' % (t, sum(ret) / len(ret)), '======\33[0m'

    rangeret = []
    rangefunc = [test_range, test_range_short]
    rangetest = [100, 200, 1000]
    range_test_count = 8

    for func in rangefunc:
        for t in testnum:
            test_insert(t)
            for j in [0, 1]:
                for k in rangetest: 
                    rangeret = []
                    for i in range(0, range_test_count):
                        if j == 0:
                            startpos = 0
                            slen = k
                        else:
                            startpos = t - k
                            slen = k

                        if func == test_range:
                            print "====== range long %d frompos:%d, len:%d, test: %d ======" % (t, startpos, slen, i)
                        else:
                            print "====== range short %d frompos:%d, len:%d, test: %d ======" % (t, startpos, slen, i)
                        speed = func(startpos, slen, 1000)  
                        rangeret.append(speed)

                    rangeret.sort()
                    ret = rangeret[1:-1]
                    print '\33[31m====== num:%d from:%d len:%d speed:%d' % (t, startpos, slen, sum(ret) / len(ret)), '======\33[0m'

            clearkey()

def alltest_thread():
    pass

def dotest():
    if len(sys.argv) == 1:
        alltest()
        return 0

    if len(sys.argv) != 4:
        print 'usage: test.py count range_start range_len'
        sys.exit()
    count = int(sys.argv[1])
    range_start = int(sys.argv[2])
    range_len = int(sys.argv[3])

    test_insert(count)
    test_range(range_start, range_len, 1000)

if __name__ == '__main__':
    dotest()


