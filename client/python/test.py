import os, sys
import time
from memlinkclient import *
#print dir(memlink)

READ_PORT  = 11001
WRITE_PORT = 11002

tbname = 'test'
lstkey = 'haha'
key = tbname + '.' + lstkey

def insert(*args):
    '''insert tbname.key '''
    try:
        start = int(args[0])
        num   = int(args[1])
    except:
        start = 0
        num   = 1000

    try:
        val = '%012d' % int(args[2])
    except:
        val = None

    print 'insert:', start, num, val

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret = m.create_table_list(tbname, 12, "32:32:2")
    if ret != MEMLINK_OK:
        print 'create haha error!', ret
        #return

    for i in xrange(start, start + num):
        if not val:
            val2 = '%012d' % i
        else:
            val2 = val
        print 'insert:', val2, i
        mstr = '%d:%d:1' % (i, i)
        ret = m.insert(key, val2, i, mstr)
        if ret != MEMLINK_OK:
            print 'insert error:', ret, i
            return

    m.destroy()

def delete(*args):
    '''delete tbname.key'''
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    val = '%012d' % int(args[0]) 
    print 'delete:', val
    ret = m.delete(key, val)
    if ret != MEMLINK_OK:
        print 'delete error:', ret, val
        return

    m.destroy()
    
    return

    for i in xrange(1, 300):
        print 'del %012d' % (i*2)
        ret = m.delete(key, '%012d' % (i*2))
        if ret != MEMLINK_OK:
            print 'delete error:', ret, i*2
            return
    
    m.destroy()

def delete_by_attr(*args):
    akey = args[0]
    attr = args[1]

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)
    ret = m.delete_by_attr(akey, attr)
    if ret != MEMLINK_OK:
        print 'delete by attr error: ', ret
    else:
        print 'delete by attr successfully!'

    m.destroy()


def range(*args):
    try:
        kind    = args[0]
        
        if kind.startswith('vis'):
            kind = MEMLINK_VALUE_VISIBLE
        elif kind.startswith('tag'):
            kind = MEMLINK_VALUE_TAGDEL
        elif kind.startswith('all'):
            kind = MEMLINK_VALUE_ALL
        else:
            print 'kind error! must visible/tagdel/all'
            return
    except:
        kind    = MEMLINK_VALUE_VISIBLE

    try:
        frompos = int(args[1])
        slen    = int(args[2])
    except:
        frompos = 0
        slen    = 1000

    try:
        attr = args[3]
    except:
        attr = ''
    
    print 'ALL:%d, VISIBLE:%d, TAGDEL:%d' % (MEMLINK_VALUE_ALL, MEMLINK_VALUE_VISIBLE, MEMLINK_VALUE_TAGDEL)
    print 'range kind:%d, from:%d, len:%d, attr:%s' % (kind, frompos, slen, attr)

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    
    ret, recs = m.range(key, kind, frompos, slen, attr)
    if ret != MEMLINK_OK:
        print 'range error:', ret
        return

    print recs.count

    #for x in recs.list():
    #    print x, type(x[0]), type(x[1])

    print '-' * 60
    items = recs.items
    while items:
        print items.value, items.attr
        items = items.next

    recs.close()

    m.destroy()

def dump(*args):
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret = m.dump()
    if ret != MEMLINK_OK:
        print 'dump error!', ret
    m.destroy()


def clean(*args):
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret = m.clean(key)
    if ret != MEMLINK_OK:
        print 'clean error!', ret
    m.destroy()

def stat(*args):
    try:
        akey = args[0]
    except:
        akey = key

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret, stat = m.stat(akey)
    if ret != MEMLINK_OK:
        print 'stat error!', ret
    print stat
    m.destroy()

def stat_sys(*args):
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret, stat = m.stat_sys()
    if ret != MEMLINK_OK:
        print 'stat_sys error!', ret
    print stat
    m.destroy()


def ping(*args):
    try:
        count = int(args[1])
    except:
        count = 1000
    print 'ping count:', count
    start = time.time()
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    for i in xrange(0, count):
        ret = m.ping()
        if ret != MEMLINK_OK:
            print 'ping error:', ret
            return 
    m.destroy()
    end = time.time()

    print 'use time:', end - start, 'speed:', count / (end-start)

