from socket import *
from struct import *
import time
import sys

def cmd(sock, data):
  for param in data:
    sock.send(param)

  header = sock.recv(4)
  length = unpack('I', header)[0]
  data = sock.recv(length)
  retcode = unpack('H', data[:2])[0]
  print 'length: %d, retcode: %d, ' % (length, retcode), repr(header + data)
  return (retcode, data)

def cmd_sync(sock, log_ver, log_index):
  data = [pack('H', 9),
          pack('B', 100),
          pack('II', log_ver, log_index)]
  (retcode, retdata) = cmd(sock, data)
  if retcode == 0:
      for a in range(32): 
          log_ver = unpack('I', sock.recv(4))[0]
          log_pos = unpack('I', sock.recv(4))[0]
          log_record = sock.recv(2)
          length = unpack('H', log_record)[0]
          log_record += sock.recv(length)
          print 'logver: %d, logpos: %d, length: %d' % \
                        (log_ver, log_pos, length + 2), repr(log_record);

def cmd_sync_dump(sock, dumpver, size):
  data = [pack('H', 13),
          pack('B', 101),
          pack('I', dumpver),
          pack('Q', size)]
  (retcode, retdata) = cmd(sock, data)
  dump_size = unpack('Q', retdata[-8:])[0]
  print "dump size: ", dump_size
  dump_data = sock.recv(dump_size)
  time.sleep(5)
  print repr(dump_data)
  print "======== receiving sync log ============"
  # receive sync logs
  while True:
      print repr(sock.recv(1))


  #time.sleep(300)

def test_cmd_sync(sock):
  #cmd_sync(sock, 2, 0)
  #cmd_sync(sock, 1, 0)

  # invalid log version
  cmd_sync(sock, 0, 0)

  # invalid log no
  cmd_sync(sock, 1, 1000000)
  # nonexistent index 
  cmd_sync(sock, 1, 4000)

  #cmd_sync(sock, 1, 0)
  #time.sleep(3000)
  #sys.exit(1)

  # nonexistent log version
  #cmd_sync(sock, 9, 4)

  #sys.exit(1)
  # valid
  #cmd_sync(sock, 1, 0)
  #cmd_sync(sock, 1, 0)
  #time.sleep(300)

def test_cmd_sync_dump(sock):
  # invalid dump version
  #cmd_sync_dump(sock, 0, 0)

  # nonexistent dump version
  #cmd_sync_dump(sock, 10, 0)

  # too big size
  # cmd_sync_dump(sock, 2, 100000)
  #sys.exit(1)

  cmd_sync_dump(sock, 2, 0) 

def main():
  sock = socket(AF_INET, SOCK_STREAM);
  sock.connect(('localhost', 11003));

  #test_cmd_sync(sock)
  test_cmd_sync_dump(sock)

  sock.close()

if __name__ == '__main__':
    main()
