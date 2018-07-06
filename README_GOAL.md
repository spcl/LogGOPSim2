
# Goal extension

### Append
```
append <size>b to {priority,overflow}_list [hh <handler id>] [ph <handler id>] [ch <handler id>] [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [mem <size of shared memory>] allowed <rank> ct <counter id> [use_once] tag <tag>
```

It appends an ME to the priority_list or overflow_list. The matching is performed according to the <tag> value. Only the rank specified by <rank> can initiate operation matching this ME. A counter ID can be associated with the ME by using the "ct" attribute. The counter is incremented when a put matches the ME. The counter ID can then be used for triggered operations. 
   
Packet handlers indices can be specified with:
   - hh: header handler index
   - ph: payload handler index
   - ch: completion handler index
   
Additional uint32_t arguments can be passed to the packet handlers, they are specified with arg1..4.
The handlers matching the ME can share memory: the size of the memory to share is specified by the mem attribute.
  
### Put
```
put <size>b to <rank> [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [ct <counter id>] tag <tag>
```
Executes a put operation of size <size> bytes towards rank <rank>. A counter ID can be associated with the put: it will be incremented when the put is completed (at the target). 

Additional uint64_t arguments can be passed to the packet handlers, they are specified with arg1..4.
TODO: uniform types between put and append.

### Get
```
get <size>b from <rank> [ct <counter id>] tag <tag>
```
Issues a get of <size> bytes from rank <rank> from an ME with tag <tag>. A counter can be associated with this operation by using the "ct" attribute. 
TODO: add arguments

### Triggered Operations
```
tappend <size>b to {priority,overflow}_list [hh <handler id>] [ph <handler id>] [ch <handler id>] [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] [mem <size of shared memory>] allowed <rank> when <counter id> reaches <counter value> ct <counter id> [use_once] tag <tag>
```

 ```
 tput <size>b to <rank> [arg1 <value>] [arg2 <value>] [arg3 <value>] [arg4 <value>] when <counter id> reaches <counter value> [ct <counter id>] tag <tag>
```

```
tget <size>b from <rank> when <counter id> reaches <counter value> [ct <counter id>] tag <tag>
```

They have the same syntax of normal operations but, in addition, they take also "when <ct> reaches <value>". This means that the operation will be triggered when the counter ID <ct> will reach the value <value>.
   
### Counter Operations

```
 test <counter id> for <counter value>
```
TODO: rename to wait
It waits until the counter with ID <counter id> reaches the value <counter value>

### Others
```
calc <delay>
```
Waits for <delay> time units (not sure, need to check).







Scratch

  [label:]calcgem5 <function id> <length>b

  <label> requires <label>

The handler are defined in gem5_files/smp.c. The handler index is the position of the handler function in the
"handlers" array (see main of gem5_files/smp_code.c).

