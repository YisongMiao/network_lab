#!/usr/bin/python

import sys

if __name__ == '__main__':
    fname = sys.argv[1]
    dic = dict()

    for line in open(fname):
        for c in filter(str.isalpha, line.lower()):
            dic[c] = dic.get(c, 0) + 1

    # print sys.argv[1]
    for i in range(ord('a'), ord('z')+1):
        print chr(i), dic[chr(i)]
