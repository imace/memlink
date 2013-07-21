#!/usr/bin/python
# coding: utf-8
import os, sys
home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(home, "client/python"))
import time
import subprocess
import glob
from memlinkclient import *

MASTER_READ_PORT  = 21011
MASTER_WRITE_PORT = 21012

SLAVE_READ_PORT  = 21021
SLAVE_WRITE_PORT = 21022

memlink_master_start = ''
memlink_slave_start  = ''

def stat_check(client2master, client2slave):
    ret1, stat1 = client2master.stat_sys()
    ret2, stat2 = client2slave.stat_sys()
    if stat1 and stat2:
        if stat1.keys != stat2.keys or stat1.values != stat2.values or stat1.logline != stat2.logline:
            print 'stat error!'
            print 'ret1: %d, ret2: %d' % (ret1, ret2)
            print 'master stat', stat1
            print 'slave stat', stat2
            return -1
    else:
        print 'stat error!'
        print 'ret1: %d, ret2: %d' % (ret1, ret2)
        return -1

    return 0

def result_check(client2master, client2slave, name, key):
    ret, st1 = client2master.stat(name, key)
    num1 = st1.data_used
    ret1, rs1 = client2master.range(name, key, MEMLINK_VALUE_VISIBLE, 0, num1)

    ret, st2 = client2master.stat(name, key)
    num2 = st2.data_used
    ret2, rs2 = client2master.range(name, key, MEMLINK_VALUE_VISIBLE, 0, num2)

    if rs1 and rs2:
        pass
    else:
        print 'error: rs1 or rs2 is null'
        return -1

    if rs1.count != rs2.count:
        print 'error ', rs1.count, rs2.count
        return -1
        
    it1 = rs1.items
    it2 = rs2.items

    while it1 and it2:
        if it1.value != it2.value:
            print 'error ', it1.value, it2.value
            return -1
        if it1.attr != it2.attr:
            print 'error', it1.attr, it2.attr
            return -1
        it1 = it1.next
        it2 = it2.next

    if it1 or it2:
        print 'error: it1 or it2 is not null!'
        return -1

    return 0
    
def test_init():
    global memlink_master_start
    global memlink_slave_start
    
    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(home)

    cmd = 'rm memlink_master memlink_slave'
    print cmd 
    os.system(cmd)
    
    cmd = "cp memlink memlink_master"
    print cmd
    os.system(cmd)
    
    cmd = "cp memlink memlink_slave"
    print cmd
    os.system(cmd)

    slave_data_dir = 'data_slave'
    if not os.path.isdir(slave_data_dir):
        cmd = 'mkdir data_slave'
        print cmd
        os.system(cmd)

    filename = 'clean_slave.sh'
    if not os.path.isfile(filename):
        f = open(filename, 'wb')
        f.write('#!/bin/bash\n')
        f.write('rm -rf data_slave/*\n')
        f.write('rm -rf slave.log\n')
        f.close()
    
    memlink_master_file  = os.path.join(home, 'memlink_master')
    memlink_master_start = memlink_master_file + ' test/memlink_master.conf'

    memlink_slave_file  = os.path.join(home, 'memlink_slave')
    memlink_slave_start = memlink_slave_file + ' test/memlink_slave.conf'

def data_produce1():
    #if not os.path.isdir('data_bak'):
    #    cmd = 'mkdir data_bak'
    #    print cmd
    #    os.system(cmd)
    #else:
    #    return 0
        
    client2master = MemLinkClient('127.0.0.1', MASTER_READ_PORT, MASTER_WRITE_PORT, 30);    
    x1 = start_a_new_master()
    time.sleep(1)
    #cmd = 'cp data/dump.dat data_bak/dump.dat_bak'
    #print cmd
    #os.system(cmd)
    
    #insert 500wan
    num2 = 5000000
    num = 0
    maskstr = '4:2:2'
    name = 'test'
    key = 'haha'
    ret = client2master.create_table_list(name, 12, '3:3:3')
    if ret != MEMLINK_OK:
        print 'create error: %d' % ret
        return -1
    
    for i in xrange(num, num2):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, i, maskstr)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, maskstr, ret
            return -2;
    print 'insert %d val' % (num2 - num)
    ''' 
    client2master.dump()
    cmd = 'cp data/* data_bak/'
    print cmd
    os.system(cmd)
    '''

    #x1.kill()
    client2master.destroy()
    return 0

def data_produce2():
    if not os.path.isdir('data_bak2'):
        cmd = 'mkdir data_bak2'
        print cmd
        os.system(cmd)
    else:
        return 0
        
    client2master = MemLinkClient('127.0.0.1', MASTER_READ_PORT, MASTER_WRITE_PORT, 30);    
    x1 = start_a_new_master()
    time.sleep(1)
    
    #insert 500wan
    num2 = 10000000
    num = 0
    maskstr = '4:2:2'
    name = 'test'
    key = 'haha'
    ret = client2master.create_table_list(name, 12, '3:3:3')
    if ret != MEMLINK_OK:
        print 'create error: %d' % ret
        return -1
    
    for i in xrange(num, num2):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, maskstr, i)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, maskstr, ret
            return -2;
    print 'insert %d val' % (num2 - num)
    
    client2master.dump()
    
    cmd = 'cp data/dump.dat data/bin.log data_bak2/'
    print cmd
    os.system(cmd)

    x1.kill()
    client2master.destroy()
    return 0

