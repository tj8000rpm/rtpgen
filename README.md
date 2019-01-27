# rtpgen
rtp traffic generator powerd by dpdk

* original soruce code is dpdk/sample/l2fwd on 17.11


## Example

```
./build/rtpgen -c 0x02 -- -p 1 -T 0 -M 52:54:00:c6:1c:79 -P 160 -I 20 -t 0 -f sample_payload.raw
```

- EAL Option -c 0x02 : use 2nd core
- -p 1 : using one port only
- -T 0 : screen reflesh off
- -M 52:54:00:c6:1c:79 : port 0's peer ethernet address is '52:54:00:c6:1c:79'
- -P 160 : payload length is 160 bytes
- -I 20 : ptime 20msec
- -t 0 : RTP Payload Type is 0(PCMU/G.711)
- -f sample_payload.raw : raw file of payload(PCMU/G.711 encoded/No headers)

## TODO
### Setting Options

### Features
- Multiport support
- L3 routing
- ARP resolve
