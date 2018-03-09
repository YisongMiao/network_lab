#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"

#include "list.h"
#include "log.h"
#include "rtable.h"

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
void *deal_rtable(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, dump, this_dump_nbr, db, rt;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&dump, NULL, dump_db, NULL);
	pthread_create(&this_dump_nbr, NULL, dump_nbr, NULL);
	pthread_create(&db, NULL, checking_db_thread, NULL);
	pthread_create(&rt, NULL, deal_rtable, NULL);
}

void send_mospf_lsu(){
	int nbr_count = 0;
	iface_info_t * current_iface;
	mospf_nbr_t *nbr_entry = NULL, *nbr_q;
	list_for_each_entry(current_iface, &instance->iface_list, list){
		// nov29 added
		//printf("iface ip:%d\n", current_iface->ip);
		if ((current_iface->ip == LAN_IP1) || (current_iface->ip == LAN_IP2)){
			printf("Found a LAN interface!\n");
			nbr_count += 1;
		}
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
		//nov29 added
		if (current_iface->ip == LAN_IP1){
			struct mospf_lsa * current_mospf_lsu = (struct mospf_lsa *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE);
			current_mospf_lsu->subnet = htonl(current_iface->ip & current_iface->mask);
			current_mospf_lsu->mask = htonl(current_iface->mask);
			current_mospf_lsu->rid = htonl(101);
			nbr_index += 1;
		}
		if (current_iface->ip == LAN_IP2){
			struct mospf_lsa * current_mospf_lsu = (struct mospf_lsa *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE);
			current_mospf_lsu->subnet = htonl(current_iface->ip & current_iface->mask);
			current_mospf_lsu->mask = htonl(current_iface->mask);
			current_mospf_lsu->rid = htonl(102);
			nbr_index += 1;
		}
		nbr_entry = NULL;
		if(current_iface->nbr_list.next != &current_iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(current_iface->nbr_list), list){
				struct mospf_lsa * current_mospf_lsu = (struct mospf_lsa *)(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE);

				current_mospf_lsu->subnet = htonl(nbr_entry->nbr_ip & nbr_entry->nbr_mask);
				current_mospf_lsu->mask = htonl(nbr_entry->nbr_mask);
				current_mospf_lsu->rid = htonl(nbr_entry->nbr_id);  //new edited

				//memcpy(sending_packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_index * MOSPF_LSA_SIZE, current_mospf_lsu, MOSPF_LSA_SIZE);
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
			if(now - current_db_entry->alive > 10){
				list_delete_entry(&(current_db_entry->list));
				printf("Time out! cleaning DB entry\n");
			}
		}
		sleep(1);
	}
	return NULL;
}

int rid_to_index(int rid){
	if(rid == 167772417){
		return 1;
	}
	if(rid == 167772674){
		return 2;
	}
	if(rid == 167772931){
		return 3;
	}
	if(rid == 167773188){
		return 4;
	}
	if(rid == 101){
		return 0;
	}
	if(rid == 102){
		return 5;
	}
	else{
		exit(-1);
	}
}

int index_to_rid(int index){
	if(index == 1){
		return 167772417;
	}
	if(index == 2){
		return 167772674;
	}
	if(index == 3){
		return 167772931;
	}
	if(index == 4){
		return 167773188;
	}
	if(index == 0){
		return 101;
	}
	if(index == 5){
		return 102;
	}
	if(index == -1){
		return 0;
	}
	else{
		printf("%dNo!\n", index);
		exit(-1);
	}
}

void draw_graph(int input_graph[NUM_VERTICES][NUM_VERTICES]){
	mospf_db_entry_t *current_db_entry = NULL, *db_entry_q;
	int i;
	list_for_each_entry_safe(current_db_entry, db_entry_q, &(mospf_db), list){
		for(i = 0; i < current_db_entry->nadv; i++){
			input_graph[rid_to_index(current_db_entry->rid)][rid_to_index(current_db_entry->array[i].rid)] = 1;
		}
	}
	printf("Insert self neighbor info\n");
	iface_info_t * current_iface;
	mospf_nbr_t *nbr_entry = NULL, *nbr_q;
	list_for_each_entry(current_iface, &instance->iface_list, list){
		if (current_iface->ip == LAN_IP1){
			input_graph[rid_to_index(instance->router_id)][0] = 1;
		}
		if (current_iface->ip == LAN_IP2){
			input_graph[rid_to_index(instance->router_id)][5] = 1;
		}
		nbr_entry = NULL;
		if(current_iface->nbr_list.next != &current_iface->nbr_list){  //means it is not empty
			list_for_each_entry_safe(nbr_entry, nbr_q, &(current_iface->nbr_list), list){
				input_graph[rid_to_index(instance->router_id)][rid_to_index(nbr_entry->nbr_id)] = 1;
			}
		}
	}
}

