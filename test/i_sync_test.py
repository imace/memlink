#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
from memlinkclient import *
from synctest import *

def test():
    client2master = MemLinkClient('127.0.0.1', MASTER_READ_PORT, MASTER_WRITE_PORT, 30);
    client2slave  = MemLinkClient('127.0.0.1', SLAVE_READ_PORT, SLAVE_WRITE_PORT, 30);
    
    test_init()
    data_produce2()

    print 
    print '============================= test i  =============================='
    cmd = 'rm test.log'
    print cmd
    os.system(cmd)
    cmd = 'rm -rf data/*'
    print cmd
    os.system(cmd)
    cmd = 'cp -rf data_bak2/* data/'
    print cmd
    os.system(cmd)
    
    x1 = restart_master()
    time.sleep(3)

    x2 = start_a_new_slave()
    print 'sleep 10'
    time.sleep(2)
    
    #kill slave 1
    print 'kill slave!'
    x2.kill()
    time.sleep(10)
    x2 = restart_slave()
    time.sleep(10) # wait slave to load data
    '''
    #kill slave 1
    print 'kill slave!'
    x2.kill()
    time.sleep(2)
    x2 = restart_slave()
    time.sleep(6) # wait slave to load data
    ''' 
    '''
    #kill master 1
    print 'kill master!'
    x1.kill()
    x1 = restart_master()
    time.sleep(100) # wait master to load data
    '''    
    if 0 != stat_check(client2master, client2slave):
        print 'test i error!'
        return -1
    print 'stat ok'
    
    print 'test i ok'

    x1.kill()
    x2.kill()

    client2master.destroy()
    client2slave.destroy()

    return 0

if __name__ == '__main__':
    sys.exit(test())

