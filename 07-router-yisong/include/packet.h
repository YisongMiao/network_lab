#ifndef __PACKET_H__
#define __PACKET_H__

#include "base.h"
#include "types.h"

void encap_send_ether_packet(iface_info_t *iface, u8 dst[], u16 proto,
		char *data, int len);

void broadcast_packet(iface_info_t *iface, const char *packet, int len);
void iface_send_packet(iface_info_t *iface, const char *packet, int len);

#endif
