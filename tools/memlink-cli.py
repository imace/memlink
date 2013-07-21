#coding:utf-8
import os, sys
clientpath = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'client/python')
sys.path.append(clientpath)
import time
import getopt
from memlinkclient import *
import string
#print dir(memlink)

READ_PORT  = 11001
WRITE_PORT = 11002

class ShortInputException (Exception):
    def __init__(self, length, atleast):
        Exception.__init__(self)
        self.length = length
        self.atleast = atleast

class MemLinkTestException (Exception):
    pass

class MemLinkTest:
    def __init__(self, host, rp, wp, timeout):
        self.ip      = host
        self.r_port  = int(rp)
        self.w_port  = int(wp)
        self.timeout = int(timeout)

        print 'memlink: %s r:%d w:%d timeout:%d' % (self.ip, self.r_port, self.w_port, self.timeout)

        self.m = MemLinkClient(self.ip, self.r_port, self.w_port, self.timeout)
        if not self.m:
            raise MemLinkTestException

        self.allkey = {}
        self.maskformat = '4:3:1'
        self.valuesize = 12

    def create(self, args):
        '''create key valuesize [maskformat]
\tIf maskformat is not input, it'll be set to ''. In this case, the maskstr
\tmust be '' too when values are inserted.
\teg: create haha 12 4:3:1
        '''
        try:
            argc = len(args)
            if argc < 2:
                raise ShortInputException(argc, 2)
            elif argc == 2:
                key = args[0]
                valuesize = int(args[1])
                maskformat = ''
            elif argc >= 3:
                key = args[0]
                valuesize = int(args[1])
                maskformat = args[2]
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
        
        ret = self.m.create_list(key, valuesize, maskformat)
        print 'create (%s %d %s) '% (key, valuesize, maskformat), 
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret

    def rmkey(self, args):
        '''rmkey key
\tremove key and all values
        '''
        try:
            argc = len(args)
            if argc < 1:
                raise ShortInputException(argc, 1)
            else:
                key = args[0]
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1

        ret = self.m.rmkey(key)
        print 'rmkey %s '% key, 
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret
                
    def insert(self, args):
        '''insert key value pos [maskstr]
\tIf maskstr is not input, it'll be set to ''. In this case, it'll only be used 
\twhen the maskformat is '', or it will cause an error!
\teg: insert haha value 0 (8:2:1)  
        '''
        #insert key value  --- mask is set to '8:2:1' and pos to 0 implicitly.
        try:
            num = 1
            i = 0
            while i < len(args):
                #print args[i]
                if args[i].startswith('-n'):
                    numstr = args[i][2:]
                    if numstr: num = int(numstr)
                    del args[i]
                else:
                    i += 1
                
            argv = len(args)                
            if argv < 2:
                raise ShortInputException(argv, 2)
            if argv == 2:
                key = args[0]
                value = args[1]
                maskstr = '8:2:1'
                pos = 0
            elif argv == 3:
                key = args[0]
                value = args[1]
                pos = int(args[2])
                maskstr = ''
            else:
                key = args[0]
                value = args[1]
                maskstr = args[3]
                pos = int(args[2])
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
                
        if key not in self.allkey:
            ret, stat = self.m.stat(key)
            if ret == MEMLINK_OK:
                keyinfo = [12, '4:3:1']
                keyinfo[0] = stat.valuesize
                self.allkey[key] = keyinfo
            elif ret == MEMLINK_ERR_RECV:
                return ret
            elif ret == MEMLINK_ERR_NOKEY:
                print 'key does not exist!'
                return ret

        ret = 0
        for i in xrange(0, num):
            myvalue = value
            if pos == -1:
                pos = i
            valuesize = self.allkey[key][0]
            llen = len(value)
            sstr = '%d' % i
            n = 0
            n = valuesize - llen - len(sstr)
            if n < 0:
                n = 0
            myvalue += '0'*n + sstr
            ret = self.m.insert(key, myvalue, maskstr, pos)
            print 'insert (%s, %s, %s, %d) ' % (key, myvalue, maskstr, pos),
            if ret == MEMLINK_OK:
                print 'OK!'
            elif ret == MEMLINK_ERR_RECV:
                break;
            else:
                print 'ERROR: ', ret
                break;
            
        return ret
    
    def range(self, args):
        '''range [option] key frompos len maskstr 
Options:
    
    -v : visible value (default option)   --- for instance: range -v haha 10 1000 (8::1)
    
    -a : all value                        
    
    -t : tagdel value                     --- for instance: range -t haha 10 1000
    
    -r : removed value  
        '''
        #range haha
        try:
            optype = MEMLINK_VALUE_VISIBLE
            i = 0
            while i < len(args):
                if args[i].startswith('-'):
                    if args[i] == '-a':
                        optype = MEMLINK_VALUE_ALL
                    elif args[i] == '-v':
                        optype = MEMLINK_VALUE_VISIBLE
                    elif args[i] == '-t':
                        optype = MEMLINK_VALUE_TAGDEL
                    elif args[i] == '-r':
                        optype = MEMLINK_VALUE_REMOVED
                    else:
                        print 'Option Is Not Found!'
                        return -1
                    del args[i]
                else:
                    i += 1
        
            argv = len(args)
            if argv < 1:
                raise ShortInputException(argv, 1)
            if argv == 1:
                key = args[0]
                maskstr = ''
                frompos = 0
                llen = 1000
            elif argv == 3:
                key = args[0]
                frompos = int(args[1])
                llen = int(args[2])
                maskstr = ''
            elif argv == 4:
                key = args[0]
                frompos = int(args[1])
                llen = int(args[2])
                maskstr = args[3]
            else:
                print 'bad input!'
                return -1
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1

        ret, recs = self.m.range(key, optype, maskstr, frompos, llen)
        print 'range (%s, "%s", %d, %d) ' % (key, maskstr, frompos, llen),
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            return ret
        else:
            print 'ERROR: ', ret
            return ret
            
        print 'range count:', recs.count
        items = recs.root
        while items:
            #print items.value, repr(items.value), items.mask
            print items.value, items.mask            
            items = items.next
            
        return 0

    def delete(self, args):
        '''delete [option] key value/maskstr
Options:
    -v : delete by value (default option)   --- for instance: delete (-v) haha 123
    -m : delete by mask                     --- for instance: delete -m haha 4::1
        '''
        #delete key frompos len : delete haha 10 50        
        try:
            optype = 0
            i = 0
            while i < len(args):
                if args[i].startswith('-'):
                    if args[i] == '-m':
                        optype = 1
                    elif args[i] == '-v':
                        optype = 0
                    else:
                        print 'Option Is Not Found!'
                        return -1
                    del args[i]
                else:
                    i += 1
        
            argc = len(args)
            if argc < 2:
                raise ShortInputException(argc, 2)
                    
            multidel = 0
            if argc == 2:
                key = args[0]
                if optype == 0:
                    value = args[1]
                elif optype == 1:
                    maskstr = args[1]
            elif argc == 3:
                key = args[0]
                frompos = int(args[1])
                llen = int(args[2])
                multidel = 1
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1

        if multidel:
            ret, result = self.m.range(key, MEMLINK_VALUE_VISIBLE, "", frompos, llen)
            if ret == MEMLINK_OK:
                num = 0
                item = result.root
                while item:
                    ret = self.m.delete(key, item.value)
                    print 'delete (%s, %s) '% (key, item.value),
                    if ret == MEMLINK_OK:
                        num += 1
                        print 'OK!'
                    elif ret == MEMLINK_ERR_RECV:
                        return ret;
                    else:
                        print 'ERROR: ', ret
                        break;
                    item = item.next
                print 'multidel %d' % num
                return ret;
            elif ret == MEMLINK_ERR_RECV:
                pass
            else:
                print 'ERROR: ', ret
            return ret
            
        if optype == 0:
            ret = self.m.delete(key, value)
            print 'delete (%s, %s) '% (key, value), 
            if ret == MEMLINK_OK:
                print 'OK!'
            elif ret == MEMLINK_ERR_RECV:
                pass
            else:
                print 'ERROR: ', ret
            return ret
        elif optype == 1:
            ret = self.m.delete_by_mask(key, maskstr)
            print 'delete (%s, %s) '% (key, maskstr), 
            if ret == MEMLINK_OK:
                print 'OK!'
            elif ret == MEMLINK_ERR_RECV:
                pass
            else:
                print 'ERROR: ', ret
            return ret
            
    def move(self, args):
        '''move key value pos   
\tmove value to pos
\teg: move haha 123 0
        '''
        try:
            argc = len(args)
            if argc < 3:
                raise ShortInputException(argv, 3)
            key = args[0]
            value = args[1]
            pos = int(args[2])
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
            
        ret = self.m.move(key, value, pos)
        print 'move (%s, %s, %d) ' % (key, value, pos)
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret

    def stat(self, args):
        '''stat [option] key
Options:
    -k : show stat of some key      --- for instance:  stat haha  or  stat -k haha
    -s : show stat of the memlink   --- for instance:  stat  or  stat -s
        '''
        try:
            argc = len(args)
            if argc == 0:  #stat
                optype = 1
            else: #other
                optype = 0
                
            i = 0
            while i < len(args):
                if args[i].startswith('-'):
                    if args[i] == '-k':
                        optype = 0
                    elif args[i] == '-s':
                        optype = 1
                    else:
                        print 'Option Is Not Found!'
                        return -1
                    del args[i]
                else:
                    i += 1
            if optype == 0:
                key = args[0]
        except:
            print 'bad input! stat must have one param!'
            return -1

        if optype == 0:
            ret, stat = self.m.stat(key)
            print 'stat %s ' % key,            
        elif optype == 1:
            ret, stat = self.m.stat_sys()
            print 'stat ',
        if ret == MEMLINK_OK:
            print 'OK!'
            print
            print stat
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret
     
    def dump(self, args):
        '''dump
\twrite all memory data to disk
        '''
        ret = self.m.dump()
        print 'dump ',
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret

    def clean(self, args):
        '''clean key
        '''
        try:
            argc = len(args)
            if argc < 1:
                raise ShortInputException(argc, 1)
            key = args[0]
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
                
        ret = self.m.clean(key)
        print 'clean %s ' % key,
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret
                
    def tag(self, args):
        '''tag key value flag  
\teg: 1 for tag_del ; 0 for restore
        '''
        try:
            argv = len(args)
            if argv < 3:
                raise ShortInputException(argv, 3)
            key = args[0]
            value = args[1]
            flag = int(args[2])
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
            
        ret = self.m.tag(key, value, flag)
        print 'tag (%s, %s, %d) ' % (key, value, flag), 
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret

    def mask(self, args):
        '''mask key value maskstr
\teg: update haha 123 0
        '''
        try:
            argv = len(args)
            if argv < 3:
                raise ShortInputException(argv, 3)
            key = args[0]
            value = args[1]
            maskstr = args[2]
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
            
        print 'mask (%s, %s, "%s") ' % (key, value, maskstr),
        ret = self.m.mask(key, value, maskstr)
        if ret == MEMLINK_OK:
            print 'OK!'
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret

    def count(self, args):
        '''count key maskstr
\teg: count haha 4::1 ; count haha (count all)
        '''
        try:
            argv = len(args)
            if argv < 1:
                raise ShortInputException(argv, 1)
            elif argv == 1:
                key = args[0]
                maskstr = ''
            else:
                key = args[0]
                maskstr = args[1]
        except ShortInputException, x:
            print 'bad input! expected at least %d' % x.atleast
            return -1
            
        ret, count = self.m.count(key, maskstr)
        print 'count (%s, "%s") ' % (key, maskstr),
        if ret == MEMLINK_OK:
            print 'OK!'
            print 'visible_count:%d, tagdel_count:%d' % (count.visible_count, count.tagdel_count)
        elif ret == MEMLINK_ERR_RECV:
            pass
        else:
            print 'ERROR: ', ret
        return ret;

