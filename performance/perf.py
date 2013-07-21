# coding: utf-8
import os, sys
sys.path.append('../client/python')
import getopt, time
from memlinkclient import *

class MemLinkPerf:
    def __init__(self):
        self.host  = '127.0.0.1'
        self.rport = 11011
        self.wport = 11012
        self.timeout = 30
    
    def dotest(self, args):
        doing = args['-d']
        n     = args['-n']
        func = getattr(self, 'test_' + doing) 

        start = time.time()
        func(args)
        end = time.time()

        print 'use time:', end - start, ' speed:', n / (end - start)

    def test_create(self, args):
        return self.test_create_list(args)

    def _test_create(self, args, listtype):
        if args['-c'] == 1:
            m = MemLinkClient(self.host, self.rport, self.wport, self.timeout)
            for i in xrange(0, args['-n']):
                key = '%s%d' % (args['-k'], i)
                ret = m.create(key, args['-s'], args['-m'], listtype, MEMLINK_VALUE_STRING)
                if ret != MEMLINK_OK:
                    print 'create_list error, ret:%d, key:%s, valuesize:%d, mask:%s' % (ret, key, args['-s'], args['-m'])
            m.destroy()
        else:
            for i in xrange(0, args['-n']):
                m = MemLinkClient(self.host, self.rport, self.wport, self.timeout)
                key = '%s%d' % (args['-k'], i)
                ret = m.create_list(key, args['-s'], args['-m'], listtype, MEMLINK_VALUE_STRING)
                if ret != MEMLINK_OK:
                    print 'create_list error, ret:%d, key:%s, valuesize:%d, mask:%s' % (ret, key, args['-s'], args['-m'])
                m.destroy()           


    def test_create_list(self, args):
        return self._test_create(args, MEMLINK_LIST)

    def test_create_queue(self, args):
        return self._test_create(args, MEMLINK_QUEUE)
        
    def test_create_sortlist(self, args):
        return self._test_create(args, MEMLINK_SORTLIST)

    def test_insert(self, args):
        format = "%%0%dd" % (args['-s'])
        n = args['-n']
        if args['-c'] == 1:
            m = MemLinkClient(self.host, self.rport, self.wport, self.timeout)
            for i in xrange(0, n):
                val = format % i
                ret = m.insert(args['-k'], val, args['-m'], args['-p'])
                if ret != MEMLINK_OK:
                    print 'insert error, ret:%d, key:%s, val:%s, mask:%s, pos:%d' % \
                            (ret, args['-k'], val, args['-m'], args['-p'])
                    return
            m.destroy() 
        else:
            for i in xrange(0, n):
                m = MemLinkClient(self.host, self.rport, self.wport, self.timeout)
                val = format % i
                ret = m.insert(args['-k'], val, args['-m'], args['-p'])
                if ret != MEMLINK_OK:
                    print 'insert error, ret:%d, key:%s, val:%s, mask:%s, pos:%d' % \
                            (ret, args['-k'], val, args['-m'], args['-p'])
                    return
                m.destroy() 

    def test_range(self, args):
        if args['-c'] == 1:
            m = MemLinkClient(self.host, self.rport, self.wport, self.timeout)
            for i in xrange(0, n):
                val = format % i
                ret, data = m.range(args['-k'], args['-i'], args['-m'], args['-f'], args['-l'])
                if ret != MEMLINK_OK:
                    print 'insert error, ret:%d, key:%s, kind:%d, from:%d, len:%d' % \
                            (ret, args['-k'], args['-i'], args['-f'], args['-l'])
                    return
                if data.count != args['-l']:
                    print 'range return count error, len:%d, range count:%d' % (args['-l'], range.count)
            m.destroy() 
        else:
            for i in xrange(0, n):
                m = MemLinkClient(self.host, self.rport, self.wport, self.timeout)
                val = format % i
                ret, data = m.range(args['-k'], args['-i'], args['-m'], args['-f'], args['-l'])
                if ret != MEMLINK_OK:
                    print 'insert error, ret:%d, key:%s, kind:%d, from:%d, len:%d' % \
                            (ret, args['-k'], args['-i'], args['-f'], args['-l'])
                    return
                if data.count != args['-l']:
                    print 'range return count error, len:%d, range count:%d' % (args['-l'], range.count)
                m.destroy() 


    def test_move(self, args):
        pass

    def test_del(self, args):
        pass

    def test_mask(self, args):
        pass

    def test_tag(self, args):
        pass

    def test_count(self, args):
        pass

def show_help():
    print "usage:\n\tperf [options]\n"
    print "options:\n"
    print "\t--help,-h\tshow help information\n"
    print "\t--thread,-t\tthread count for test\n"
    print "\t--count,-n\ttest count\n"
    print "\t--from,-f\trange from position\n"
    print "\t--len,-l\trange length\n"
    print "\t--do,-d\t\tcreate/insert/range/move/del/mask/tag/count\n"
    print "\t--key,-k\tkey\n"
    print "\t--value,-v\tvalue\n"
    print "\t--valuesize,-s\tvaluesize for create\n"
    print "\t--pos,-p\tposition for insert\n"
    print "\t--popnum,-o\tpop number\n"
    print "\t--kind,-i\tkind for range. all/visible/tagdel\n"
    print "\t--mask,-m\tmask str\n"
    print "\t--longconn,-c\tuse long connection for test. default 1\n"
    print "\n\n"

    
def main():
    args = sys.argv[1:]
    optlist, args = getopt.getopt(args, 'ht:n:f:l:k:v:s:p:o:i:a:m:d:c:')
    opts = dict(optlist)

    def default_val(items, values):
        for name,val in values:
            if not items.has_key(name):
                items[name] = val

    def convert_int(items, names):
        for name in names:
            items[name] = int(items[name])

    def error_must(items, names):
        for name in names:
            if not items.has_key(name):
                print 'error: must set %s xxx' % (name)
                sys.exit(-1)

    error_must(opts, ['-d', '-n'])

    v = opts.get('kind')
    if v:
        if v == 'all':
            opts['-i'] = 0
        elif v == 'visible':
            opts['-i'] = 1
        else:
            opts['-i'] = 2

    defv = [('-t', '1'), ('-f', '0'), ('-l', '0'), ('-s', '0'), 
            ('-p', '0'), ('-o', '1'), ('-a', '0'), ('-c', '1'), 
            ('-m', '')]
    default_val(opts, defv)
    
    conv = ['-t', '-n', '-f', '-l', '-s', '-p', '-o', '-a', '-c']
    convert_int(opts, conv)
     
    test = MemLinkPerf()
    test.dotest(opts)

if __name__ == '__main__':
    main()

