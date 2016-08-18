# What
​
sheep_net is a character virtual device that bridge between BasiliskII and Physical Ethernet card(P)

Here is logical diagram:

Guest Mac OS in emulation (G) <==> Basilisk II (B) <==> /dev/sheep_net (S) <==> Physical Ethernet card on host (P)

sheep_net module masquerade and de-masquerade MAC address on Ethernet frame so that Guest OS and host share the same MAC address with different IP.

See details in [IP aliasing](https://en.wikipedia.org/wiki/IP_aliasing)

# How
## How it works

Sample Setting:

Guest Mac OS IP: 192.168.2.2, Fake MAC address: 84:38:35:5e:c5:5b

Host OS Physical Ethernet car IP: 192.168.2.3, physical MAC address: 84:38:35:5e:c5:5a

From outside, we see 192.168.2.2 and 192.168.2.3 share the same physical MAC address: 84:38:35:5e:c5:5a

From insides, sheep_net module masquerade and de-masquerade MAC address of Ethernet packet between Basilisk and

```
B ==> S ==> P: de-masquerade MAC, convert Fake to Physical
B <== S <== P: masquerade MAC, convert Physical to Fake
```

## How to compile
```
​cd Linux/NetDriver
make
//create sheep_net device node
sudo make dev
sudo chown [user account] /dev/sheep_net
sudo make install
sudo modprobe sheep_net
```

## How to use
1. Disable IP forwarding on host (Recommended: By disabling it, guest OS won't receive duplicate IP packet from host again.)
2. Disable firewall on host (Recommended: host may send ICMP host unreachable to gateway. Or you can disable ICMP sending from host by changing iptables.)
3. sudo modprobe sheep_net
4. sudo chown [user account] /dev/sheep_net
5. Launch BasiliskII, choose your physical Ethernet card interface in network tab
