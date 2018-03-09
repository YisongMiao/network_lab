#include "arp.h"
#include "base.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "arpcache.h"
#include "ip.h"
#include "math.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	//fprintf(stderr, "TODO: send arp request when lookup failed in arpcache.\n");
	//generate the ether_head string
	u8 broadcast_dmac[ETH_ALEN];
	int i;
	for(i = 0; i < ETH_ALEN; i++){
		broadcast_dmac[i] = 0xff;
	}
	ether_header_t *ether_info = malloc(ETHER_HDR_SIZE);
	memcpy(ether_info->ether_shost, iface->mac, ETH_ALEN);
	memcpy(ether_info->ether_dhost, broadcast_dmac, ETH_ALEN);
	ether_info->ether_type = htons(ETH_P_ARP);

	//generate the arp string
	ether_arp_t *arp_info = malloc(ARP_HDR_SIZE);
	arp_info->arp_hrd = htons(0x01);
	arp_info->arp_pro = htons(0x0800);
	arp_info->arp_hln = 6;
	arp_info->arp_pln = 4;
	arp_info->arp_op = htons(ARPOP_REQUEST);
	memcpy(arp_info->arp_sha, iface->mac, ETH_ALEN);
	arp_info->arp_spa = htonl(iface->ip);
	arp_info->arp_tpa = htonl(dst_ip);

	//concatenate two string together;
	char *sending_packet = malloc(ETHER_HDR_SIZE + ARP_HDR_SIZE);
	memcpy(sending_packet, ether_info, ETHER_HDR_SIZE);
	memcpy(sending_packet + ETHER_HDR_SIZE, arp_info, ARP_HDR_SIZE);

	int len;
	len = (int) (ETHER_HDR_SIZE + ARP_HDR_SIZE) / sizeof(char);
	iface_send_packet(iface, sending_packet, len);
}

void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	//generate the ether_head string
	ether_header_t *ether_info = malloc(sizeof(ether_header_t));
	memcpy(ether_info->ether_shost, iface->mac, ETH_ALEN);
	memcpy(ether_info->ether_dhost, req_hdr->arp_tha, ETH_ALEN);
	ether_info->ether_type = htons(ETH_P_ARP);

	//concatenate two struct together
	char *sending_packet = malloc(ETHER_HDR_SIZE + ARP_HDR_SIZE);
	memcpy(sending_packet, ether_info, ETHER_HDR_SIZE);
	memcpy(sending_packet + ETHER_HDR_SIZE, req_hdr, ARP_HDR_SIZE);

	//send the sending_packet
	int len;
	len = (int) (ETHER_HDR_SIZE + ARP_HDR_SIZE) / sizeof(char);
	iface_send_packet(iface, sending_packet, len);
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	//fprintf(stderr, "TODO: process arp packet: arp request & arp reply.\n");
	struct ether_arp *arp_info = packet_to_ether_arp(packet);
	//u32 dst = ntohl(arp_info->arp_tpa);
	u32 source = ntohl(arp_info->arp_spa);

	if(ntohs(arp_info->arp_op) == ARPOP_REPLY){
		printf("Hey Jude!!!!!!!!!\n");
		arpcache_insert(arp_info->arp_spa, arp_info->arp_sha);
	}
	if(ntohs(arp_info->arp_op) == ARPOP_REQUEST){
		//create a new ether_arp struct for the reply function
		//prepare it
		ether_arp_t *new_arp_info = malloc(sizeof(ether_arp_t));
		new_arp_info->arp_hrd = htons(0x01);
		new_arp_info->arp_pro = htons(0x0800);
		new_arp_info->arp_hln = 6;
		new_arp_info->arp_pln = 4;
		new_arp_info->arp_op = htons(ARPOP_REPLY);
		memcpy(new_arp_info->arp_sha, iface->mac, ETH_ALEN);
		new_arp_info->arp_spa = htonl(iface->ip);
		memcpy(new_arp_info->arp_tha, arp_info->arp_sha, ETH_ALEN);
		new_arp_info->arp_tpa = htonl(source);
		//then do the work!
		arp_send_reply(iface, new_arp_info);
	}
}

// This function should free the memory of the packet if needed.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	//printf("Send by arp: iface ip:"IP_FMT" \n", HOST_IP_FMT_STR(iface->ip));
	//printf("Send by arp: dst_ip:"IP_FMT" \n", HOST_IP_FMT_STR(dst_ip));
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		printf("*******Found! in arpcache!ip**********: ("IP_FMT") \n", HOST_IP_FMT_STR(dst_ip));
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		printf("Ether dst Mac Addr:" ETHER_STRING "\n", ETHER_FMT(eh->ether_dhost));
		printf("Ether source Mac Addr:" ETHER_STRING "\n", ETHER_FMT(eh->ether_shost));
		printf("ETHER OP Type: 0x%04hx \n", ntohs(eh->ether_type));
		printf("Sending: Current checksum is: 0x%04hx \n", ((struct icmphdr *)packet + 14 + 20)->checksum);
		printf("Sending: Current Type is: %d\n", ((struct icmphdr *)packet + 14 + 20)->type);
		iface_send_packet(iface, packet, len);
		free(packet);
	}
	else {
		printf("Havent Found!!\n");
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
