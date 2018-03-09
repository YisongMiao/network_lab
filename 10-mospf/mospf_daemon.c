#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *dump_db(void *param);
void *dump_nbr(void *param);
void *checking_db_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, dump, this_dump_nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&dump, NULL, dump_db, NULL);
	pthread_create(&this_dump_nbr, NULL, dump_nbr, NULL);
	pthread_create(&db, NULL, checking_db_thread, NULL);
}

void send_mospf_lsu(){
	int nbr_count = 0;
	iface_info_t * current_iface;
	mospf_nbr_t *nbr_entry = NULL, *nbr_q;
	list_for_each_entry(current_iface, &instance->iface_list, list){
		nbr_entry = NULL;
		if(current_iface->nbr_list.next != &current_iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(current_iface->nbr_list), list){
				nbr_count += 1;
			}
		}
	}

	char* sending_packet = malloc(ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE);
	struct ether_header * out_ether_hdr = (struct ether_header *)sending_packet;
	out_ether_hdr->ether_type = htons(ETH_P_IP);

	struct iphdr * out_ip_hdr = (struct iphdr *)(sending_packet + ETHER_HDR_SIZE);
	out_ip_hdr->version = 4;
	out_ip_hdr->ihl = 5;
	out_ip_hdr->tos = 0;
	out_ip_hdr->tot_len = htons(IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE);
	out_ip_hdr->id = rand();
	out_ip_hdr->frag_off = htons(IP_DF);
	out_ip_hdr->ttl = DEFAULT_TTL;
	out_ip_hdr->protocol = IPPROTO_MOSPF; // should be 90
	//out_ip_hdr->checksum is at the end
	//out_ip_hdr->saddr = htonl(iface->ip);
	//out_ip_hdr->daddr = 0;

	struct mospf_hdr * out_ospf_hdr = (struct mospf_hdr *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	out_ospf_hdr->version = MOSPF_VERSION;
	out_ospf_hdr->type = MOSPF_TYPE_LSU;
	out_ospf_hdr->len = htons(MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE);
	out_ospf_hdr->rid = htonl(instance->router_id);  //new edited
	out_ospf_hdr->aid = htonl(0);
	//out_ospf_hdr->checksum is at the end
	out_ospf_hdr->padding = htons(0);

	struct mospf_lsu * out_mospf_lsu = (struct mospf_lsu *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
	//instance->sequence_num += 1;
	//out_mospf_lsu->seq = htons(instance->sequence_num);
	out_mospf_lsu->ttl = MOSPF_MAX_LSU_TTL;  //should change
	out_mospf_lsu->unused = 0;  //not sure
	out_mospf_lsu->nadv = htonl(nbr_count);

	int nbr_index = 0;
	list_for_each_entry(current_iface, &instance->iface_list, list){
		nbr_entry = NULL;
		if(current_iface->nbr_list.next != &current_iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(current_iface->nbr_list), list){
				struct mospf_lsa * current_mospf_lsu = (struct mospf_lsa *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE);

				current_mospf_lsu->subnet = htonl(nbr_entry->nbr_ip & nbr_entry->nbr_mask);
				current_mospf_lsu->mask = htonl(nbr_entry->nbr_mask);
				current_mospf_lsu->rid = htonl(nbr_entry->nbr_id);  //new edited

				memcpy(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE, current_mospf_lsu, MOSPF_LSA_SIZE);
				nbr_index += 1;
			}
		}
	}

	list_for_each_entry(current_iface, &instance->iface_list, list){
		nbr_entry = NULL;
		if(current_iface->nbr_list.next != &current_iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(current_iface->nbr_list), list){
				memcpy(out_ether_hdr->ether_shost, current_iface->mac, ETH_ALEN);
				out_ip_hdr->saddr = htonl(current_iface->ip);
				out_ip_hdr->daddr = htonl(nbr_entry->nbr_ip);
				instance->sequence_num += 1;
				out_mospf_lsu->seq = htons(instance->sequence_num);
				out_ospf_hdr->checksum = mospf_checksum(out_ospf_hdr);
				out_ip_hdr->checksum = ip_checksum(out_ip_hdr);
				//iface_send_packet(iface, sending_packet, ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE);
				ip_send_packet(sending_packet, ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE);
			}
		}
	}
	//free(sending_packet);
}

