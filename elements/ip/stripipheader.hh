#ifndef STRIPIPHEADER_HH
#define STRIPIPHEADER_HH

/*
 * =c
 * StripIPHeader()
 * =s
 * strips outermost IP header
 * V<encapsulation>
 * =d
 * Removes the outermost IP header from IP packets based on the IP Header annotation.
 *
 * =a CheckIPHeader, CheckIPHeader2, MarkIPHeader, UnstripIPHeader
 */

#include <click/element.hh>

class StripIPHeader : public Element {

 public:
  
  StripIPHeader();
  ~StripIPHeader();
  
  const char *class_name() const		{ return "StripIPHeader"; }
  StripIPHeader *clone() const			{ return new StripIPHeader; }

  Packet *simple_action(Packet *);
  
};

#endif
