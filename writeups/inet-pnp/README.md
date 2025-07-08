# inet-pnp
Ever wondered why this works?

```
$ ping 127.1
PING 127.1 (127.0.0.1) 56(84) bytes of data.
64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.047 ms

$ ping 1.1
PING 1.1 (1.0.0.1) 56(84) bytes of data.
64 bytes from 1.0.0.1: icmp_seq=1 ttl=48 time=5.04 ms
```

## What's doing on here?

getaddrinfo(3):
> node specifies either a numerical network address (for IPv4, numbers-and-dots
> notation as supported by inet_aton(3); for IPv6, hexadecimal string format as
> supported by inet_pton(3))

inet(3):
> The address supplied in cp can have one of the following forms:
>
> 	a.b.c.d   Each of the four numeric parts specifies a byte of the address;
> 	the bytes are assigned in left-to-right order to produce the binary
> 	address.
> 
> 	a.b.c     Parts a and b specify the first two bytes of the binary address.
> 	Part  c  is  interpreted  as  a 16-bit  value  that defines the rightmost
> 	two bytes of the binary address.  This notation is suitable for specifying
> 	(outmoded) Class B network addresses.
>
> 	a.b       Part a specifies the first byte of the binary address.  Part b is
> 	interpreted as  a  24-bit  value that defines the rightmost three bytes of
> 	the binary address.  This notation is suitable for specifying (outmoded)
> 	Class A network addresses.
> 
> 	a         The valuqe a is interpreted as a 32-bit value that is stored
> 	directly into the binary address without any byte rearrangement.

inet_ntop(3):
> AF_INET src  points to a struct in_addr (in network byte order) which is
> 	converted to an IPv4 network address in the dotted-decimal format,
> 	"ddd.ddd.ddd.ddd".  The buffer dst must  be  at  least  INET_ADDRSTRLEN
> 	bytes long.
>
> AF_INET6 src  points  to  a  struct in6_addr (in network byte order) which is
> 	converted to a representation of this address in the most appropriate IPv6
> 	network address format for this address.   The  buffer  dst must be at least
> 	INET6_ADDRSTRLEN bytes long.

It's because `inet_ntop()` only accepts "ddd.ddd.ddd.ddd" format whereas
`inet_aton()` accepts "a", "a.b", ..., "a.b.c.d". Any program that calls
`getaddrinfo()` could potentially accept the `inet_aton()` style.

## The code

Build:
```sh
cc -std=c17 inet-pnp -o inet-pnp.c
```

Run:
```sh
# normal data
echo '1.2.3.4' | ./inet-pnp
# inet_aton() style
echo '127.1' | ./inet-pnp
```

### Result
Team "loose"(Windows, Linux, FreeBSD):

<pre>
pton: 1.2.3.4, gai: 1.2.3.4
pton: Bad message, <b>gai: 127.0.0.1</b>
</pre>

Team "strict"(OpenBSD):

<pre>
pton: 1.2.3.4, gai: 1.2.3.4
pton: Bad message, <b>gai: name or service is not known</b>
</pre>

## Takeaway
The `inet_aton()` style IPv4 address representation is not portable. Always use
'a.b.c.d' format. Use `inet_ntop()` over `inet_aton()` where possible.

## Some history

https://datatracker.ietf.org/doc/html/draft-main-ipaddr-text-rep-00