void generate_subnet_graph(int input_graph[NUM_VERTICES][NUM_VERTICES]){
	mospf_db_entry_t *current_db_entry = NULL, *db_entry_q;
	int i;
	list_for_each_entry_safe(current_db_entry, db_entry_q, &(mospf_db), list){
		for(i = 0; i < current_db_entry->nadv; i++){
			input_graph[rid_to_index(current_db_entry->rid)][rid_to_index(current_db_entry->array[i].rid)] = current_db_entry->array[i].subnet;
		}
	}
}

void mirror_graph(int input_graph[NUM_VERTICES][NUM_VERTICES]){
	int i, j;
	for(i = 0; i < NUM_VERTICES; i++){
		for(j = 0; j < NUM_VERTICES; j++){
			if(input_graph[i][j] != 0){
				input_graph[j][i] = input_graph[i][j];
			}
		}
	}
}

void show_subnet_drawing(int input_graph[NUM_VERTICES][NUM_VERTICES]){
	int i;
	for(i = 0; i < NUM_VERTICES; i++){
		printf(""IP_FMT" ** "IP_FMT" ** "IP_FMT" ** "IP_FMT" ** "IP_FMT" ** "IP_FMT"\n", HOST_IP_FMT_STR(input_graph[i][0]), HOST_IP_FMT_STR(input_graph[i][1]), HOST_IP_FMT_STR(input_graph[i][2]), HOST_IP_FMT_STR(input_graph[i][3]), HOST_IP_FMT_STR(input_graph[i][4]), HOST_IP_FMT_STR(input_graph[i][5]));
	}
}

void show_drawing(int input_graph[NUM_VERTICES][NUM_VERTICES]){
	int i;
	for(i = 0; i < NUM_VERTICES; i++){
		printf("%d%d%d%d%d%d\n", input_graph[i][0], input_graph[i][1], input_graph[i][2], input_graph[i][3], input_graph[i][4], input_graph[i][5]);
	}
}

int minDistance(int dist[NUM_VERTICES], int sptSet[NUM_VERTICES]){
   // Initialize min value
   int min = INT_MAX, min_index;
   int v;
   for(v = 0; v < NUM_VERTICES; v++){
		if (sptSet[v] == 0 && dist[v] < min){
        	min = dist[v];
        	min_index = v;
		}
	}
	return min_index;
}

void dijkstra(int input_graph[NUM_VERTICES][NUM_VERTICES], int src, int dist[NUM_VERTICES], int prev[NUM_VERTICES], int purpose[NUM_VERTICES], int subnet_graph[NUM_VERTICES][NUM_VERTICES]){
	int sptSet[NUM_VERTICES];
	int i;
	for(i = 0; i < NUM_VERTICES; i++){
		dist[i] = INT_MAX;
		sptSet[i] = 0;
		prev[i] = -1; 
	} // do the initialization

	dist[src] = 0;

	int count;
	for(count = 0; count < NUM_VERTICES - 1; count++){
		int u = minDistance(dist, sptSet);
		sptSet[u] = 1;
		int v;
		for(v = 0; v < NUM_VERTICES; v++){
			if(sptSet[v] == 0 && input_graph[u][v] && dist[u] + input_graph[u][v] < dist[v]){
				dist[v] = dist[u] + input_graph[u][v];
				prev[v] = u;
				purpose[v] = subnet_graph[u][v];
			}
		}
	}
}

void print_dijkstra_dist(int dist[NUM_VERTICES]){
	printf("Printing Dijkstra dist result:\n");
	int i;
	for(i = 0; i < NUM_VERTICES; i++){
		printf("%d: %d\n", i, dist[i]);
	}
}

void print_dijkstra_prev(int prev[NUM_VERTICES]){
	printf("Printing Dijkstra prev result:\n");
	int i;
	for(i = 0; i < NUM_VERTICES; i++){
		printf("%d: %d\n", i, prev[i]);
	}
}

void print_dijkstra_purpose(int purpose[NUM_VERTICES]){
	printf("Printing Dijkstra prev result:\n");
	int i;
	for(i = 0; i < NUM_VERTICES; i++){
		printf("The purpose of %d is: "IP_FMT" \n", i, HOST_IP_FMT_STR(purpose[i]));
	}
}

