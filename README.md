# rtpgen
rtp traffic generator powerd by dpdk

* original soruce code is dpdk/sample/l2fwd on 17.11


## Example

```
./build/rtpgen -c0x02 -- -p1 -T0 -P80 -t18 -M52:54:00:c6:1c:79  
```

- EAL Option -c0x02 : use 2nd core
- -p1 : using one port only
- -T0 : screen reflesh off
- -P80 : payload length is 80 bytes
- -M52:54:00:c6:1c:79 : port 0's peer ethernet address is '52:54:00:c6:1c:79'

## TODO
### Setting Options
- ptime setting(currently 20ms)

### Features
- Multiport support
- L3 routing
- ARP resolve
