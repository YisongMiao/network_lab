#!/usr/bin/python

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.cli import CLI

class RouterTopo(Topo):
    "Topology connected by routers"
    def build(self):
        h1 = self.addHost('h1')
        h2 = self.addHost('h2')
        r1 = self.addHost('r1')

        self.addLink(h1, r1, bw=10, delay='10ms')
        self.addLink(h2, r1)


topo = RouterTopo()
net = Mininet(topo = topo, link = TCLink)

net.start()
CLI(net)
net.stop()
