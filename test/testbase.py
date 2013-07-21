# coding: utf-8
import os, sys

def memlink_addr(path=''):
    if not path.endswith('test.h'):
        path = os.path.join(path, 'test.h')
    f = open(path, 'r')
    lines = f.readlines()
    f.close()

    rport = wport = 0

    for line in lines:
        line = line.strip()
        if line.find('MEMLINK_READ_PORT') > 0:
            rport = int(line.split()[-1])
        elif line.find('MEMLINK_WRITE_PORT') > 0:
            wport = int(line.split()[-1])

    return rport, wport
