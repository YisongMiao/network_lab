#include "mac.h"
#include "headers.h"
#include "log.h"
#include "ether.h"

mac_port_map_t mac_port_map;

int compare_mac_addr(u8 ether_dhost_1[ETH_ALEN], u8 ether_dhost_2[ETH_ALEN]){
	int i = 0;
	int count = 0;
	for(i = 0; i < ETH_ALEN; i++){
		//printf("1st is: %d, 2nd is: %d, count is: %d\n", ether_dhost_1[i], ether_dhost_2[i], count);
		if(ether_dhost_1[i] == ether_dhost_2[i]){
			count += 1;
		}
	}
	if(count == ETH_ALEN){
		return 1;
	}
	else{
		return 0;
	}
}

void assign_mac_addr(u8 ether_dhost_1[ETH_ALEN], u8 ether_dhost_2[ETH_ALEN]){
	int i = 0;
	for(i = 0; i < ETH_ALEN; i++){
		ether_dhost_1[i] = ether_dhost_2[i];
	}
}

void init_mac_hash_table()
{
	bzero(&mac_port_map, sizeof(mac_port_map_t));

	pthread_mutexattr_init(&mac_port_map.attr);
	pthread_mutexattr_settype(&mac_port_map.attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mac_port_map.lock, &mac_port_map.attr);

	pthread_create(&mac_port_map.tid, NULL, sweeping_mac_port_thread, NULL);
}

void destory_mac_hash_table()
{
	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t *tmp, *entry;
	int i;
	for (i = 0; i < HASH_8BITS; i++) {
		entry = mac_port_map.hash_table[i];
		if (!entry) 
			continue;

		tmp = entry->next;
		while (tmp) {
			entry->next = tmp->next;
			free(tmp);
			tmp = entry->next;
		}
		free(entry);
	}
	pthread_mutex_unlock(&mac_port_map.lock);
}

iface_info_t *lookup_port(u8 mac[ETH_ALEN])
{
	// TODO: implement the lookup process here
	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t *current_entry;
	int i;
	for(i = 0; i < HASH_8BITS; i++){
		current_entry = mac_port_map.hash_table[i];
		if(!current_entry){
			continue;
		}
		while(current_entry){
			if(compare_mac_addr(mac, current_entry->mac)){
				time_t now = time(NULL);
				current_entry->visited = now;
				pthread_mutex_unlock(&mac_port_map.lock);
				//printf("We have looked up" ETHER_STRING " with fd: %d\n", ETHER_FMT(mac), current_entry->iface->fd);
				return current_entry->iface;
			}
			else{
				current_entry = current_entry->next;
			}
		}
	}
	pthread_mutex_unlock(&mac_port_map.lock);
	fflush(stdout);
	return NULL;
}

void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)
{
	// TODO: implement the insertion process here
	pthread_mutex_lock(&mac_port_map.lock);
	int hash_index = hash8(mac, ETH_ALEN);
	if(!mac_port_map.hash_table[hash_index]){
		mac_port_map.hash_table[hash_index] = malloc(sizeof(mac_port_entry_t) + 50);
		assign_mac_addr(mac_port_map.hash_table[hash_index]->mac, mac);
		mac_port_map.hash_table[hash_index]->iface = iface;
		time_t now = time(NULL);
		mac_port_map.hash_table[hash_index]->visited = now;
		mac_port_map.hash_table[hash_index]->next = NULL;
		printf("We have inserted fd: %d into %dth position\n", iface->fd, hash_index);
	}
	else{
		//if current hash index has mac entry, then we append it to the last entry!
		mac_port_entry_t *last_entry = mac_port_map.hash_table[hash_index];
		while(last_entry->next){
			last_entry = last_entry->next;
		}
		last_entry->next = malloc(sizeof(mac_port_entry_t) + 50);
		assign_mac_addr(last_entry->next->mac, mac);
		last_entry->next->iface = iface;
		time_t now = time(NULL);
		last_entry->next->visited = now;
		last_entry->next->next = NULL;
		printf("We have inserted fd: %d into %dth position\n", iface->fd, hash_index);
	}

	pthread_mutex_unlock(&mac_port_map.lock);
	fflush(stdout);
}

void dump_mac_port_table()
{
	mac_port_entry_t *entry = NULL;
	time_t now = time(NULL);

	fprintf(stdout, "dumping the mac_port table:\n");
	pthread_mutex_lock(&mac_port_map.lock);
	int i;

	for (i = 0; i < HASH_8BITS; i++) {
		entry = mac_port_map.hash_table[i];
		while (entry) {
			printf("Printing mac_table from i = %d\n", i);
			fprintf(stdout, ETHER_STRING " -> %s, fd: %d, visited_time: %d\n", ETHER_FMT(entry->mac), entry->iface->name, entry->iface->fd, (int)(now - entry->visited));
			entry = entry->next;
		}		
	}

	pthread_mutex_unlock(&mac_port_map.lock);
}

int sweep_aged_mac_port_entry()
{
	// TODO: implement the sweeping process here
	//fprintf(stdout, "TODO: implement the sweeping process here.\n");
	mac_port_entry_t  *current_entry, *previous_entry, *tmp;
	int delete_count = 0;
	pthread_mutex_lock(&mac_port_map.lock);
	int i;
	time_t now = time(NULL);
	for (i = 0; i < HASH_8BITS; i++) {
		current_entry = mac_port_map.hash_table[i];
		if(!current_entry){
			continue;
		}
		tmp = current_entry;
		while(current_entry){
			previous_entry = current_entry;
			if((int)(now - current_entry->visited) > 30){
				//connect previous's 'next pointer' to the next
				if(current_entry != tmp && current_entry->next){
					previous_entry->next = current_entry->next;
				}
				if(current_entry == tmp && current_entry->next){
					mac_port_map.hash_table[i] = current_entry->next;
				}
				printf("We are deleting fd:%d\n", current_entry->iface->fd);
				free(mac_port_map.hash_table[i]);
				mac_port_map.hash_table[i] = NULL;
				delete_count += 1;
				printf("We have deleted fd:%d\n", current_entry->iface->fd);

				break;
			}
			current_entry = current_entry->next;
		}
	}
	pthread_mutex_unlock(&mac_port_map.lock);
	return delete_count;
}

void *sweeping_mac_port_thread(void *nil)
{	
	time_t start_time = time(NULL);
	while (1) {
		sleep(1);
		time_t current_time = time(NULL);
		int n = sweep_aged_mac_port_entry();
		if (n > 0){
			log(DEBUG, "%d aged entries in mac_port table are removed.", n);
			printf("Current time: %d seconds, %d aged entries in mac_port table are removed.", (int)(current_time - start_time), n);
		}
		dump_mac_port_table();
	}

	return NULL;
}
