/*
 * setgridchecksum.{cc,hh} -- element sets Grid header checksum
 * Douglas S. J. De Couto
 * adapted from setipchecksum.{cc,hh} by Robert Morris
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
#include "setgridchecksum.hh"
#include <click/glue.hh>
#include "grid.hh"
#include <click/click_ether.h>
#include <click/click_ip.h>

SetGridChecksum::SetGridChecksum()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

SetGridChecksum::~SetGridChecksum()
{
  MOD_DEC_USE_COUNT;
}

SetGridChecksum *
SetGridChecksum::clone() const
{
  return new SetGridChecksum();
}

Packet *
SetGridChecksum::simple_action(Packet *xp)
{
  WritablePacket *p = xp->uniqueify();
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  unsigned plen = p->length();
  unsigned int tlen = ntohs(gh->total_len);
  
  if (!gh || plen < sizeof(grid_hdr) + sizeof(click_ether))
    goto bad;

  if (/* hlen < sizeof(grid_hdr) || */ // grid_hdr size keeps changing...
      tlen > plen - sizeof(click_ether))
    goto bad;

  gh->cksum = 0;
  gh->cksum = in_cksum((unsigned char *) gh, tlen);

  return p;

 bad:
  click_chatter("SetGridChecksum: bad lengths");
  p->kill();
  return(0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SetGridChecksum)
