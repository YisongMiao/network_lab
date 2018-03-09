#!/usr/bin/python

import socket
import time
import os

#################################

# dns
def timeit_dns(url):
    t1 = time.time()
    IP = socket.gethostbyname(url)
    t2 = time.time()
    
    return t2 - t1

# ping
def timeit_ping(url):
    IP = socket.gethostbyname(url)

    t1 = time.time()
    os.system('ping -c1 ' + IP + ' >> /dev/null')
    t2 = time.time()

    return t2 - t1

# establish connection
def timeit_conn(url):
    IP = socket.gethostbyname(url)

    s = socket.socket()
    t1 = time.time()
    s.connect((IP, 80))
    t2 = time.time()
    s.close()

    return t2 - t1

# page downloading
def timeit_curl(url):
    t1 = time.time()
    os.system('curl -s -o /dev/null ' + url)
    t2 = time.time()

    return t2 - t1


if __name__ == '__main__':
    url = 'www.baidu.com'
    print 'dns:', timeit_dns(url)
    print 'ping:', timeit_ping(url)
    print 'conn:', timeit_conn(url)
    print 'all:', timeit_curl(url)
