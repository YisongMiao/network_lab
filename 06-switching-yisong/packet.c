#include "packet.h"
#include "types.h"
#include "ether.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
 
extern ustack_t *instance;

void iface_send_packet(iface_info_t *iface, const char *packet, int len)
{
	struct sockaddr_ll addr;
	memset(&addr, 0, sizeof(struct sockaddr_ll));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = iface->index;
	addr.sll_halen = ETH_ALEN;
	addr.sll_protocol = htons(ETH_P_ARP);
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(addr.sll_addr, eh->ether_dhost, ETH_ALEN);

	if (sendto(iface->fd, packet, len, 0, (const struct sockaddr *)&addr,
				sizeof(struct sockaddr_ll)) < 0) {
 		perror("Send raw packet failed");
	}
}

//void broadcast_packet(iface_info_t *iface, const char *packet, int len)
//{
//	// TODO: implement the broadcast process here
//	fprintf(stdout, "TODO: implement the broadcast process here.\n");
//	fflush(stdout);
//}
void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	// TODO: implement the broadcast process here
	//fprintf(stdout, "TODO: implement the broadcast process here.\n");
	//printf("Input fd is:%d\n", iface->fd);
	iface_info_t *current_iface = NULL;
	list_for_each_entry(current_iface, &instance->iface_list, list) {
		//printf("Print each fd:%d\n", current_iface->fd);
		if(current_iface->fd != iface->fd){
			//printf("We are attempting to send packet to this fd:%d\n\n", current_iface->fd);
			iface_send_packet(current_iface, packet, len);
		}
	}
}