int find_desired_index(int prev[NUM_VERTICES], int dest_index){  // actually a backsource
	int current_index = dest_index;
	while(prev[prev[current_index]] != -1){
		current_index = prev[current_index];
	}
	return current_index;
}

u32 find_desired_subnet(int input_rid){
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		mospf_db_entry_t *current_db_entry = NULL, *db_entry_q;
		list_for_each_entry_safe(current_db_entry, db_entry_q, &(mospf_db), list){
			if(current_db_entry->rid == input_rid){
				int i;
				for(i = 0; i < current_db_entry->nadv; i++){
					if((current_db_entry->array[i].subnet & current_db_entry->array[i].mask) ==
						(iface->ip & iface->mask)){
						return (iface->ip & iface->mask);
					}
				}
			}
		}
	}
	return 0;
}

void *deal_rtable(void *param)
{
	while(1){
		//print_rtable();
		int subnet_graph[NUM_VERTICES][NUM_VERTICES];
		generate_subnet_graph(subnet_graph);
		mirror_graph(subnet_graph);
		//show_subnet_drawing(subnet_graph);

		int graph[NUM_VERTICES][NUM_VERTICES];
		int dist[NUM_VERTICES];
		int prev[NUM_VERTICES];
		int purpose[NUM_VERTICES];
		draw_graph(graph);
		mirror_graph(graph);
		//show_drawing(graph);
		int src = rid_to_index(instance->router_id);
		dijkstra(graph, src, dist, prev, purpose, subnet_graph);
		//print_dijkstra_dist(dist);
		//print_dijkstra_prev(prev);
		//print_dijkstra_purpose(purpose);
		//int desired_index = find_desired_index(prev, 5);
		//printf("Desired index: %d\n", desired_index);
		//u32 desired_subnet = find_desired_subnet(index_to_rid(desired_index));
		//printf("Desired subnet:"IP_FMT" \n", HOST_IP_FMT_STR(desired_subnet));

		int i;
		for(i = 0; i < NUM_VERTICES; i++){
			iface_info_t * desired_iface = NULL;
			int desired_gw = 0;
			int desired_mask;
			char desired_if_name[16];
			int current_subnet = purpose[i];
			printf("i: %d\n", i);
			printf("current subnet:"IP_FMT" \n", HOST_IP_FMT_STR(current_subnet));
			if(current_subnet != 0){
				int desired_index = find_desired_index(prev, i);
				int communicating_subnet = find_desired_subnet(index_to_rid(desired_index));
				printf("communicating_subnet:"IP_FMT" \n", HOST_IP_FMT_STR(communicating_subnet));

				iface_info_t * current_iface;
				list_for_each_entry(current_iface, &instance->iface_list, list){
					if((current_iface->ip & current_iface->mask) == communicating_subnet){
						desired_iface = current_iface;
					}
				}
				if(desired_iface){
					desired_mask = desired_iface->mask;
					strcpy(desired_if_name, desired_iface->name);
					printf("desired_iface: %s\n", desired_if_name);
					mospf_nbr_t *nbr_entry = NULL, *nbr_q;
					list_for_each_entry_safe(nbr_entry, nbr_q, &(desired_iface->nbr_list), list){
						if((nbr_entry->nbr_ip & nbr_entry->nbr_mask) == communicating_subnet){
							desired_gw = nbr_entry->nbr_ip;
						}
					}
				}
			}
			int found = 0;
			rt_entry_t *entry = NULL;
			list_for_each_entry(entry, &rtable, list){
				if(entry->dest == current_subnet){ // refresh the content of the entry
					printf("Refresh the content of current entry\n");
					found = 1;
					entry->gw = desired_gw;
					entry->mask = desired_mask;
					strcpy(entry->if_name, desired_if_name);
					entry->iface = desired_iface;
				}
			}
			if(found == 0){
				printf("Need to add a new entry\n");
				rt_entry_t * new_rt_entry = malloc(sizeof(rt_entry_t));
				new_rt_entry->dest = current_subnet;
				new_rt_entry->gw = desired_gw;
				new_rt_entry->mask = desired_mask;
				strcpy(new_rt_entry->if_name, desired_if_name);
				new_rt_entry->iface = desired_iface;

				list_add_tail(&new_rt_entry->list, &rtable);
			}
		}
		printf("This round ends!\n");
		print_rtable();
		sleep(4);
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
		sleep(5);
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
		sleep(40);
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
