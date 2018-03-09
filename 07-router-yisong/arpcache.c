#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "packet.h"
#include "icmp.h"
#include "ip.h"
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{	
	//pthread_mutex_lock(&arpcache.lock);
	fprintf(stderr, "TODO: lookup ip address in arp cache.\n");
	int i;
	for(i = 0; i < MAX_ARP_SIZE; i++){
		//printf("Current Looking up ip("IP_FMT")\n", HOST_IP_FMT_STR(arpcache.entries[i].ip4));
		if(ip4 == arpcache.entries[i].ip4){
			//assign mac here
			memcpy(mac, arpcache.entries[i].mac, ETH_ALEN);
			pthread_mutex_unlock(&arpcache.lock);
			return 1;
		}
	}
	//pthread_mutex_unlock(&arpcache.lock);
	return 0;
}

void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	pthread_mutex_lock(&arpcache.lock);
	fprintf(stderr, "TODO: append the ip address if lookup failed, and send arp request if necessary.\n");
	struct arp_req *req_entry = NULL, *req_q;
	int found_req_entry = 0;
	if(arpcache.req_list.next == &arpcache.req_list){
		printf("The req_list is empty!\n");
	}
	if(arpcache.req_list.next != &arpcache.req_list){ //the req_list is not empty!
		list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list){
			printf("The req_list is not empty!\n");
			if(req_entry->ip4 == ip4){
				found_req_entry = 1;
				//do the append work
				//prepare the cached_pkt struct
				struct cached_pkt *new_cached_pkt = malloc(SIZE_CACHED_PKT);
				new_cached_pkt->packet = packet;
				//then we append the packet to cached_packets
				list_add_tail(&new_cached_pkt->list, &req_entry->cached_packets);
				//maybe need to add 1 to retries?
				//req_entry->retries += 1;
				printf("Not at head, We append a packet to this ip:"IP_FMT" \n", HOST_IP_FMT_STR(req_entry->ip4));
			}
		}
	}
	printf("Hey!\n");
	if(found_req_entry == 0){ 
		//then we should create a new req_entry first
		printf("Beter\n");
		struct arp_req *new_req_entry = malloc(SIZE_ARP_REQ);
		new_req_entry->iface = iface;
		new_req_entry->ip4 = ip4;
		new_req_entry->sent = time(NULL);
		new_req_entry->retries = 0;
		//prepare the cached_pkt struct
		struct cached_pkt *new_cached_pkt = malloc(SIZE_CACHED_PKT);
		new_cached_pkt->len = len;
		new_cached_pkt->packet = packet;
		//then we append the packet to cached_packets
		init_list_head(&new_req_entry->cached_packets);
		list_add_tail(&new_cached_pkt->list, &new_req_entry->cached_packets);
		//finally we append this arp_req to the req_list
		list_add_tail(&new_req_entry->list, &(arpcache.req_list));
		printf("At Head, We append a packet to this ip:"IP_FMT" \n", HOST_IP_FMT_STR(new_req_entry->ip4));
		arp_send_request(iface, ip4);
		//we only send arp request under this circumstance
	}
	pthread_mutex_unlock(&arpcache.lock);
}

void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	pthread_mutex_lock(&arpcache.lock);
	fprintf(stderr, "TODO: insert ip->mac entry, and send all the pending packets.\n");

	//we should find an available position to insert.
	//not hash, lol...
	int i = 0;
	int position_index = -1;
	while(i < MAX_ARP_SIZE){
		if(arpcache.entries[i].ip4 == 0){
			position_index = i;
			printf("We insert this ip->mac entry at position %d\n", position_index);
			break;
		}
		i += 1;
	}

	//then insert the information
	arpcache.entries[position_index].ip4 = ntohl(ip4);
	memcpy(arpcache.entries[position_index].mac, mac, ETH_ALEN);
	arpcache.entries[position_index].added = time(NULL);
	printf("We Insert ip:"IP_FMT" \n", HOST_IP_FMT_STR(arpcache.entries[position_index].ip4));
	//*** what is valid? ***/

	//then we send all pending packets.
	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list){
		if(req_entry->ip4 == ntohl(ip4)){ //we found the pending entry!
			struct cached_pkt *pkt_entry = NULL, *pkt_q;
			list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list){
				struct ether_header *eh = (struct ether_header *)(pkt_entry->packet);
				memcpy(eh->ether_dhost, mac, ETH_ALEN); // Assign the mac addr!
				iface_send_packet(req_entry->iface, pkt_entry->packet, pkt_entry->len);
				printf("We sent a pending packet!\n");
				printf("Mac Addr:" ETHER_STRING "\n", ETHER_FMT(mac));
				u32 dst = ntohl(ip4);
				printf("IP:"IP_FMT" \n", HOST_IP_FMT_STR(dst));
			}
			//list_delete_entry(&(req_entry->list));
			//free(req_entry);
		}
	}

	pthread_mutex_unlock(&arpcache.lock);
}

void *arpcache_sweep(void *arg) 
{
	while (1) {
		sleep(1);
		fprintf(stderr, "TODO: sweep arpcache periodically: remove old entries, resend arp requests .\n");

		pthread_mutex_lock(&arpcache.lock);

		struct arp_req *req_entry = NULL, *req_q;
		list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
			if(req_entry->retries > 5){
				//It is very important!!!!!!
				pthread_mutex_unlock(&arpcache.lock);
				struct cached_pkt *pkt_entry = NULL, *pkt_q;
				list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list){
					icmp_send_packet(pkt_entry->packet, pkt_entry->len, 3, 1);
					printf("Sending ARP Unreachable\n");
					//list_delete_entry(&(pkt_entry->list));
				}
				//free(pkt_entry->packet);
				//list_delete_entry(&(req_entry->list));
				//free(req_entry);
			}

			time_t now = time(NULL);
			if(now - req_entry->sent > 1){
				int i;
				int found = 0;
				for(i = 0; i < MAX_ARP_SIZE; i++){
					if(arpcache.entries[i].ip4 == req_entry->ip4){
						found = 1;
					}
				}
				if(found == 0){
					arp_send_request(req_entry->iface, req_entry->ip4);
					req_entry->retries += 1;
					printf("We're Retrying ARP send IP:"IP_FMT" \n", HOST_IP_FMT_STR(req_entry->ip4));
					printf("The retries is: %d\n", req_entry->retries);
				}
			}
		}
		int i;
		for(i = 0; i < MAX_ARP_SIZE; i++){
			time_t now = time(NULL);
			if((now - arpcache.entries[i].added) > 6 && arpcache.entries[i].valid == 1){
				free(&arpcache.entries[i]);
				printf("Hey, an old entry is here!\n");
			}
		}
		pthread_mutex_unlock(&arpcache.lock);
	}

	return NULL;
}
