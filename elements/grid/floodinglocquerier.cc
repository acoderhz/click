/*
 * floodinglocquerier.{cc,hh} -- Flooding protocol for finding Grid locations
 * Douglas S. J. De Couto
 * based on arpquerier.{cc,hh} by Robert Morris and Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "floodinglocquerier.hh"
#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>

FloodingLocQuerier::FloodingLocQuerier()
  : _expire_timer(expire_hook, (unsigned long)this)
{
  MOD_INC_USE_COUNT;
  add_input(); /* GRID_NBR_ENCAP packets */
  add_input(); /* flooding queries and responses */
  add_output(); /* GRID_NBR_ENCAP packets  */
  add_output(); /* flooding queries */
}

FloodingLocQuerier::~FloodingLocQuerier()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}


FloodingLocQuerier *
FloodingLocQuerier::clone() const
{
  return new FloodingLocQuerier;
}


int
FloodingLocQuerier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpEthernetAddress, "Ethernet address", &_my_en,
		     cpIPAddress, "IP address", &_my_ip,
		     0);
}

int
FloodingLocQuerier::initialize(ErrorHandler *)
{
  _expire_timer.attach(this);
  _expire_timer.schedule_after_ms(EXPIRE_TIMEOUT_MS);
  _loc_queries = 0;
  _pkts_killed = 0;
  return 0;
}

void
FloodingLocQuerier::uninitialize()
{
  _expire_timer.unschedule();
  for (int i = 0; i < NMAP; i++) {
    for (LocEntry *t = _map[i]; t; ) {
      LocEntry *n = t->next;
      if (t->p)
	t->p->kill();
      delete t;
      t = n;
    }
    _map[i] = 0;
  }
}


void
FloodingLocQuerier::expire_hook(unsigned long thunk)
{
  FloodingLocQuerier *locq = (FloodingLocQuerier *)thunk;
  int jiff = click_jiffies();
  for (int i = 0; i < NMAP; i++) {
    LocEntry *prev = 0;
    while (1) {
      LocEntry *e = (prev ? prev->next : locq->_map[i]);
      if (!e)
	break;
      if (e->ok) {
	int gap = jiff - e->last_response_jiffies;
	if (gap > 120*CLICK_HZ) {
	  // click_chatter("FloodingLocQuerier timing out %x", e->ip.addr());
	  // delete entry from map
	  if (prev) prev->next = e->next;
	  else locq->_map[i] = e->next;
	  if (e->p)
	    e->p->kill();
	  delete e;
	  continue;		// don't change prev
	} else if (gap > 60*CLICK_HZ)
	  e->polling = 1;
      }
      prev = e;
    }
  }
  locq->_expire_timer.schedule_after_ms(EXPIRE_TIMEOUT_MS);
}

void
FloodingLocQuerier::send_query_for(const IPAddress &want_ip)
{
  click_ether *e;
  grid_hdr *gh;
  grid_loc_query *fq;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*gh) + sizeof(*fq));
  if (q == 0) {
    click_chatter("in %s: cannot make packet!", id().cc());
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  gh = (grid_hdr *) (e + 1);
  fq = (grid_loc_query *) (gh + 1);

  memcpy(e->ether_dhost, "\xff\xff\xff\xff\xff\xff", 6);
  memcpy(e->ether_shost, _my_en.data(), 6);
  e->ether_type = htons(ETHERTYPE_GRID);

  gh->hdr_len = sizeof(grid_hdr);
  gh->type = grid_hdr::GRID_LOC_QUERY;
  gh->ip = gh->tx_ip = _my_ip;
  gh->total_len = htons(q->length() - sizeof(click_ether));

  fq->dst_ip = want_ip;
  fq->seq_no = htonl(_loc_queries);

  // make sure we never propagate our own queries!
  _query_seqs.insert(_my_ip, _loc_queries);

  _loc_queries++;
  output(1).push(q);
}

/* if the packet has location information already in it, just send it
 * out, ignoring the state of our location table (e.g. don't update
 * our table with the packet info, and don't update the packet with
 * any info we might have).
 *
 * otherwise....
 * If the packet's location is in the table, fill in the
 * grid_nbr_encap header and push it out.  Otherwise push out a query
 * packet.  May save the packet in the ARP table for later sending.
 * May call p->kill().  */
void
FloodingLocQuerier::handle_nbr_encap(Packet *p)
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);

#if 1
  click_chatter("%s: got packet for %s", id().cc(), IPAddress(nb->dst_ip).s().cc());