void *sending_mospf_hello_thread(void *param)
{
	fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	iface_info_t *iface;
	static u8 bc_mac[ETH_ALEN] = { 1, 0, 94, 0, 0, 5 };
	while(1){
		list_for_each_entry(iface, &instance->iface_list, list) {
			char* sending_packet = malloc(ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE);
			//printf("Trying to send hello! Current iface: %s\n", iface->name);

			struct ether_header * out_ether_hdr = (struct ether_header *)sending_packet;
			memcpy(out_ether_hdr->ether_dhost, bc_mac, ETH_ALEN);
			memcpy(out_ether_hdr->ether_shost, iface->mac, ETH_ALEN);
			out_ether_hdr->ether_type = htons(ETH_P_IP);

			struct iphdr * out_ip_hdr = (struct iphdr *)(sending_packet + ETHER_HDR_SIZE);
			out_ip_hdr->version = 4;
			out_ip_hdr->ihl = 5;
			out_ip_hdr->tos = 0;
			out_ip_hdr->tot_len = htons(IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE);
			out_ip_hdr->id = rand();
			out_ip_hdr->frag_off = htons(IP_DF);
			out_ip_hdr->ttl = DEFAULT_TTL;
			out_ip_hdr->protocol = IPPROTO_MOSPF; // should be 90
			//out_ip_hdr->checksum is at the end
			out_ip_hdr->saddr = htonl(iface->ip);
			out_ip_hdr->daddr = htonl(MOSPF_ALLSPFRouters);

			struct mospf_hdr * out_ospf_hdr = (struct mospf_hdr *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
			out_ospf_hdr->version = MOSPF_VERSION;
			out_ospf_hdr->type = MOSPF_TYPE_HELLO;
			out_ospf_hdr->len = htons(MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE);
			out_ospf_hdr->rid = htonl(instance->router_id);  //new edited
			out_ospf_hdr->aid = htonl(0);
			//out_ospf_hdr->checksum is at the end
			out_ospf_hdr->padding = htons(0);

			struct mospf_hello * out_ospf_hello = (struct mospf_hello *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
			out_ospf_hello->mask = htonl(iface->mask);
			out_ospf_hello->helloint = htons(MOSPF_DEFAULT_HELLOINT);
			out_ospf_hello->padding = htons(0);

			//out_ospf_hdr->checksum = checksum((u16 *)out_ospf_hdr, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, 0);
			//out_ip_hdr->checksum = checksum((u16 *)out_ip_hdr, IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, 0);
			out_ospf_hdr->checksum = mospf_checksum(out_ospf_hdr);
			out_ip_hdr->checksum = ip_checksum(out_ip_hdr);

			iface_send_packet(iface, sending_packet, ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE);
		}
		sleep(MOSPF_DEFAULT_HELLOINT);
	}
	return NULL;
}

void *checking_nbr_thread(void *param)
{
	fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	iface_info_t *iface;
	while(1){
		list_for_each_entry(iface, &instance->iface_list, list){
			mospf_nbr_t *nbr_entry = NULL, *nbr_q;
			if(iface->nbr_list.next != &iface->nbr_list){  //means it is not empty
				list_for_each_entry_safe(nbr_entry, nbr_q, &(iface->nbr_list), list){
					time_t now;
					now = time(NULL) - 1500000000;
					//printf("now:%d, alive:%d\n", (u8)now, nbr_entry->alive);
					if(now - nbr_entry->alive > MOSPF_NEIGHBOR_TIMEOUT){
						list_delete_entry(&(nbr_entry->list));
						send_mospf_lsu();
						printf("Time out! cleaning neighbor\n");
					}
				}
			}
		}
		sleep(1);
	}
	return NULL;
}

void *checking_db_thread(void *param)
{
	while(1){
		mospf_db_entry_t *current_db_entry = NULL, *db_entry_q;
		list_for_each_entry_safe(current_db_entry, db_entry_q, &(mospf_db), list){
			time_t now = time(NULL) - 1500000000;
			if(now - current_db_entry->alive > MOSPF_DB_TIMEOUT){
				list_delete_entry(&(current_db_entry->list));
				printf("Time out! cleaning DB entry\n");
			}
		}
		sleep(1);
	}
	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	//fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct ether_header * in_ether_hdr = (struct ether_header *)packet;
	struct iphdr * in_ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr * in_ospf_hdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct mospf_hello * in_ospf_hello = (struct mospf_hello *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);

	if(in_ip_hdr->protocol == IPPROTO_MOSPF && in_ospf_hdr->type == MOSPF_TYPE_HELLO){
		//nbr_list is already initialized at mospf_init()
		//struct arp_req *req_entry = NULL, *req_q;
		int found = 0;
		mospf_nbr_t *nbr_entry = NULL, *nbr_q;
		if(iface->nbr_list.next != &iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(iface->nbr_list), list){
				if(ntohl(in_ip_hdr->saddr) == nbr_entry->nbr_ip){
					found = 1;
					time_t now;
					now = time(NULL) - 1500000000;
					nbr_entry->alive = now;
				}
			}
		}
		if (found == 0){
			mospf_nbr_t * new_nbr = malloc(sizeof(mospf_nbr_t));
			new_nbr->nbr_id = ntohl(in_ospf_hdr->rid);
			new_nbr->nbr_ip = ntohl(in_ip_hdr->saddr);
			new_nbr->nbr_mask = ntohl(in_ospf_hello->mask);
			time_t now;
			now = time(NULL) - 1500000000;
			new_nbr->alive = now;
			memcpy(new_nbr->mac, in_ether_hdr->ether_shost, ETH_ALEN);

			list_add_tail(&(new_nbr->list), &(iface->nbr_list));
			send_mospf_lsu();
		}
	}

}

void *sending_mospf_lsu_thread(void *param)
{
	fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	while(1){
		send_mospf_lsu();
		//sleep(MOSPF_DEFAULT_LSUINT);  // seems too long
		sleep(10);
	}
	return NULL;
}

void *dump_nbr(void *param)
{
	fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	iface_info_t *iface;
	while(1){
		printf("-------Dumping nbr-------\n");
		list_for_each_entry(iface, &instance->iface_list, list){
			mospf_nbr_t *nbr_entry = NULL, *nbr_q;
			if(iface->nbr_list.next != &iface->nbr_list){  //means it is not empty
				list_for_each_entry_safe(nbr_entry, nbr_q, &(iface->nbr_list), list){
					printf("Nbr for %d, rid: %d\n", instance->router_id, nbr_entry->nbr_id);
				}
			}
		}
		printf("---Nbr done.---\n");
		sleep(100);
	}
	return NULL;
}

void *dump_db(void *param){
	while(1){
		mospf_db_entry_t *current_db_entry = NULL, *db_entry_q;
		printf("------Dumping DB------\n");
		time_t now = time(NULL) - 1500000000;
		int now_time = (int)now;
		printf("now is :%d\n", now_time);
		int j = 1;
		list_for_each_entry_safe(current_db_entry, db_entry_q, &(mospf_db), list){
			printf("We are printing %dth db_entry\n", j);
			printf("rid: %d\n", current_db_entry->rid);
			printf("seq: %d\n", current_db_entry->seq);
			printf("nadv: %d\n", current_db_entry->nadv);
			int i;
			for(i = 0; i < current_db_entry->nadv; i++){
				printf("--\n");
				printf("    subnet:"IP_FMT" \n", HOST_IP_FMT_STR(current_db_entry->array[i].subnet));
				printf("    mask:"IP_FMT" \n", HOST_IP_FMT_STR(current_db_entry->array[i].mask));
				printf("    rid:%d\n", current_db_entry->array[i].rid);
			}
			j += 1;
		}
		printf("------Done Dumping DB------\n");
		sleep(5);
	}
	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	//fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct iphdr * in_ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr * in_ospf_hdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct mospf_lsu * in_mospf_lsu = (struct mospf_lsu *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);

	int found = 0;

	mospf_db_entry_t *current_db_entry = NULL, *db_entry_q;
	list_for_each_entry_safe(current_db_entry, db_entry_q, &(mospf_db), list){
		if(current_db_entry->rid == ntohl(in_ospf_hdr->rid)){
			found = 1;
			if(current_db_entry->seq < ntohs(in_mospf_lsu->seq)){  //bigger seq, update!
				//update lsa info
				printf("Update lsa info\n");
				current_db_entry->rid = ntohl(in_ospf_hdr->rid);
				current_db_entry->seq = ntohs(in_mospf_lsu->seq);
				current_db_entry->nadv = ntohl(in_mospf_lsu->nadv);
				time_t now = time(NULL) - 1500000000;
				current_db_entry->alive = now;
				int i;
				for(i = 0; i < current_db_entry->nadv; i++){
					struct mospf_lsa * traverse_lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + i * MOSPF_LSA_SIZE);
					current_db_entry->array[i].subnet = ntohl(traverse_lsa->subnet);
					current_db_entry->array[i].mask = ntohl(traverse_lsa->mask);
					current_db_entry->array[i].rid = ntohl(traverse_lsa->rid);
				}
			}
		}
	}

	if(found == 0){
		printf("Insert new lsa info\n");
		mospf_db_entry_t * new_db_entry = malloc(sizeof(mospf_db_entry_t));
		new_db_entry->rid = ntohl(in_ospf_hdr->rid);
		new_db_entry->seq = ntohs(in_mospf_lsu->seq);
		new_db_entry->nadv = ntohl(in_mospf_lsu->nadv);
		time_t now = time(NULL) - 1500000000;
		new_db_entry->alive = now;
		int i;
		for(i = 0; i < new_db_entry->nadv; i++){
			struct mospf_lsa * traverse_lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + i * MOSPF_LSA_SIZE);
			new_db_entry->array[i].subnet = ntohl(traverse_lsa->subnet);
			new_db_entry->array[i].mask = ntohl(traverse_lsa->mask);
			new_db_entry->array[i].rid = ntohl(traverse_lsa->rid);
		}
		list_add_tail(&(new_db_entry->list), &(mospf_db));
	}

	//then do the forward thing
	//printf("Tyring to forward, from %d\n", ntohl(in_ospf_hdr->rid));
	iface_info_t * current_iface;
	list_for_each_entry(current_iface, &instance->iface_list, list){
		mospf_nbr_t *nbr_entry = NULL, *nbr_q;
		if(current_iface->nbr_list.next != &current_iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(current_iface->nbr_list), list){
				//if(nbr_entry->nbr_id != ntohl(in_ospf_hdr->rid)){  //can forward
				if(nbr_entry->nbr_ip != ntohl(in_ip_hdr->saddr) && nbr_entry->nbr_id != ntohl(in_ospf_hdr->rid)){  //can forward
					char * sending_packet = malloc(len);
					memcpy(sending_packet, packet, len);
					struct ether_header * out_ether_hdr = (struct ether_header *)sending_packet;
					struct iphdr * out_ip_hdr = (struct iphdr *)(sending_packet + ETHER_HDR_SIZE);
					struct mospf_hdr * out_ospf_hdr = (struct mospf_hdr *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
					memcpy(out_ether_hdr->ether_shost, current_iface->mac, ETH_ALEN);
					out_ip_hdr->saddr = htonl(current_iface->ip);
					out_ip_hdr->daddr = htonl(nbr_entry->nbr_ip);
					out_ospf_hdr->checksum = mospf_checksum(out_ospf_hdr);
					out_ip_hdr->checksum = ip_checksum(out_ip_hdr);
					ip_send_packet(sending_packet, len);
				}
			}
		}
	}
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	//log(DEBUG, "**************received mospf packet, type: %d*****************", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
	//printf("******************************************\n");
}
