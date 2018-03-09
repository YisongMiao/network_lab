#include "ip.h"
#include "icmp.h"
#include "packet.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"

#include "log.h"

#include <stdlib.h>

void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = htons(len);
	ip->id = rand();
	ip->frag_off = htons(IP_DF);
	ip->ttl = DEFAULT_TTL;
	ip->protocol = proto;
	ip->saddr = htonl(saddr);
	ip->daddr = htonl(daddr);
	ip->checksum = ip_checksum(ip);
}

// the input address is in host byte order
rt_entry_t *longest_prefix_match(u32 dst)
{
	//fprintf(stderr, "TODO: longest prefix match for the packet.\n");
	rt_entry_t *entry = NULL;
	rt_entry_t *desired_entry = NULL;
	u32 net;
	list_for_each_entry(entry, &rtable, list){
		net = entry->dest & entry->mask;
		if((dst & entry->mask) == net){
			if(desired_entry == NULL){
				desired_entry = entry;
			}
			else{
				if(entry->mask > desired_entry->mask){
					//We brutally think that:
					//the longer the mask, the greater it is.
					desired_entry = entry;
				}
			}
		}
	}
	return desired_entry;
	//return NULL;
}

void ip_forward_packet(u32 ip_dst, char *packet, int len)
{
	//fprintf(stderr, "TODO: forward ip packet.\n");
	struct iphdr *ip = packet_to_ip_hdr(packet);

	//so in the following 2 cases, the packet does not change.
	ip->ttl -= 1;
	if(!ip->ttl > 0){
		fprintf(stderr, "TODO: ICMP.\n");
		icmp_send_packet(packet, len, 11, 0);
	}
	ip->checksum = ip_checksum(ip);

	u32 dst = ntohl(ip->daddr);
	rt_entry_t *entry = longest_prefix_match(dst);
	if (!entry) {
		log(ERROR, "Could not find forwarding rule for IP (dst:"IP_FMT") packet.", 
				HOST_IP_FMT_STR(dst));
		icmp_send_packet(packet, len, 3, 0);
		free(packet);
		return ;
	}
	//printf("The longest prefix found ip:"IP_FMT" \n", HOST_IP_FMT_STR(entry->dest));
	u32 next_hop = entry->gw;
	if (!next_hop)
		next_hop = dst;
	//printf("The longest prefix found gw:"IP_FMT" \n", HOST_IP_FMT_STR(dst));
	struct ether_header *eh = (struct ether_header *)packet;
	eh->ether_type = ntohs(ETH_P_IP);
	memcpy(eh->ether_shost, entry->iface->mac, ETH_ALEN);
	iface_send_packet_by_arp(entry->iface, next_hop, packet, len);
}

void handle_ip_packet(iface_info_t *iface, char *packet, int len)//referenced in main.c
{
	//fprintf(stderr, "TODO: handle ip packet: echo the ping packet, and forward other IP packets.\n");
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 dst = ntohl(ip->daddr);
	struct icmphdr *icmp = packet_to_ICMP_header(packet);
	u8 icmp_type = icmp->type;
	if(dst == iface->ip && icmp_type == 8){
		printf("The Length of Current Packet is: %d\n", len);
		printf("We found a like-ICMP packet, its type is: %d\n", icmp_type);
		//then we echo it back!
		icmp_send_packet(packet, len, 0, 0);
	}
	else{
		ip_forward_packet(dst, packet, len);
	}
}

void ip_send_packet(char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 dst = ntohl(ip->daddr);
	rt_entry_t *entry = longest_prefix_match(dst);
	if (!entry) {
		log(ERROR, "Could not find forwarding rule for IP (dst:"IP_FMT") packet.", 
				HOST_IP_FMT_STR(dst));
		free(packet);
		return ;
	}

	u32 next_hop = entry->gw;
	if (!next_hop)
		next_hop = dst;

	struct ether_header *eh = (struct ether_header *)packet;
	eh->ether_type = ntohs(ETH_P_IP);
	memcpy(eh->ether_shost, entry->iface->mac, ETH_ALEN);

	printf("IP Send dst:"IP_FMT" \n", HOST_IP_FMT_STR(dst));
	printf("next_hop dst:"IP_FMT" \n", HOST_IP_FMT_STR(next_hop));
	printf("iface IP:"IP_FMT" \n", HOST_IP_FMT_STR(entry->iface->ip));

	iface_send_packet_by_arp(entry->iface, next_hop, packet, len);
	printf("Send Successfully here!\n");
}
