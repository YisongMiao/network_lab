from timeout import timeout
import matplotlib.pyplot as plt
import numpy as np
from timeit import *


@timeout(10)
def timeit_dns(url):
    t1 = time.time()
    IP = socket.gethostbyname(url)
    t2 = time.time()

    return t2 - t1

# ping
@timeout(10)
def timeit_ping(url):
    IP = socket.gethostbyname(url)

    t1 = time.time()
    os.system('ping -c1 ' + IP + ' >> /dev/null')
    t2 = time.time()

    return t2 - t1

# establish connection
@timeout(10)
def timeit_conn(url):
    IP = socket.gethostbyname(url)

    s = socket.socket()
    t1 = time.time()
    s.connect((IP, 80))
    t2 = time.time()
    s.close()

    return t2 - t1

# page downloading
@timeout(10)
def timeit_curl(url):
    t1 = time.time()
    os.system('curl -s -o /dev/null ' + url)
    t2 = time.time()

    return t2 - t1

url = 'www.finishline.com'

print timeit_conn(url)
print timeit_dns(url)
print timeit_curl(url)