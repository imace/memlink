# coding: utf-8
import os, sys
import glob
import subprocess
import time

def test(args):
    try:
        testname = args[0]
        num = int(args[1])
    except:
        testname = args[0]
        num = 50
    home = os.path.dirname(os.getcwd())
    os.chdir(home)

    memlinkfile = os.path.join(home, 'memlink')
    if not os.path.isfile(memlinkfile):
        print 'not found memlink'
        return

    memlinkstart = memlinkfile + ' test/memlink.conf'

    fpath = os.path.join(home, 'test', testname)
    print fpath
    
    for i in range(0, num):
        cmd = "killall -9 memlink"
        os.system(cmd)

        logfile = os.path.join(home, 'test.log')
        if os.path.isfile(logfile):
            #print 'remove log:', logfile
            os.remove(logfile)
            
        binfiles = glob.glob('data/bin.log*')
        for bf in binfiles:
            #print 'remove binlog:', bf
            os.remove(bf)
            
        binfiles = glob.glob('data/dump.dat*')
        for bf in binfiles:
            #print 'remove dump:', bf
            os.remove(bf)

        x = subprocess.Popen(memlinkstart, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True) 
        time.sleep(1)
        ret = os.system(fpath)
        x.kill()
        print i, ' result: ', ret
        if (0 != ret):
            return

if __name__ == '__main__':
    args = []
    if len(sys.argv) > 1:
        args.extend(sys.argv[1:])
    #print args
    test(args)