def lpush(*args):
    try:
        start = int(args[0])
        num   = int(args[1])
    except:
        start = 0
        num   = 1000

    try:
        val = '%012d' % int(args[2])
    except:
        val = None

    print 'lpush:', start, num, val

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
   
    ret = m.create_table_queue(tbname, 12, "1")
    if ret != MEMLINK_OK:
        print 'create haha error!', ret
        #return

    for i in xrange(start, start + num):
        if not val:
            val2 = '%012d' % i
        else:
            val2 = val
        print 'lpush:', val2
        ret = m.lpush(key, val2, "1")
        if ret != MEMLINK_OK:
            print 'lpush error:', ret, i
            return

    m.destroy()

def rpush(*args):
    try:
        start = int(args[0])
        num   = int(args[1])
    except:
        start = 0
        num   = 1000

    try:
        val = '%012d' % int(args[2])
    except:
        val = None

    print 'lpush:', start, num, val

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
   
    ret = m.create_table_queue(tbname, 12, "1")
    if ret != MEMLINK_OK:
        print 'create haha error!', ret
        #return

    for i in xrange(start, start + num):
        if not val:
            val2 = '%012d' % i
        else:
            val2 = val
        print 'rpush:', val2
        ret = m.rpush(key, val2, "1")
        if ret != MEMLINK_OK:
            print 'rpush error:', ret, i
            return

    m.destroy()

def lpop(*args):
    try:
        count = int(args[0])
    except:
        count = 1
    print 'lpop count:', count
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret, result = m.lpop(key, count)
    if ret != MEMLINK_OK:
        print 'lpop error:', ret
        return 
    print 'lpop result:', result.count
    item = result.root
    while item:
        print item.value, item.attr
        item = item.next

    m.destroy()

def rpop(*args):
    try:
        count = int(args[0])
    except:
        count = 1
    print 'rpop count:', count
    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
    ret, result = m.rpop(key, count)
    if ret != MEMLINK_OK:
        print 'rpop error:', ret
        return 
    print 'rpop result:', result.count
    item = result.root
    while item:
        print item.value, item.attr
        item = item.next

    m.destroy()

def sortlistinsert(*args):
    try:
        start = int(args[0])
        num   = int(args[1])
    except:
        start = 0
        num   = 1000

    try:
        val = '%012d' % int(args[2])
    except:
        val = None

    print 'insert:', start, num, val

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)
   
    ret = m.create_table_sortlist(tbname, 12, "1", MEMLINK_VALUE_STRING)
    if ret != MEMLINK_OK:
        print 'create haha error!', ret
        #return

    for i in xrange(start, start + num):
        if not val:
            val2 = '%012d' % i
        else:
            val2 = val
        print 'insert:', val2
        ret = m.sortlist_insert(key, val2, "1")
        if ret != MEMLINK_OK:
            print 'insert error:', ret, i
            return

    m.destroy()

def sortlist_range(*args):
    akey = args[0]
    valmin = args[1]
    valmax = args[2]
    attr = args[3]

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)

    retset = m.sortlist_range(akey, MEMLINK_VALUE_VISIBLE, valmin, valmax, attr)

    if retset[0] != MEMLINK_OK:
        print 'sortlist range error: ', ret
        m.destroy()
        return;

    print 'count: %d, valuesize: %d, attrsize: %d' % (retset[1].count, retset[1].valuesize, retset[1].attrsize)

    n = retset[1].root

    while n:
        print '\tvalue: %s, attr: %s' % (n.value, n.attr)
        n = n.next

    m.destroy()

def sortlist_del(*args):
    akey = args[0]
    valmin = args[1]
    valmax = args[2]
    attr = args[3]

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)

    ret = m.sortlist_del(akey, MEMLINK_VALUE_VISIBLE, valmin, valmax, attr)

    if ret != MEMLINK_OK:
        print 'sortlist del error: ', ret

    m.destroy()

def sortlist_count(*args):
    akey = args[0]
    valmin = args[1]
    valmax = args[2]
    attr = args[3]

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 10)

    ret, count = m.sortlist_count(akey, valmin, valmax, attr)

    if ret != MEMLINK_OK:
        print 'sortlist count error: ', ret
        m.destroy()
        return
    print 'visible_count: %d, tagdel_count: %d\n' % (count.visible_count, count.tagdel_count)

    m.destroy()


