#ifndef SETADDRESS_HH
#define SETADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>

/*
 * =c
 * SetIPAddress(IP)
 * =s sets destination IP address annotations
 * =d
 * Set the destination IP address annotation of incoming packets to the
 * static IP address IP.
 *
 * =a StoreIPAddress, GetIPAddress
 */

class SetIPAddress : public Element {
  
  IPAddress _ip;
  
 public:
  
  SetIPAddress();
  ~SetIPAddress();
  
  const char *class_name() const		{ return "SetIPAddress"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetIPAddress *clone() const			{ return new SetIPAddress; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