def data_produce3():
    if not os.path.isdir('data_bak3'):
        cmd = 'mkdir data_bak3'
        print cmd
        os.system(cmd)
    else:
        return 0
        
    client2master = MemLinkClient('127.0.0.1', MASTER_READ_PORT, MASTER_WRITE_PORT, 30);    
    x1 = start_a_new_master()
    time.sleep(1)
    
    #insert 500wan
    num2 = 20000000
    num = 0
    maskstr = '4:2:2'
    name = 'test'
    key = 'haha'
    ret = client2master.create_table_list(name, 12, '3:3:3')
    if ret != MEMLINK_OK:
        print 'create error: %d' % ret
        return -1
    
    for i in xrange(num, num2):
        val = '%012d' % i
        ret = client2master.insert(name, key, val, maskstr, i)
        if ret != MEMLINK_OK:
            print 'insert error!', key, val, maskstr, ret
            return -2;
    print 'insert %d val' % (num2 - num)
    
    client2master.dump()
    
    cmd = 'cp data/dump.dat data/bin.log data_bak3/'
    print cmd
    os.system(cmd)

    x1.kill()
    client2master.destroy()
    return 0
    
def start_a_new_master():
    global memlink_master_start
    #start a new master
    print 'start a new master:'
    cmd = 'bash clean.sh'
    print '   ',cmd
    os.system(cmd)
    cmd = "killall memlink_master"
    print '   ',cmd
    os.system(cmd)
    print '   ', memlink_master_start
    x1 = subprocess.Popen(memlink_master_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x1

def restart_master():
    cmd = "killall memlink_master"
    print '   ',cmd
    os.system(cmd)
    time.sleep(1)
    global memlink_master_start
    print 'restart master:', memlink_master_start
    x1 = subprocess.Popen(memlink_master_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x1

def start_a_new_slave():
    global memlink_slave_start
    #start a new slave
    print 'start a new slave:'    
    cmd = 'bash clean_slave.sh'
    print '   ',cmd
    os.system(cmd)
    cmd = "killall memlink_slave"
    print '   ',cmd
    os.system(cmd)
    print '   ', memlink_slave_start
    x2 = subprocess.Popen(memlink_slave_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x2

def restart_slave():
    global memlink_slave_start

    cmd = "killall memlink_slave"
    print '   ',cmd
    os.system(cmd)
    time.sleep(1)
    
    cmd = 'python tools/check.py -c data_slave/bin.log -r'
    print cmd
    os.system(cmd)

    print 'restart slave: ', memlink_slave_start
    x2 = subprocess.Popen(memlink_slave_start, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
                             shell=True, env=os.environ, universal_newlines=True)
    return x2

def change_synclog_indexnum(num):
    source_name = "common.h"
    f = open(source_name, 'r')

    source_name_tmp = "common.h.tmp"
    f1 = open(source_name_tmp, 'w+')

    lines = f.readlines()

    for line in lines:
        if '#define SYNCLOG_INDEXNUM' in line:
            newline = "#define SYNCLOG_INDEXNUM    %d\n" %  num
            f1.write(newline)
            continue
        f1.write(line)

    f.close()
    f1.close()

    cmd = 'mv common.h.tmp common.h'
    os.system(cmd)

    cmd = '/usr/local/bin/scons'

    os.system(cmd)
    cmd = 'cp memlink memlink_slave'
    os.system(cmd)
    cmd = 'cp memlink memlink_master'
    os.system(cmd)

def sync_test_clean():
    cmd = "killall -9 memlink_master"
    os.system(cmd)

    cmd = "killall -9 memlink_slave"
    os.system(cmd)


    binfiles = glob.glob('data/*')
    for bf in binfiles:
        print 'remove:', bf
        os.remove(bf)

    binfiles = glob.glob('data_slave/*')
    for bf in binfiles:
        print 'remove:', bf
        os.remove(bf)
    '''
    cmd = "killall -9 memlink_master"
    os.system(cmd)

    cmd = "killall -9 memlink_slave"
    os.system(cmd)
    '''


def mb_sync_test_init(server_nums):

    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(home)
    for i in xrange(0, server_nums):
        bdata_dir = 'data_mb%d' % i
        binfiles = glob.glob(bdata_dir)
        for bf in binfiles:
            print 'remove:', bf
            os.remove(bf)

        cmd = 'rm -rf test/memlink-mb%d' % i
        os.system(cmd)


    for i in xrange(0, server_nums):
        mb = 'memlink-mb%d' % i
        cmd = "killall -9 '%s'" % mb 
        os.system(cmd)

    cmd = '/usr/local/bin/scons'
    os.system(cmd)


    #create config file
    conffile = 'etc/memlink.conf'
    f = open(conffile, 'r')
    lines = f.readlines()

    vconffile = 'etc/vote.conf'
    vf = open(vconffile, 'r')
    vlines = f.readline()
    
    l_wport = []
    port = 30000
    for i in xrange (0, server_nums):
        mconf = 'test/memlink-mb%d.conf' % i
        f1 = open(mconf, "w+") 
        for line in lines:
            if 'read_port' in line:
                newline = 'read_port = %d\n' % int(port + 1)
                f1.write(newline)
                continue
            if 'write_port' in line:
                newline = 'write_port = %d\n' % int(port + 2)
                f1.write(newline)
                l_wport.append(port + 2)
                continue
            if 'sync_port' in line and 'master_sync_port' not in line:
                newline = 'sync_port = %d\n' % int(port + 3)
                f1.write(newline)
                continue
            if 'sync_mode' in line:
                newline = "sync_mode = %s\n" % 'master-backup'
                f1.write(newline)
                continue
            if 'data_dir' in line:
                newline = "data_dir = data_mb%d\n" % int(i)
                f1.write(newline)
                continue
            if 'sync_mode' in line:
                newline = "sync_mode = master-backup\n"
                f1.write(newline)
                continue
            f1.write(line)
        port = port + 1000
        f1.close() 
        cmd = 'cp memlink test/memlink-mb%d' % i 
        os.system(cmd)

    print l_wport
    f.close()
