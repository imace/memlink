from socket import *
from struct import *
import sys
import subprocess
import threading
import time
import random
import signal
import os

CMD_VOTE = 200
CMD_VOTE_WAIT = 201
CMD_VOTE_MASTER = 202
CMD_VOTE_BACKUP = 203
CMD_VOTE_NONEED = 204
MEMLINK_ERR_VOTE_PARAM = -45
MEMLINK_OK = 0
is_sigint = False

def sigint_handler(signum, frame):
    global is_sigint
    is_sigint = True
    print "SIGINT HANDLED\n"

def parse_conf(file):
    try:
        f = open(file, "r")
    except:
        print "open %s error\n" % file
        sys.exit(-1)

    hosts = []
    key = ''
    lines = 0
    for line in f:
        lines = lines + 1
        line = line.strip(' \t\r\n')
        if (len(line) == 0 or line[0] == '#'):
            continue
        if ('=' not in line):
            if (len(key) == 0):
                print "configure error %d" % lines
                sys.exit(-1)
        else:
            l = line.split('=')
            key = l[0].strip()
            if (len(key) == 0):
                print "configure error %d" % lines
                sys.exit(-1)
            line = l[1].strip(' \t,')

        if (cmp(key, "host") == 0):
            line = line.strip(',')
            if (len(line) != 0):
                l = line.split()
                hosts.append((l[0], l[1]))
        elif (cmp(key, "bindport") == 0):
            hosts.append(("", line))

    return hosts

def request_vote(voteid, id, idflag, wport, port, ret):
    s = socket(AF_INET, SOCK_STREAM)
    try:
        s.connect(("127.0.0.1", port))
    except error, e:
        print "connect error: %s" % e
        return

    cmd = [pack("B", CMD_VOTE), pack("Q", id), pack("B", idflag), pack("Q", voteid), pack("H", wport)]
    cmdlen = pack("I", 20)
    s.send(cmdlen)
    for c in cmd:
        s.send(c)

    ret.append(wport)

    while (1):
        h = s.recv(4)
        if (len(h) == 0):
            s.close()
            return
        l = unpack("I", h)[0]
        index = 0
        data = s.recv(l)
        
        cmd = unpack("B", data[index])[0]
        index = index + 1
        retcode = unpack("H", data[index:index+2])[0]
        index = index + 2
        l = l - 3
        if (cmd == CMD_VOTE):
            if (retcode == CMD_VOTE_MASTER):
                print "I AM MASTER:\n"
                vid = unpack("Q", data[index:index+8])[0]
                index = index + 8
                l = l - calcsize("Q")
                while (l > 0):
                    iplen = 0
                    i = index
                    while (data[i] != '\0'):
                        iplen = iplen + 1
                        i = i + 1
                    host = unpack("%ds" % iplen, data[index:index+iplen])[0]
                    index = index + iplen + 1
                    l = l - iplen - 1
                    port = unpack("H", data[index:index+2])[0]
                    index = index + 2
                    l = l - 2
                    print "\tHOST: %s(%d)\n" % (host, port)

                ret.append(retcode)
                ret.append(vid)

                cmd = [pack("I", 2), pack("H", MEMLINK_OK)]
                for c in cmd:
                    s.send(c)
                continue
            elif (retcode == CMD_VOTE_BACKUP):
                print "I AM BACKUP:\n"
                vid = unpack("Q", data[index:index+8])[0]
                index = index + 8
                l = l - calcsize("Q")
                iplen = 0
                i = index
                while (data[i] != '\0'):
                    iplen = iplen + 1
                    i = i + 1
                host = unpack("%ds" % iplen, data[index:index+iplen])[0]
                index = index + iplen + 1
                l = l - iplen - 1
                port = unpack("H", data[index:index+2])[0]
                index = index + 2
                l = l - 2
                print "\tHOST: %s(%d)\n" % (host, port)

                ret.append(retcode)
                ret.append(vid)

                cmd = [pack("I", 2), pack("H", MEMLINK_OK)]
                for c in cmd:
                    s.send(c)
                continue
            elif (retcode == CMD_VOTE_WAIT):
                print "VOTE WAITING...\n"
                continue
            elif (retcode == CMD_VOTE_NONEED):
                print "VOTE NONEED...\n"
                continue
            elif (retcode == MEMLINK_ERR_VOTE_PARAM):
                print "VOTE PARAM ERROR...\n"
                continue

def client(param, ret, port):

    request_vote(param['voteid'], param['id'], param['idflag'], param['wport'], port, ret)

if __name__ == '__main__':
    hosts = parse_conf("./vote_config")
#    print hosts

    COMMITED = 1
    ROLLBACKED = 2

    vote = []
    port = 0
    voteid = 0
    index = 0

    try:
        votep = subprocess.Popen(["../vote/vote", "vote_config"])
    except:
        print "vote process error\n"
        sys.exit(-1)

    time.sleep(1)
    pidp = subprocess.Popen("netstat -antp 2>/dev/null| grep :11000 | awk '{print $7}' | awk -F '/' '{print $1}'", stdout=subprocess.PIPE, shell=True)
    #print pidp.stdout.readline()
    #sys.exit(0)
    pid = int(pidp.stdout.readline().strip())

    try:
        for host in hosts:
            if (len(host[0]) == 0):
                port = int(host[1])
                print "port: %d\n" % port
            else:
                vote.append({})

                vote[index]['id'] = 0
                vote[index]['idflag'] = COMMITED
                vote[index]['wport'] = int(host[1])
                index = index + 1

        time.sleep(1)

        signal.signal(signal.SIGINT, sigint_handler)

        while (1):
            threads = []
            i = 0
            j = random.randint(0, 2)
            ret = []
            while (i < index):
                param = {}
                ret.append([])
            
                param['voteid'] = voteid+1
                param['id'] = vote[j]['id']
                param['idflag'] = vote[j]['idflag']
                param['wport'] = vote[j]['wport']
                threads.append(threading.Thread(target=client, args=(param, ret[i], port)))
                threads[i].start()
                i = i + 1
                j = (j + 1) % 3
                time.sleep(1)

            for t in threads:
                t.join()

            for l in ret:
                if (l[1] != CMD_VOTE_NONEED and l[1] != MEMLINK_ERR_VOTE_PARAM):
                    print "%d voteid: %d" % (l[0], l[2])
                    if (l[2] > voteid):
                        voteid = l[2]
        
            if (is_sigint == True):
                os.kill(pid, signal.SIGKILL)
                votep.kill()
                sys.exit(1)

            time.sleep(2)
            break
    except:
        os.kill(pid, signal.SIGKILL)
        votep.kill()
        sys.exit(-1)

    os.kill(pid, signal.SIGKILL)
    votep.kill()
    sys.exit(0)
