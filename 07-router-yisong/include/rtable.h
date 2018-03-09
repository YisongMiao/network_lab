#ifndef __RTABLE_H__
#define __RTABLE_H__

#include "base.h"
#include "types.h"

#include "list.h"

// NOTE: 1, the table supports only IPv4 address;
// 		 2, Addresses are stored in host byte order.
typedef struct {
	struct list_head list;
	u32 dest;
	u32 gw;
	u32 mask;
	int flags;
	char if_name[16];
	iface_info_t *iface;
} rt_entry_t;

extern struct list_head rtable;

void init_rtable();
void load_static_rtable();
void clear_rtable();
void add_rt_entry(rt_entry_t *entry);
void remove_rt_entry(rt_entry_t *entry);
void print_rtable();
rt_entry_t *longest_prefix_match(uint32_t ip);

#endif
