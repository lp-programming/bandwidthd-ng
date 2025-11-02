module;
export module VlanHeader;

import net_integer;
using types::net_integer::net_u16;

export 
class VlanHeader {
public:
  net_u16   vlan_tag;     /* vlan tag information */
  net_u16   encapsulated_proto;
};