#endif

  // see if packet has location info in it already
  if (nb->dst_loc_good) {
    output(0).push(p);
    return;
  }
  
  // oops, no loc info, let's look it up!
  IPAddress ipa(nb->dst_ip);
  int bucket = (ipa.data()[0] + ipa.data()[3]) % NMAP;
  LocEntry *ae = _map[bucket];
  while (ae && ae->ip != ipa)
    ae = ae->next;

  if (ae) {
    if (ae->polling) {
      send_query_for(ae->ip);
      ae->polling = 0;
    }
    
    if (ae->ok) {
      WritablePacket *q = p->uniqueify();
      grid_hdr *gh2 = (grid_hdr *) (q->data() + sizeof(click_ether));
      gh2->tx_ip = _my_ip;
      grid_nbr_encap *nb2 = (grid_nbr_encap *) (gh2 + 1);
      nb2->dst_loc = ae->loc;
      nb2->dst_loc_err = htons(ae->loc_err);
      nb2->dst_loc_good = ae->loc_good;
      if (!ae->loc_good)
	click_chatter("FloodingLocQuerier %s: invalid location information in table!  sending packet anyway...", id().cc());
      output(0).push(q);
      p->kill();
    } else {
      if (ae->p) {
        ae->p->kill();
	_pkts_killed++;
      }
      ae->p = p;
      send_query_for(p->dst_ip_anno());
    }
    
  } else {
    LocEntry *ae = new LocEntry;
    ae->ip = ipa;
    ae->ok = ae->polling = 0;
    ae->p = p;
    ae->next = _map[bucket];
    _map[bucket] = ae;
    send_query_for(p->dst_ip_anno());
  }
}

/*
 * Got a loc query response.
 * Update our loc table.
 * If there was a packet waiting to be sent, send it.
 */
void
FloodingLocQuerier::handle_reply(Packet *p)
{
  if (p->length() < sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap))
    return;
  
  click_ether *ethh = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (ethh + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);
  IPAddress ipa(nb->dst_ip);
  int bucket = (ipa.data()[0] + ipa.data()[3]) % NMAP;
  LocEntry *ae = _map[bucket];
  while (ae && ae->ip != ipa)
    ae = ae->next;
  if (!ae)
    return;
  
  unsigned int loc_seq_no = ntohl(gh->loc_seq_no);
  if (ae->loc_seq_no > loc_seq_no) {
    ae->loc = gh->loc;
    ae->loc_err = ntohs(gh->loc_err);
    ae->loc_good = gh->loc_good;
    ae->loc_seq_no = loc_seq_no;
    ae->ok = 1;
    ae->polling = 0;
    ae->last_response_jiffies = click_jiffies();
  }
  Packet *cached_packet = ae->p;
  ae->p = 0;

  if (cached_packet)
    handle_nbr_encap(cached_packet);

  p->kill();
}

void 
FloodingLocQuerier::handle_query(Packet *p)
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  grid_loc_query *lq = (grid_loc_query *) (gh + 1);

#if 1
  click_chatter("%s: got query for %s from %s (%u)", id().cc(), IPAddress(lq->dst_ip).s().cc(), IPAddress(gh->ip).s().cc(), ntohl(lq->seq_no));
#endif

  if (lq->dst_ip == (unsigned int) _my_ip) {
    click_chatter("FloodingLocQuerier %s: got location query for us, but it should go to the LocQueryResponder.  Check the configuration.", id().cc());
    p->kill();
    return;
  }
  else {
    // (possibly) propagate the query
    unsigned int *seq_no = _query_seqs.findp(gh->ip);
    unsigned int q_seq_no = ntohl(lq->seq_no);
    if (seq_no && *seq_no >= q_seq_no) {
      // already handled this query
      p->kill();
      return;
    }
    _query_seqs.insert(gh->ip, q_seq_no);
    WritablePacket *wp = p->uniqueify();
    gh = (grid_hdr *) (p->data() + sizeof(click_ether));
    gh->tx_ip = _my_ip; 
    // FixSrcLoc will handle the rest of the tx_* fields
    output(1).push(wp);
  }    
}

void
FloodingLocQuerier::push(int port, Packet *p)
{
  if (port == 0)
    handle_nbr_encap(p);
  else {
    grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
    if (gh->type == grid_hdr::GRID_LOC_QUERY)
      handle_query(p);
    else if (gh->type == grid_hdr::GRID_LOC_REPLY) {
      handle_reply(p);
      p->kill();
    }
    else {
      click_chatter("%s: got an unexpected packet type", id().cc());
      assert(0);
    }
  }
}

String
FloodingLocQuerier::read_table(Element *e, void *)
{
  FloodingLocQuerier *q = (FloodingLocQuerier *)e;
  String s;
  for (int i = 0; i < NMAP; i++)
    for (LocEntry *e = q->_map[i]; e; e = e->next) {
      s += e->ip.s() + " " + (e->ok ? "1" : "0") + " " + e->loc.s() + "\n";
    }
  return s;
}

static String
FloodingLocQuerier_read_stats(Element *e, void *)
{
  FloodingLocQuerier *q = (FloodingLocQuerier *)e;
  return
    String(q->_pkts_killed) + " packets killed\n" +
    String(q->_loc_queries) + " loc queries sent\n";
}

void
FloodingLocQuerier::add_handlers()
{
  add_read_handler("table", read_table, (void *)0);
  add_read_handler("stats", FloodingLocQuerier_read_stats, (void *)0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FloodingLocQuerier)
