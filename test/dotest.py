# coding: utf-8
import os, sys
import glob
import subprocess
import time

def testsync(fn, wait = 2):
    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cwd  = os.getcwd()

    print 'home:', home, 'cwd:', cwd
    fpath = os.path.join(home, 'test', fn)
   
    if fpath.endswith('.py'):
        fpath = 'python ' + fpath

    binfiles = glob.glob('data/*')
    for bf in binfiles:
        print 'remove:', bf
        os.remove(bf)

    binfiles = glob.glob('data_slave/*')
    for bf in binfiles:
        print 'remove:', bf
        os.remove(bf)

    cmd = "killall -9 memlink_master"
    os.system(cmd)

    cmd = "killall -9 memlink_slave"
    os.system(cmd)

    print 'run test:', fpath
    ret = os.system(fpath)
    if ret != 0:
        print fn, '\t\t\33[31mfailed!\33[0m'
    else:
        print fn, '\t\t\33[32msuccess!\33[0m'

    return ret

def testone(fn, wait=2):
    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cwd  = os.getcwd()

    print 'home:', home, 'cwd:', cwd
    fpath = os.path.join(home, 'test', fn)

    memlinkfile = os.path.join(home, 'memlink')
    if not os.path.isfile(memlinkfile):
        print 'not found memlink:', memlinkfile
        return
    memlinkstart = '%s %s/test/memlink.conf' % (memlinkfile, home)

    cmd = "killall -9 memlink"
    os.system(cmd)

    logfile = os.path.join('memlink.log')
    if os.path.isfile(logfile):
        print 'remove log:', logfile
        os.remove(logfile)

    binfiles = glob.glob('data/bin.log*')
    for bf in binfiles:
        print 'remove binlog:', bf
        os.remove(bf)

    binfiles = glob.glob('data/dump.dat*')
    for bf in binfiles:
        print 'remove dump:', bf
        os.remove(bf)

    time.sleep(1)
    print 'open memlink:', memlinkstart
    x = subprocess.Popen(memlinkstart, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                         shell=True, env=os.environ, universal_newlines=True) 
    time.sleep(wait)
   
    if fpath.endswith('.py'):
        fpath = 'python ' + fpath
    print 'run test:', fpath
    ret = os.system(fpath)
    x.kill()
    
    if ret != 0:
        print fn, '\t\t\33[31mfailed!\33[0m'
    else:
        print fn, '\t\t\33[32msuccess!\33[0m'

    return ret

def test():
    if os.path.isfile('dotest.log'):
        os.remove('dotest.log')
    sources = glob.glob("*_test.c")
    files = glob.glob("*_test")
    files.sort()
    print files
    pyfiles = glob.glob("*_test.py")
    pyfiles = ['dump_test.py', 'push_pop_test.py', 'sortlist_test.py', 'vote_test.py']
    files += pyfiles

    syncfile = ['ab_sync_test.py', 'cd_sync_test.py', 'f_sync_test.py', 'g_sync_test.py', 'h_sync_test.py']
    #syncfile = ['g_sync_test.py']
    result = {}
    
    for x in sources:
        biname = x[:-2]
        if biname not in files:
            result[biname] = 0

    print 'do all test ...'

    failed = 0
    for fn in files:
        ret = testone(fn)
        if ret != 0: # failed
            failed += 1
            result[fn] = 0
        else: # success
            result[fn] = 1

    for fn in syncfile:
        ret = testsync(fn)
        if ret != 0:
            failed += 1
            result[fn] = 0
        else:
            result[fn] = 1

    f = open('dotest.log', 'w')
    for k,v in result.iteritems():
        if v == 0:    
            f.write('%s\t0\n' % k)
        else:
            f.write('%s\t1\n' % k)
    f.close()

    #return result
    if failed > 0:
        return -1
    return 0

if __name__ == '__main__':
    if len(sys.argv) > 1:
        sys.exit(testone(sys.argv[1]))
    else:
        sys.exit(test())

