#ifndef __ETHER_H__
#define __ETHER_H__

#include "types.h"

#define ETH_ALEN 		6
#define ETH_FRAME_LEN	1514

#define ETH_P_ALL		0x0003          /* Every packet (be careful!!!) */
#define ETH_P_IP		0x0800
#define ETH_P_ARP		0x0806

struct ether_header {
	u8 ether_dhost[ETH_ALEN];
	u8 ether_shost[ETH_ALEN];
	u16 ether_type;
};

typedef struct ether_header ether_header_t;

#define ETHER_HDR_SIZE sizeof(struct ether_header)
#define ETHER_STRING "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHER_FMT(m) m[0],m[1],m[2],m[3],m[4],m[5]


#endif
