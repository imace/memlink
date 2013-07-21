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
    
    sync_test_clean()
    test_init()
    change_synclog_indexnum(5000000);
    #os.system("bash clean.sh")
    data_produce1()
    
    print 
    print '============================= test h  =============================='
    cmd = 'rm test.log'
    print cmd
    os.system(cmd)
    x1 = restart_master()
    time.sleep(3)
    '''
    cmd = 'rm data/*'
    print cmd
    os.system(cmd)
    cmd = 'cp data_bak/* data/'
    print cmd
    os.system(cmd)
    
    
    cmd = 'rm data/dump.dat'
    print cmd
    os.system(cmd)
    cmd = 'mv data/dump.dat_bak data/dump.dat'
    os.system(cmd)'''

    x2 = start_a_new_slave()
    print 'sleep 10'
    time.sleep(10)
    
    '''
    cmd = 'cp data_bak/dump.dat data/'
    print cmd
    os.system(cmd)
    '''
    
    #kill master 1
    print 'kill master!'
    x1.kill()
    x1 = restart_master()
    time.sleep(30) # wait master to load data

    #kill master 1
    print 'kill master!'
    x1.kill()
    x1 = restart_master()
    time.sleep(30) # wait master to load data

    #kill master 1
    print 'kill master!'
    x1.kill()
    x1 = restart_master()
    time.sleep(10) # wait master to load data

    #kill master 1
    print 'kill master!'
    x1.kill()
    x1 = restart_master()
    time.sleep(10) # wait master to load data

    #kill master 1
    print 'kill master!'
    x1.kill()
    x1 = restart_master()
    time.sleep(30) # wait master to load data
    
    #cmd = 'cp data_bak/dump.dat_bak data/dump.dat'

    if 0 != stat_check(client2master, client2slave):
        print 'test h error!'
        return -1
    print 'stat ok'
    
    print 'test h ok'

    #x1.kill()
    #x2.kill()

    client2master.destroy()
    client2slave.destroy()
    sync_test_clean()

    return 0

if __name__ == '__main__':
    sys.exit(test())

