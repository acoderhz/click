/*
 * setcyclecount.{cc,hh} -- set cycle counter annotation
 * Eddie Kohler
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
#include "setcyclecount.hh"
#include <click/glue.hh>

SetCycleCount::SetCycleCount()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetCycleCount::~SetCycleCount()
{
  MOD_DEC_USE_COUNT;
}

SetCycleCount *
SetCycleCount::clone() const
{
  return new SetCycleCount();
}

void
SetCycleCount::push(int, Packet *p)
{
  p->set_perfctr_anno(click_get_cycles());
  output(0).push(p);
}

Packet *
SetCycleCount::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p->set_perfctr_anno(click_get_cycles());
  return p;
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(SetCycleCount)
