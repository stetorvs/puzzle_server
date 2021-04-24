# Echo client program
import socket
import sys
import os

if len(sys.argv) != 3:
  print "Usage: python client.py <question name> <question answer>"
  sys.exit(0)

HOST = 'to-interactive2'  # The remote host
PORT = 3490               # The same port as used by the server
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
s.sendall(str(sys.argv[1]).lower() + ' ' + str(sys.argv[2]).lower() + ' ' + os.environ['USER'])
data = s.recv(1024)
s.close()
print 'Received:', data,