def test_main():
    args = sys.argv[1:]
    optlist, args = getopt.getopt(args, 'h:r:w:t:')
    opts = dict(optlist)
 
    all_the_cmd = ('create', 'rmkey', 'insert', 'delete', 'range', 
                   'move', 'tag', 'mask', 'count', 'stat', 'dump', 'clean')

    all_cmd_str = 'commands:\n\t%s' % ' '.join(all_the_cmd)
    mtest = MemLinkTest(opts.get('-h', '127.0.0.1'),
                        opts.get('-r', '11011'),
                        opts.get('-w', '11012'),
                        opts.get('-t', '30'))
    print 'command:'
    print '\thelp -- type "help / help command" for some help. for instance: help insert.'
    print '\tq    -- quit.'
    print ''
    
    while True:
        try:
            sstr = raw_input('memlink> ')
        except EOFError:
            return
        if not sstr:
            continue
        if sstr in ('q', 'quit'):
            return

        cmd_str = string.split(sstr)
        cmd     = cmd_str[0]

        if not hasattr(mtest, cmd) and cmd != 'help'and cmd != 'list':
            print 'Bad input! DO NOT have this command.'
            continue

        if cmd == 'help':
            if len(cmd_str) < 2:
                print all_cmd_str
            elif hasattr(mtest, cmd_str[1]):
                print getattr(mtest, cmd_str[1]).__doc__
            else:
                print 'command "%s" don\'t exist! All are as follows:' % cmd_str[1]
                print all_cmd_str
        else:
            opnum = 0;
            while True:
                opnum += 1
                args = cmd_str[1:]
                ret = getattr(mtest, cmd)(args)
                if ret == MEMLINK_ERR_RECV:
                    if opnum < 2:
                        continue;
                    else:
                        print "Connection ERROR! May be lost!"
                        break;
                else:
                    break;

if __name__ == '__main__':
    test_main()


