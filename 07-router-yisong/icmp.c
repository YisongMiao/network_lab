#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"
#include "packet.h"
#include "string.h"

#include <stdio.h>
#include <stdlib.h>

//void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
//{
//	ip->version = 4;
//	ip->ihl = 5;
//	ip->tos = 0;
//	ip->tot_len = htons(len);
//	ip->id = rand();
//	ip->frag_off = htons(IP_DF);
//	ip->ttl = DEFAULT_TTL;
//	ip->protocol = proto;
//	ip->saddr = htonl(saddr);
//	ip->daddr = htonl(daddr);
//	ip->checksum = ip_checksum(ip);
//}

void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	//use ip_send_packet
	fprintf(stderr, "TODO: malloc and send icmp packet.\n");

	struct ether_header *empty_ether_hdr = malloc(ETHER_HDR_SIZE);

	struct iphdr *original_ip_hdr = packet_to_ip_hdr(in_pkt);
	struct iphdr *current_ip_hdr = malloc(IP_BASE_HDR_SIZE);

	ip_init_hdr(current_ip_hdr, ntohl(original_ip_hdr->daddr), ntohl(original_ip_hdr->saddr), (98 - 14), 1);
	//proto is 1

	if(type == 0){//we are dealing with a PING
		printf("We receive a self-echo PING! The Total Length is %d\n", ntohs(original_ip_hdr->tot_len));

		char* sending_packet = malloc(98);
		memcpy(sending_packet, empty_ether_hdr, ETHER_HDR_SIZE);
		memcpy(sending_packet + ETHER_HDR_SIZE, current_ip_hdr, IP_HDR_SIZE(current_ip_hdr));

		struct icmphdr *out_icmp = (struct imcphdr *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
		out_icmp -> type = type;
		out_icmp -> code = code;
		//out_icmp -> icmp_identifier = htons(0x0);
		//out_icmp -> icmp_sequence = htons(0x0);

		memcpy(sending_packet + 14 + 20 + 4, in_pkt + 14 + 20 + 4, 98 - 14 - 20 - 4);
		out_icmp->checksum = (icmp_checksum(out_icmp, (98 - 14 - 20)));

		printf("Current checksum is: 0x%04hx \n", out_icmp->checksum);
		printf("Current Type is: %d\n", out_icmp->type);

		printf("###The Input type is:%d\n", type);
		ip_send_packet(sending_packet, len);
	}

	if(type != 0){
		printf("We are dealing with a NON-ECHO!\n");
		char* sending_packet = malloc(98);
		memcpy(sending_packet, empty_ether_hdr, ETHER_HDR_SIZE);
		memcpy(sending_packet + ETHER_HDR_SIZE, current_ip_hdr, IP_HDR_SIZE(current_ip_hdr));

		struct icmphdr *out_icmp = (struct imcphdr *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
		out_icmp -> type = type;
		out_icmp -> code = code;
		out_icmp -> icmp_identifier = htons(0x0);
		out_icmp -> icmp_sequence = htons(0x0);

		memcpy(sending_packet + 14 + 20 + 8, in_pkt + 14, 28);
		out_icmp->checksum = (icmp_checksum(out_icmp, (98 - 14 - 20)));

		printf("Current checksum is: 0x%04hx \n", out_icmp->checksum);
		printf("Current Type is: %d\n", out_icmp->type);

		printf("###The Input type is:%d\n", type);
		printf("Trying to send!\n");
		ip_send_packet(sending_packet, len);
		printf("Sent Successful!!\n");
	}
}
