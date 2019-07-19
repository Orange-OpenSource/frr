/*
 * This is an implementation of Segment Routing for IS-IS
 * as per draft draft-ietf-isis-segment-routing-extensions-24
 *
 * Module name: Segment Routing header definitions
 *
 * Author: Olivier Dugeon <olivier.dugeon@orange.com>
 *
 * Copyright (C) 2019 Orange Labs http://www.orange.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _FRR_ISIS_SR_H
#define _FRR_ISIS_SR_H

#include "stream.h"

/*
 * Segment Routing information are transport through LSP:
 *  - Extended IS Reachability          TLV = 22   (RFC5305)
 *  - Extended IP Reachability          TLV = 135  (RFC5305)
 *
 *  and support following sub-TLV:
 *
 * Name					Value	TLVs
 * ____________________________________________________________
 * SID Label				 1
 *
 * Prefix Segment Identifier		 3	135 (235,236 and 237)
 *
 * Adjacency Segment Identifier		31	22 (23, 141, 222 and 223)
 * LAN Adjacency Segment Identifier	32	22 (23, 141, 222 and 223)
 *
 * Segment Routing Capability		 2	242
 * Segment Routing Algorithm		19	242
 * Node Maximum Stack Depth (MSD)	23	242
 *
 */
/* Default Route priority for ISIS Segment Routing */
#define ISIS_SR_PRIORITY_DEFAULT	10

/* Label range for Adj-SID attribution purpose */
#define ADJ_SID_MIN                    5000
#define ADJ_SID_MAX                    5999

#define ISIS_SR_DEFAULT_METRIC		1

/* Segment Routing TLVs as per draft-ietf-isis-segment-routing-extensions-24 */

/* Segment ID could be a Label (3 bytes) or an Index (4 bytes) */
#define SID_LABEL	3
#define SID_LABEL_SIZE(U) (U - 1)
#define SID_INDEX	4
#define SID_INDEX_SIZE(U) (U)

/*
 * subTLVs definition, serialization and de-serialization
 * are defined in isis_tlvs.[c,h]
 */

/*
 * Following section define structure for Segment Routing management
 */
#define IS_SR(a)	(a && a->srdb.enabled)
#define IS_SELF(srn)    (srn == srn->area->srdb.self)

/* Status to manage SID and NHLFE */
enum sid_status { IDLE_SID, NEW_SID, MODIFIED_SID, UNCHANGED_SID };
enum nh_state { IDLE_NH, NEW_NH, ACTIVE_NH, UNACTIVE_NH, UNCHANGED_NH };

/* NOTE: these values must be in sync with the YANG module. */
enum sr_sid_value_type {
	SR_SID_VALUE_TYPE_INDEX = 0,
	SR_SID_VALUE_TYPE_ABSOLUTE = 1,
};


/* NOTE: these values must be in sync with the YANG module. */
enum sr_last_hop_behavior {
	SR_LAST_HOP_BEHAVIOR_EXP_NULL = 0,
	SR_LAST_HOP_BEHAVIOR_NO_PHP = 1,
	SR_LAST_HOP_BEHAVIOR_PHP = 2,
};

/* Structure aggregating all received SR info from LSPs by node */
struct sr_node {
	RB_ENTRY(sr_node) entry;

	/* System ID of the SR Node */
	uint8_t sysid[ISIS_SYS_ID_LEN];

	/* Router Capabilities */
	struct isis_router_cap cap;

	/* List of Prefix & IS advertise by this node */
	struct list *pref_sids;	/* For Prefix SID inc. Node SID*/
	struct list *adj_sids;	/* For Adjacency SID inc. LAN */

	/* Pointer to FRR SR-Node or NULL if it is not a neighbor */
	struct sr_node *neighbor;

	/* Back pointer to area */
	struct isis_area *area;
};
RB_HEAD(srdb_node_head, srn);
RB_PROTOTYPE(srdb_node_head, sr_node, entry, sr_node_cmp)

#define IS_SR_SELF(s, a)	(s && a && s == a->srdb.self)

