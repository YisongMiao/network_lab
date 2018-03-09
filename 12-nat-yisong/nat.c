#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	//fprintf(stdout, "TODO: determine the direction of this packet.\n");
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 ip_saddr = ntohl(ip->saddr);
	u32 ip_daddr = ntohl(ip->daddr);
	if(longest_prefix_match(ip_saddr)->iface->ip == nat.internal_iface->ip){
		if(longest_prefix_match(ip_daddr)->iface->ip == nat.external_iface->ip){
			return DIR_OUT;
		}
	}
	if(longest_prefix_match(ip_saddr)->iface->ip == nat.external_iface->ip){
		if(ip_daddr == nat.external_iface->ip){
			return DIR_IN;
		}
	}
	return DIR_INVALID;
}

int assign_external_port(){
	int r;
	int can = 0;
	while(can == 0){
		r = rand() % 65536;
		if(nat.assigned_ports[r] == 0){
			can = 1;
		}
	}
	return r;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	fprintf(stdout, "TODO: do translation for this packet.\n");
	printf("Dir is: %d\n", dir);
	struct iphdr * current_iphdr = packet_to_ip_hdr(packet);
	struct tcphdr * current_tcphdr = packet_to_tcp_hdr(packet);
	u16 sport = ntohs(current_tcphdr->sport);
	u32 saddr = ntohl(current_iphdr->saddr);
	u16 dport = ntohs(current_tcphdr->dport);
	u32 daddr = ntohl(current_iphdr->daddr);
	printf("Current flag is: 0x%08x\n", current_tcphdr->flags);

	if(dir == 2){
		pthread_mutex_lock(&nat.lock);
		char sport_str[8];
		sprintf(sport_str, "%d", sport);
		int sport_hash_result = hash8(sport_str, (int)strlen(sport_str));

		int found = 0;
		struct nat_mapping * current_nat_mapping;
		list_for_each_entry(current_nat_mapping, &nat.nat_mapping_list[sport_hash_result], list){
			if(current_nat_mapping){
				if(current_nat_mapping->internal_ip == saddr){
					if(current_nat_mapping->internal_port == sport){
						found = 1;
					}
				}
			}
		}

		if(found == 0){
			printf("Not found in nat_table, need to append to table\n");
			int assigning_port = assign_external_port();
			char assign_str[8];
			sprintf(assign_str, "%d", assigning_port);
			int assign_hash_result = hash8(assign_str, (int)strlen(assign_str));

			struct nat_mapping * new_nat_mapping = malloc(sizeof(struct nat_mapping));
			new_nat_mapping->internal_ip = saddr;
			new_nat_mapping->internal_port = sport;
			new_nat_mapping->external_ip = nat.external_iface->ip;
			new_nat_mapping->external_port = assigning_port;
			nat.assigned_ports[new_nat_mapping->external_port] = 1;

			time_t now = time(NULL);
			new_nat_mapping->update_time = now;

			struct nat_mapping * new_nat_mapping_2 = malloc(sizeof(struct nat_mapping));
			new_nat_mapping_2->internal_ip = saddr;
			new_nat_mapping_2->internal_port = sport;
			new_nat_mapping_2->external_ip = nat.external_iface->ip;
			new_nat_mapping_2->external_port = assigning_port;
			nat.assigned_ports[new_nat_mapping_2->external_port] = 1;

			new_nat_mapping_2->update_time = now;

			list_add_tail(&new_nat_mapping->list, &nat.nat_mapping_list[sport_hash_result]);
			list_add_tail(&new_nat_mapping_2->list, &nat.nat_mapping_list[assign_hash_result]);
			printf("sport_hash_result: %d\n", sport_hash_result);
			printf("assign_hash_result: %d\n", assign_hash_result);
		}

		list_for_each_entry(current_nat_mapping, &nat.nat_mapping_list[sport_hash_result], list){
			//printf("Here is an entry!\n");
			if(current_nat_mapping){
				if(current_nat_mapping->internal_ip == saddr){
					if(current_nat_mapping->internal_port == sport){
						//printf("Assigned port is:%d\n", current_nat_mapping->external_port);
						current_iphdr->saddr = htonl(nat.external_iface->ip);
						current_tcphdr->sport = htons(current_nat_mapping->external_port);
						current_tcphdr->checksum = tcp_checksum(current_iphdr, current_tcphdr);
						current_iphdr->checksum = ip_checksum(current_iphdr);
						ip_send_packet(packet, len);
						//printf("Found and Send!\n");

						time_t now = time(NULL);
						current_nat_mapping->update_time = now;

						if(current_tcphdr->flags == TCP_FIN + TCP_ACK){
							printf("It is a FIN + ACK!\n");
							current_nat_mapping->conn.internal_fin = 1;
						}

						if(current_tcphdr->flags == TCP_RST){
							printf("It is a RST!\n");
							if(current_nat_mapping->conn.internal_fin + current_nat_mapping->conn.external_fin == 2){
								nat.assigned_ports[current_nat_mapping->external_port] = 0;
								printf("both FIN, so recall the assigned external port\n");
								list_delete_entry(&current_nat_mapping->list);
							}
						}
					}
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
	}

	if(dir == 1){
		pthread_mutex_lock(&nat.lock);
		char str[8];
		//printf("sport is: %d\n", sport);
		printf("dport is: %d\n", dport);
		sprintf(str, "%d", dport);
		int hash_result = hash8(str, (int)strlen(str));
		printf("hash result: %d\n", hash_result);

		struct nat_mapping * current_nat_mapping;
		list_for_each_entry(current_nat_mapping, &nat.nat_mapping_list[hash_result], list){
			if(current_nat_mapping){
				if(current_nat_mapping->external_ip == daddr){
					if(current_nat_mapping->external_port == dport){
						//printf("Know how to go inside!\n");
						current_iphdr->daddr = htonl(current_nat_mapping->internal_ip);
						current_tcphdr->dport = htons(current_nat_mapping->internal_port);
						current_tcphdr->checksum = tcp_checksum(current_iphdr, current_tcphdr);
						current_iphdr->checksum = ip_checksum(current_iphdr);
						ip_send_packet(packet, len);

						time_t now = time(NULL);
						current_nat_mapping->update_time = now;
						if(current_tcphdr->flags == TCP_FIN + TCP_ACK){
							printf("It is a FIN + ACK!\n");
							current_nat_mapping->conn.external_fin = 1;
						}

						if(current_tcphdr->flags == TCP_RST){
							printf("It is a RST!\n");
							if(current_nat_mapping->conn.internal_fin + current_nat_mapping->conn.external_fin == 2){
								nat.assigned_ports[current_nat_mapping->external_port] = 0;
								printf("both FIN, so recall the assigned external port\n");
								list_delete_entry(&current_nat_mapping->list);
							}
						}
					}
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
	}
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		pthread_mutex_lock(&nat.lock);
		int i;
		for(i = 0; i < 65536; i++){
			if(nat.assigned_ports[i] == 1){
				printf("i is: %d\n", i);
				time_t now = time(NULL);

				char str[8];
				sprintf(str, "%d", i);
				int hash_result = hash8(str, (int)strlen(str));
				printf("Hash result is: %d\n", hash_result);

				struct nat_mapping * current_nat_mapping;
				list_for_each_entry(current_nat_mapping, &nat.nat_mapping_list[hash_result], list){
					printf("update time is: %ld\n", current_nat_mapping->update_time);
					if(now - current_nat_mapping->update_time > 5){
						printf("Time is too long, need to recall the assigned external port!\n");
						nat.assigned_ports[current_nat_mapping->external_port] = 0;
						list_delete_entry(&current_nat_mapping->list);
					}
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
		sleep(1);
	}

	return NULL;
}

// initialize nat table
void nat_table_init()
{
	memset(&nat, 0, sizeof(nat));

	int i;
	for (i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	nat.internal_iface = if_name_to_iface("n1-eth0");
	nat.external_iface = if_name_to_iface("n1-eth1");
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

// destroy nat table
void nat_table_destroy()
{
	pthread_mutex_lock(&nat.lock);

	int i;
	for (i = 0; i < HASH_8BITS; i++) {
		struct list_head *head = &nat.nat_mapping_list[i];
		struct nat_mapping *mapping_entry, *q;
		list_for_each_entry_safe(mapping_entry, q, head, list) {
			list_delete_entry(&mapping_entry->list);
			free(mapping_entry);
		}
	}

	pthread_kill(nat.thread, SIGTERM);

	pthread_mutex_unlock(&nat.lock);
}
