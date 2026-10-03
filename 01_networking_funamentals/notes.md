# Networking Fundamentals

- A socket is a way to speak to other programs using standard Unix file
  descriptors.
- A file descriptor is a small non-negative integer the OS gives a process to
  refer to an open resource.

--------------------------------------------------------------------------------

## Two Types of Sockets

- Stream sockets (`SOCK_STREAM`):
  - Two-way connected communication streams.
  - Packets that are sent are guaranteed to be received in the same order they
    were sent.
  - E.g., `telnet`, `ssh`, `http`.
  - They achieve this high level of data transmission quality by using
    Transmission Control Protocol (TCP).
- Datagram sockets (`SOCK_DGRAM`):
  - Called connectionless sockets.
  - Packets can arrive out of order due to network routing.
  - Packets may not arrive at all (UDP provides no delivery guarantees; dropped
    packets are lost unless tracked at the application layer).
  - If a packet does arrive, the checksum guarantees its contents are intact.
  - You don't need to maintain a connection between sockets, but packets still
    require both an IP header and a UDP header (the UDP header provides
    source/destination port numbers).
  - They use User Datagram Protocol (UDP).
  - Higher-level protocols built on top of UDP can implement custom
    ACK/retransmission mechanisms, but standard raw UDP simply lets dropped
    packets drop.

--------------------------------------------------------------------------------

## Layered Network Model

### Data Encapsulation

- How data is sent across the wire.
- Data is wrapped in a Trivial File Transfer Protocol (TFTP) layer, which then
  gets UDP headers, then IP headers, and finally Ethernet headers (plus a frame
  check sequence trailer at the end).
- `[[[[[Data]TFTP]UDP]IP]Ethernet]` \<- Like this.

### The Layers

- In the layered network model (OSI 7-layer reference), the layers are:
  - Application \<- Where users interact with the network
  - Presentation
  - Session
  - Transport
  - Network
  - Data Link
  - Physical \<- The hardware
- Note: Modern socket programming primarily maps to the 4/5-layer TCP/IP model
  (where Application/Presentation/Session collapse into Application).
- For a `SOCK_STREAM`, you use `send()`; for a `SOCK_DGRAM`, you encapsulate the
  payload and use `sendto()`.
- The kernel handles building the Transport and Internet layers automatically.
- Your hardware (Ethernet, Wi-Fi, etc.) handles the Network Access layer.

--------------------------------------------------------------------------------

## IP Addresses

### IPv4 and IPv6

- Most IP addresses traditionally come from Internet Protocol version 4 (IPv4).
- These addresses use 4 bytes (32 bits) written in dotted-decimal form, e.g.,
  `192.0.2.111`.
- To combat IPv4 address exhaustion, IPv6 was introduced.
- IPv6 uses hex representation separated by colons, e.g.,
  `2001:0db8:c9d2:aee5:73e3:934a:a5ae:9551`.
- Dual-stack socket APIs can represent IPv4 addresses inside IPv6 structures
  using IPv4-mapped IPv6 syntax, prefixed with `::ffff:` (e.g.,
  `::ffff:192.0.2.111`).
- IPv4 and IPv6 are separate, non-interoperable network protocols; you cannot
  arbitrarily convert IPv4 to IPv6 for internet routing simply by padding bytes.

### Subnets

- Splitting up the bits/bytes of an IP address to assign specific functions to
  different sections.
- E.g., with IP address `192.0.2.12`, using a `/24` mask means the first three
  bytes identify the network, and the fourth byte identifies the host (host `12`
  on network `192.0.2.0`).
- The network portion is determined by the netmask (or CIDR prefix length),
  which is bitwise-ANDed with the IP address to extract the network identifier:
  - E.g., netmask `255.255.255.0` (`/24`).
  - `192.0.2.12 AND 255.255.255.0` -> `192.0.2.0`.
- Fixed byte-aligned class masks were too rigid for the internet, leading to
  modern Classless Inter-Domain Routing (CIDR).

--------------------------------------------------------------------------------

## Port Numbers

- Port numbers are used by TCP and UDP to multiplex connections to specific
  applications.
- A port number is a 16-bit unsigned integer (0 to 65535).
- Analogy: If the IP address is the street address of a hotel, the port number
  is the room number.
- Different network services run on different default ports (e.g., HTTP is port
  80, HTTPS is 443, SSH is 22).

--------------------------------------------------------------------------------

## Byte Order

- Multi-byte numbers can be stored in different byte orders in memory depending
  on the CPU architecture.
- **Big-endian (network byte order):** Stores the most significant byte first
  (e.g., hex `0xb34f` is stored as `b3 4f`).
- **Little-endian (host byte order on x86/ARM/Intel):** Stores the least
  significant byte first (e.g., hex `0xb34f` is stored as `4f b3`).
- Network protocols require data to be transmitted in **network byte order
  (big-endian)**.
- Functions are used to convert values between host and network byte orders to
  guarantee cross-platform compatibility:
  - `htons()`: Host to network short (16-bit, e.g., ports).
  - `htonl()`: Host to network long (32-bit, e.g., IPv4 addresses).
  - `ntohs()`: Network to host short.
  - `ntohl()`: Network to host long.

--------------------------------------------------------------------------------

## Structs

- Sockets use specialized C structures for address and connection information.
- A socket descriptor itself is represented as a plain integer (`int`).

### `struct addrinfo`

Used to prepare socket address structures for creation, as well as hostname and
service resolution via `getaddrinfo()`.

```c
struct addrinfo {
    int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
    int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
    int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
    int              ai_protocol;  // 0 for auto, or IPPROTO_TCP / IPPROTO_UDP
    size_t           ai_addrlen;   // Size of ai_addr in bytes
    struct sockaddr *ai_addr;      // Pointer to struct sockaddr
    char            *ai_canonname; // Full canonical hostname
    struct addrinfo *ai_next;      // Pointer to next node in linked list
};
```

### `struct sockaddr_in`

IPv4-specific socket address structure (easily castable to `struct sockaddr`).

```c
struct sockaddr_in {
    short int          sin_family; // AF_INET
    unsigned short int sin_port;   // Port number in Network Byte Order (htons)
    struct in_addr     sin_addr;   // IPv4 address structure
    unsigned char      sin_zero[8]; // Padding to match size of struct sockaddr (zero out with memset)
};

struct in_addr {
    uint32_t s_addr; // 32-bit IPv4 address in Network Byte Order
};
```

### `struct sockaddr_storage`

A structure large enough to hold both IPv4 (`sockaddr_in`) and IPv6
(`sockaddr_in6`) structures, aligned to prevent memory issues.

```c
struct sockaddr_storage {
    sa_family_t ss_family; // Address family (AF_INET or AF_INET6)
    char        __ss_pad1[_SS_PAD1SIZE];
    int64_t     __ss_align;
    char        __ss_pad2[_SS_PAD2SIZE];
};
```

Usage: Inspect `ss_family` first to check if the address is `AF_INET` or
`AF_INET6`, then cast the `sockaddr_storage` pointer to `struct sockaddr_in*` or
`struct sockaddr_in6*` accordingly.

--------------------------------------------------------------------------------

## More on IP

Functions exist to convert IP addresses between text strings and binary
representations:

- `inet_pton()` (presentation to network): Converts a dotted-decimal/hex string
  (e.g., `"192.0.2.1"`) into binary network byte order (`in_addr` / `in6_addr`).
- `inet_ntop()` (network to presentation): Converts binary address structures
  back into human-readable strings.
