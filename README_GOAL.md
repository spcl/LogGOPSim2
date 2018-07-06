
Goal extension:

Append: 
append <size>b to {priority,overflow}_list [hh <handler id>] [ph <handler id>] [ch <handler id>] [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [mem <size of shared memory>] allowed <rank> ct <counter id> [use_once] tag <tag>

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

