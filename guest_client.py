# Puzzle client helper program

import socket
import sys
import os
import re

if len(sys.argv) != 4:
  print "Usage: python guest_client.py <question name> <question answer> <email>"
  sys.exit(0)

HOST = 'localhost'  # The remote host
PORT = 3490         # The same port as used by the server

msg = str(sys.argv[1]).lower() + ' ' + str(sys.argv[2]).lower() + ' guest ' + str(sys.argv[3])
if len(msg) > 512:
  print 'Submission too long, ignoring'
elif not re.match('^[a-zA-Z0-9]+$', sys.argv[2]):
  print 'Please remove all non alpha-numerical characters from your answer'
else:
  print 'Connecting to server: ', HOST
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  s.connect((HOST, PORT))
  s.sendall(msg)
  data = s.recv(1024)
  s.close()
  print 'Received:', data,
