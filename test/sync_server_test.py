# coding: utf-8
import os, sys
import socket
from errno import EINTR
import struct
import time
import traceback

CMD_SYNC    = 100 
CMD_GETDUMP = 101

CMD_DEL     = 5
CMD_INSERT  = 6

class SyncServer:
    def __init__(self, host, port, size=0):
        self.host = host
        self.port = port

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((host, port))
        self.sock.listen(32)

        self.dump_filename = "mydump.dat"

        self.logver  = 0
        self.logline = 0

        self.dumpdatalist = []

        self.seqid = 0

        f = open(self.dump_filename, 'rb')
        self.dumpdata = f.read()
        f.close()

        self.dumpfile_format = struct.unpack("H", self.dumpdata[:2])[0]
        self.dumpfile_ver    = struct.unpack('I', self.dumpdata[2:6])[0]
        self.dumpfile_logver = struct.unpack('I', self.dumpdata[6:10])[0]
        print 'dump format:%d ver:%d' % (self.dumpfile_format, self.dumpfile_ver)
        
        self.logver  = self.dumpfile_logver
        self.logline = len(self.dumpdatalist)

        for i in xrange(0, size):
            self.dumpdatalist.append(self.make_data(self.logver, self.logline))
            self.logline = len(self.dumpdatalist)


    def loop(self):
        while True:
            try:
                newsock, newaddr = self.sock.accept()
            except socket.error, why:
                if why.args[0] == EINTR:
                    continue
                raise
            print 'new conn:', newsock, newaddr
            try:
                self.run(newsock)
            except:
                traceback.print_exc()
            finally:
                newsock.close()
    
    def run(self, sock):
        cli_logline = 0
        while (True):
            dlen, cmd, param1, param2 = self.recv_cmd(sock)
            print 'recv len:%d, cmd:%d, %d %d' % (dlen, cmd, param1, param2)
            print 'local logver:%d, logline:%d' % (self.logver, self.logline)

            if cmd == CMD_SYNC:
                if self.logver == param1 and self.logline >= param2:
                    cli_logline = param2
                    self.seqid = param2
                    self.send_sync_cmd(sock, 0) 
                    break
                else:
                    self.send_sync_cmd(sock, 1) 
            else:
                if param2 > 0:
                    self.send_getdump_cmd(sock, 1, self.dumpfile_ver, len(self.dumpdata) - param2)
                    print 'send dump:', sock.send(self.dumpdata[param2:])
                else:
                    self.send_getdump_cmd(sock, 1, self.dumpfile_ver, len(self.dumpdata))
                    print 'send dump:', sock.send(self.dumpdata[:len(self.dumpdata)/2])
                    #self.sock.close()
                    #sys.exit()
                print 'send dump ok!'

        while True:
            print 'send data logver:%d, logline:%d, client_loglien:%d' % (self.logver, self.logline, cli_logline)
            s = self.send_data(sock, self.logver, cli_logline)
            time.sleep(1)

            if cli_logline > self.logline:
                print 'add to dumpdatalist:', repr(s)
                self.dumpdatalist.append(s) 
                self.logline = len(self.dumpdatalist)
            cli_logline += 1

    def recv_cmd(self, sock):
        hlen = sock.recv(2)
        dlen = struct.unpack('H', hlen)[0]
        print 'head len:', dlen
        s = sock.recv(dlen)
        ss = hlen + s
        print 'recv:', repr(ss)
        return self.decode(ss)

    def send_sync_cmd(self, sock, ret):
        print 'send sync:', ret
        s  = struct.pack('H', ret)
        ss = struct.pack('I', len(s)) + s
        print 'reply sync:', repr(ss)
        sock.send(ss)

    def send_getdump_cmd(self, sock, ret, dumpver, size):
        print 'send getdump:', ret,  dumpver, size
        s  = struct.pack('H', ret) + struct.pack('I', dumpver) + struct.pack('Q', size)
        ss = struct.pack('I', len(s)) + s 
        print 'reply getdump:', repr(ss)
        sock.send(ss)

    def send_data(self, sock, logver, logline):
        '''logver(4B) + logline(4B) + len(2B) + cmd(1B) + data''' 
        print 'send data:', logver, logline
        #s  = struct.pack('BB4sB12sBII', CMD_INSERT, 4, 'haha', 12, 'xx%010d' % self.seqid, 1, 1, 0)
        #ss = struct.pack('IIH', logver, logline, len(s)) + s
        #self.seqid += 1

        ss = self.make_data(logver, logline)
        print 'push:', repr(ss)
        sock.send(ss)
        return ss

    def make_data(self, logver, logline):
        s  = struct.pack('BB4sB12sBII', CMD_INSERT, 4, 'haha', 12, 'xx%010d' % self.seqid, 1, 1, 0)
        ss = struct.pack('IIH', logver, logline, len(s)) + s
        self.seqid += 1

        return ss

    def decode(self, data):
        '''datalen(2B) + cmd(1B) + logver/dumpver(4B) + logline(4B)/size(8B)'''
        dlen, cmd = struct.unpack('HB', data[:3])
        
        if cmd == CMD_SYNC:
            logver, logline = struct.unpack('II', data[3:])
            print 'decode dlen:%d, cmd:%d, logver:%d, logline:%d' % (dlen, cmd, logver, logline)
            return dlen, cmd, logver, logline
        elif cmd == CMD_GETDUMP:
            dumpver, size = struct.unpack('IQ', data[3:])
            print 'decode dlen:%d, cmd:%d, dumpver:%d, size:%d' % (dlen, cmd, dumpver, size) 
            return dlen, cmd, dumpver, size
        else:
            raise ValueError, 'cmd error:' + cmd

    
def main():
    try:
        size = int(sys.argv[1])
    except:
        size = 0
    
    print 'size:', size
    ss = SyncServer("127.0.0.1", 11005, size)
    ss.loop()

if __name__ == '__main__':
    main()