def create_list(*args):
    try:
        name = args[0]
        vallen = int(args[1])
        attr = args[2]
    except:
        name = tbname;
        vallen = 20;
        attr = '20:1:1'

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.create_table_list(name, vallen, attr)

    if ret != MEMLINK_OK:
        print 'create_list error: ', ret
    else:
        print 'create_list successfully!'

    m.destroy()

def create_queue(*args):
    try:
        name = args[0]
        vallen = int(args[1])
        attr = args[2]
    except:
        name = tbname; 
        vallen = 20;
        attr = '20:1:1'

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.create_table_queue(name, vallen, attr)

    if ret != MEMLINK_OK:
        print 'create_queue error: ', ret
    else:
        print 'create_queue successfully!'

    m.destroy()

def create_sortlist(*args):
    try:
        name = args[0]
        vallen = int(args[1])
        attr = args[2]
    except:
        name = tbname;
        vallen = 20;
        attr = '20:1:1'

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.create_table_sortlist(name, vallen, MEMLINK_SORTLIST, attr)
    if ret != MEMLINK_OK:
        print 'create_sortlist error: ', ret
    else:
        print 'create_sortlist successfully!'

    m.destroy()

def move(*args):
    try:
        akey = args[0]
        val = args[1]
        pos = int(args[2])
    except:
        print 'usage: key val pos'
        return

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.move(key, val, pos)

    if ret != MEMLINK_OK:
        print 'move error: ', ret

    m.destroy()

def attr(*args):
    try:
        akey = args[0]
        val = args[1]
        attr = args[2]
    except:
        print 'args usage: key val attr'
        return

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.attr(akey, val, attr)

    if ret != MEMLINK_OK:
        print 'attr error: ', ret

    m.destroy()

def tag(*args):
    try:
        akey = args[0]
        val = args[1]
        tag = int(args[2])
    except:
        print 'args usage: key val tag'
        return

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.tag(key, val, tag)
    if ret != MEMLINK_OK:
        print 'tag error: ', ret

    m.destroy()

def count(*args):
    try:
        akey = args[0]
        attr = args[1]
    except:
        akey = key
        attr = ''

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret, count = m.count(key, attr)
    if ret != MEMLINK_OK:
        print 'count error: ', ret

    if count:
        print 'visible_count: %d, tagdel_count: %d\n' % (count.visible_count, count.tagdel_count)

    m.destroy()

def rmkey(*args):
    try:
        akey = args[0]
    except:
        akey = key

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret = m.rmkey(akey)
    if ret != MEMLINK_OK:
        print 'rmkey error: ', ret

    m.destroy()

def read_conn_info():

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret, info = m.read_conn_info()
    if ret != MEMLINK_OK:
        print 'read_conn_info error: ', ret
    if info:
        print info

    info.close()
    m.destroy()

def write_conn_info():

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret, info = m.write_conn_info()
    if ret != MEMLINK_OK:
        print 'write_conn_info error: ', ret
    if info:
        print info

    info.close()

    m.destroy()

def sync_conn_info():

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)

    ret, info = m.sync_conn_info()
    if ret != MEMLINK_OK:
        print 'sync_conn_info error: ', ret
    if info:
        print info

    info.close()

    m.destroy()

def insert_mkv(*args):

    if len(args) < 4 or len(args) % 4:
        print 'args usage: key1 value1 attr1 pos1 key1 value2 attr2 pos2...'
        return

    imkv = []
    i = 0
    while i < len(args):
        imkv.append((args[i], args[i+1], args[i+2], args[i+3]))
        i = i + 4;

    m = MemLinkClient('127.0.0.1', READ_PORT, WRITE_PORT, 30)
    ret, mkv = m.insert_mkv(tuple(imkv))
    if ret != MEMLINK_OK:
        print 'insert mkv error: ', ret

    print 'kindex: %d, vindex: %d\n' % (mkv.kindex, mkv.vindex)

    mkv.close()

    m.destroy()

if __name__ == '__main__':
    action = sys.argv[1]
    args = []
    if len(sys.argv) > 2:
        args.extend(sys.argv[2:])

    func = globals()[action]
    func(*args)
            


