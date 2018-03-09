#!/usr/bin/python

import matplotlib.pyplot as plt
import numpy as np

from timeit import *
from timeout import timeout

#################################

def plot_cdf(fname, label):
    dns_list = list()
    conn_list = list()
    curl_list = list()

    for line in open(fname, 'r'):
        dns_t = float(line.strip().split()[1])
        conn_t = float(line.strip().split()[2])
        curl_t = float(line.strip().split()[3])
        dns_list.append(dns_t)
        conn_list.append(conn_t)
        curl_list.append(curl_t)

    dns_list.sort()
    conn_list.sort()
    curl_list.sort()

    p = 1. * np.arange(len(dns_list))/(len(dns_list) - 1)

    #plt.plot(dns_list, p, label='DNS')
    #plt.plot(conn_list, p, label='CONN')
    #plt.plot(curl_list, p, label='CURL')
    plt.semilogx(dns_list, p, label='DNS')
    plt.semilogx(conn_list, p, label='CONN')
    plt.semilogx(curl_list, p, label='CURL')

    plt.legend(loc='best')
    plt.xlabel('time (log(s))')
    plt.ylabel('CDF')

    plt.show()
    plt.savefig('timeit.png')

def timeit_ping_all(fname='www_url.dat'):
    ofile = 'final_results.txt'

    #of = open(ofile, 'w')
    #for lineno, line in enumerate(open(fname, 'r')):
    #    #if lineno > 50: break
    #    try:
    #        print lineno
    #        #if lineno < 26: continue
    #        url = line.strip()
    #        start_time = time.time()
    #        end_time = time.time() + 10
    #        dns_t = timeit_dns(url)
    #        conn_t = timeit_conn(url)
    #        curl_t = timeit_curl(url)
    #        print url, dns_t, conn_t, curl_t, time.time() - start_time
    #        of.write('{} {} {} {}\n'.format(url, dns_t, conn_t, curl_t, time.time() - start_time))
    #    except:
    #        print 'error'
    #of.close()

    plot_cdf(ofile, 'ping')

# dns
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


if __name__ == '__main__':
    timeit_ping_all('www_url.dat')
