# coding: utf-8
import os, sys
import glob
import subprocess
import time

home = os.path.dirname(os.getcwd())

def testone(filename):
    fpath = os.path.join(home, 'unittest', filename)
    print 'run test:', fpath
    ret = os.system(fpath)
    if ret == 0:
        print filename, '\t\t\33[32msuccess!\33[0m' 
    else:
        print filename, '\t\t\33[31mfailed!\33[0m' 

    return ret

def test():
    if os.path.isfile('dotest.log'):
        os.remove('dotest.log')

    sources = glob.glob("*_test.c")
    files = glob.glob("*_test")
    files.sort()
    print files

    result = {}
    for x in sources:
        biname = x[:-2]
        if biname not in files:
            result[biname] = 0

    for fn in files:
        ret = testone(fn)
        if ret != 0: # failed
            result[fn] = 0
            #print fn, '\t\t\33[31mfailed!\33[0m'
        else: # succ
            result[fn] = 1
            #print fn, '\t\t\33[32msuccess!\33[0m'

    print '=' * 60
    failed = 0
    f = open('dotest.log', 'w')
    for k,v in result.iteritems():
        if v:
            print k, '\t\t\33[32msuccess!\33[0m' 
            f.write('%s\t1\n' % k)
        else:
            failed += 1
            print k, '\t\t\33[31mfailed!\33[0m' 
            f.write('%s\t0\n' % k)
    f.close()
    
    #return result

    if failed > 0:
        return -1
    else:
        return 0

if __name__ == '__main__':
    if len(sys.argv) == 1:
        sys.exit(test())
    else:
        testone(sys.argv[1])