/* Segment Routing - NHLFE info: support IPv4 Only */
struct sr_nhlfe {
	/* State of this NHLFE */
	enum nh_state state;

	/* Nexthop information including SR Node */
	struct in_addr nexthop;
	struct in6_addr nexthop6;
	ifindex_t ifindex;
	struct sr_node *srnext;;

	/* Input and output MPLS labels */
	mpls_label_t label_in;
	mpls_label_t label_out;
};

/* Structure aggregating all Segment Routing Adjacency information */
/* which are generally advertised by pair: primary + backup */
struct sr_adjacency {

	/* prefix IPv4 or IPv6 */
	struct prefix prefix;

	/* Adjacency & LAN Adjacency SID */
	struct isis_adj_sid *adj_sid;
	struct isis_lan_adj_sid *lan_sid;

	/* SR NHLFE for this Adjacency */
	struct sr_nhlfe nhlfe;

	/* Back pointer to SR Node which advertise this Adjacency */
	struct sr_node *srn;

	/* Back pointer to isis adjacency to which SR Adjacency is attached */
	struct isis_adjacency *adj;
};

/* Structure aggregating all Segment Routing Prefix information */
struct sr_prefix {
	RB_ENTRY(sr_prefix) entry;

	/* prefix IPv4 or IPv6 */
	struct prefix prefix;

	/* SID, Flags & Algo to manage this prefix parameters */
	struct isis_prefix_sid sid;

	/* Segment Routing status */
	enum sid_status status;

	/* List of SR NHLFE for this prefix */
	struct list *nhlfes;

	/* Back pointer to SR Node which advertise this Prefix */
	struct sr_node *srn;
};
RB_HEAD(srdb_prefix_head, srp);
RB_PROTOTYPE(srdb_prefix_head, sr_prefix, entry, sr_prefix_cmp)

/* Structure aggregating all ISIS Segment Routing information for the node */
struct isis_sr_db {
	/* Status of Segment Routing: enable or disable */
	bool enabled;

	/* Ongoing Update following an ISIS SPF */
	bool update;
	struct thread *t_sr_update;

	/* IPv4 or IPv6 Segment Routing */
	uint8_t flags;

	/* FRR SR node */
	struct sr_node *self;

	/* SR information from all nodes */
	struct srdb_node_head sr_nodes;

	/* List of Prefix-SIDs */
	struct srdb_prefix_head prefix_sids;

	/* Local SR info announced in Router Capability TLV 242 */

	/* Algorithms supported by the node */
	uint8_t algo[SR_ALGORITHM_COUNT];
	/*
	 * Segment Routing Global Block lower & upper bound
	 * Only one range supported in this code
	 */
	uint32_t lower_bound;
	uint32_t upper_bound;
	/* Label Manager flag to indicate that ranges are reserved */
	bool srgb_lm;
	bool adj_lm;

	/* Maximum SID Depth supported by the node */
	uint8_t msd;
};

/* Prototypes definition */
/* Segment Routing initialization functions */
extern void isis_sr_init(struct isis_area *area);
extern void isis_sr_start(struct isis_area *area);
extern void isis_sr_stop(struct isis_area *area);
extern void isis_sr_term(void);
extern void isis_sr_srgb_update(struct isis_area *area);
extern void isis_sr_msd_update(struct isis_area *area);
/* Segment Routing Prefix management functions */
extern struct sr_prefix *isis_sr_prefix_add(struct isis_area *area,
					    const struct prefix *prefix);
extern void isis_sr_prefix_del(struct sr_prefix *srp);
extern void isis_sr_prefix_commit(struct sr_prefix *srp);
extern struct sr_prefix *isis_sr_prefix_find(const struct isis_area *area,
					     const struct prefix *prefix);
extern void isis_sr_prefix_update(struct isis_area *area,
				  struct prefix *prefix);
/* Segment Routing re-routing function */
extern void isis_sr_update_timer_add(struct isis_area *area);

#endif /* _FRR_ISIS_SR_H */