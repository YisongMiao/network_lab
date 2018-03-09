#ifndef __NOSPF_NBR_H__
#define __NOSPF_NBR_H__

#include "base.h"
#include "types.h"
#include "list.h"

typedef struct {
	struct list_head list;
	u32 	nbr_id;			// neighbor ID
	u32		nbr_ip;			// neighbor IP
	u32		nbr_mask;		// neighbor mask
	//u8		alive;			// alive for #(seconds)
	u32		alive;			// alive for #(seconds)
	u8	mac[ETH_ALEN];		// new added
} mospf_nbr_t;

#endif
