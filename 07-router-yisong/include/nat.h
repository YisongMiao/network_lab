#ifndef __NAT_H__
#define __NAT_H__

#include "types.h"
#include "base.h"
#include "list.h"

#include <time.h>
#include <pthread.h>

#define NAT_PORT_MIN	12345
#define NAT_PORT_MAX	23456

// TODO
#define ICMP_TIMEOUT	30 
#define TCP_ESTABLISH_TIMEOUT	30
#define TCP_TRANSLATE_TIMEOUT	30

enum packet_dir { DIR_IN = 1, DIR_OUT, DIR_BYPASS, DIR_INVALID };

struct nat_connection {
	struct list_head list;

	u8 internal_syn;
	u8 external_syn;

	u8 internal_fin;
	u8 external_fin;

	u8 internal_fack;
	u8 external_fack;

	u32	internal_fin_seq;
	u32	external_fin_seq;

	u32	external_ip;	// both ip and port are in host-order
	u16	external_port;

	time_t update_time;
};

struct nat_mapping {
	struct list_head list;
	int proto;		// see ip.h	iphdr->protocol

	u32 internal_ip;
	u32 external_ip;
	u16 internal_id;	// port or icmp id
	u16 external_id;

	time_t update_time;
	struct list_head conn_list;
};

struct tcp_syn {
	struct list_head list;

	u32 sip;
	u16 sport;
	time_t arrived;
	char *packet;	// pending packet
	int len;

	iface_info_t *in_iface;
};

struct nat_table {
	struct list_head nat_mapping_list;
	struct list_head tcp_syn_list;

	iface_info_t *internal_iface;
	iface_info_t *external_iface;

	u16 assigned_port;

	pthread_mutex_t lock;
	pthread_t thread;
};


void nat_table_init();
void nat_table_destroy();
void nat_handle_packet(iface_info_t *iface, char *packet, int len);

#endif
