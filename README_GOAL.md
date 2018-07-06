
Goal extension:

Append:
append <size>b to {priority,overflow}_list [hh <handler id>] [ph <handler id>] [ch <handler id>] [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [mem <size of shared memory>] allowed <rank> ct <counter id> [use_once] tag <tag>

It appends an ME to the priority_list or overflow_list. The matching is performed according to the <tag> value. Only the rank specified by <rank> can initiate operation matching this ME. Packet handlers indices can be specified with:
   - hh: header handler
   - ph: payload handler
   - ch: completion handler
Additional uint32_t arguments can be passed to the packet handlers, they are specified with arg1..4.
The handlers matching the ME can share memory: the size of the memory to share is specified by the mem attribute.
  
  
  [label:]put <size>b to <rank> [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [ct <counter id>] tag <tag>

  [label:]get <size>b from <rank> [ct <counter id>] tag <tag>

  [label:]tappend <size>b to {priority,overflow}_list [hh <handler id>] [ph <handler id>] [ch <handler id>] [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [mem <size of shared memory>] allowed <rank> when <counter id> reaches <counter value> ct <counter id> [use_once] tag <tag>

  [label:]tput <size>b to <rank> [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] when <counter id> reaches <counter value> [ct <counter id>] tag <tag>

  [label:]tget <size>b from <rank> when <counter id> reaches <counter value> [ct <counter id>] tag <tag>

  [label:]test <counter id> for <counter value>

  [label:]calc <delay>

  [label:]calcgem5 <function id> <length>b
 
  <label> requires <label>

The handler are defined in gem5_files/smp.c. The handler index is the position of the handler function in the
"handlers" array (see main of gem5_files/smp_code.c).

