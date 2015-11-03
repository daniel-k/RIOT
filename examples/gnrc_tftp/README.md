# gnrc_tftp example

## Connecting RIOT native and the Linux host

> **Note:** RIOT does not support IPv4, so you need to stick to IPv6 anytime. To establish a connection between RIOT and the Linux host,
you will need `tftp` (with IPv6 support). On Ubuntu and Debian you would need the package `tftp-hpa` for TFTP client and `tftpd-hpa` for TFTP server.
Be aware that many programs require you to add an option such as -6 to tell them to use IPv6, otherwise they
will fail. If you're using a _Raspberry Pi_, run `sudo modprobe ipv6` before trying this example, because raspbian does not load the
IPv6 module automatically.
On some systems (openSUSE for example), the _firewall_ may interfere, and prevent some packets to arrive at the application (they will
however show up in Wireshark, which can be confusing). So be sure to adjust your firewall rules, or turn it off (who needs security anyway).

First, create a tap interface (to which RIOT will connect) and a bridge (to which Linux will connect):

    sudo ip tuntap add tap0 mode tap user ${USER}
    sudo ip link set tap0 up

Now you can start the `gnrc_tftp` example by invoking `make term`. This should automatically connect to the `tap0` interface. If
this doesn't work for some reason, run `make` without any arguments, and then run the binary manually like so (assuming you are in the `examples/gnrc_tftp` directory):

To verify that there is connectivity between RIOT and Linux, go to the RIOT console and run `ifconfig`:

    > ifconfig
    Iface  7   HWaddr: ce:f5:e1:c5:f7:5a
    inet6 addr: ff02::1/128  scope: local [multicast]
    inet6 addr: fe80::ccf5:e1ff:fec5:f75a/64  scope: local
    inet6 addr: ff02::1:ffc5:f75a/128  scope: local [multicast]

Copy the [link-local address](https://en.wikipedia.org/wiki/Link-local_address) of the RIOT node (prefixed with `fe80`) and try to ping it **from the Linux node**:

    ping6 fe80::ccf5:e1ff:fec5:f75a%tap0

Note that the interface on which to send the ping needs to be appended to the IPv6 address, `%tap0` in the above example. When talking to the RIOT node, you always want to send to/receive from the `tap0` interface.

If the pings succeed you can go on to send UDP packets. To do that, first start a UDP server on the RIOT node:

    > tftps start
    tftp_server: Starting TFTP service at port 69

Now, on the Linux host, you can run tftp to connect with RIOT's TFTP server:

    tftp -6 fe80::ccf5:e1ff:fec5:f75a%tap0

The `-6` option is necessary to tell tftp to use IPv6 only.

You should now see that the TFTP messages are received on the RIOT side. Opening a TFTP server on the Linux side is also possible. To do that, write down the IP address of the host (run on Linux):

    ifconfig tap0
    tap0     Link encap:Ethernet  HWaddr ce:f5:e1:c5:f7:59
            inet6 addr: fe80::4049:5fff:fe17:b3ae/64 Scope:Link
            UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
            RX packets:6 errors:0 dropped:0 overruns:0 frame:0
            TX packets:36 errors:0 dropped:0 overruns:0 carrier:0
            collisions:0 txqueuelen:0
            RX bytes:488 (488.0 B)  TX bytes:3517 (3.5 KB)

Now, on the RIOT side, send a UDP packet using:

    tftp get welcome.txt octet 1 fe80::4049:5fff:fe17:b3ae
