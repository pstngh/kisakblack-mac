// sys_net.cpp — UDP networking for Linux and macOS: the IP-socket half of
// src/win32/win_net.cpp (socket setup, packet send/receive, address parsing, LAN
// checks) on BSD sockets. The SOCKS proxy, the client socket pool and the remote
// script-debug sockets are not ported; their entry points stay no-ops in
// sys_platform.cpp.
#include <win32/win_net.h>
#include <universal/q_shared.h>
#include <universal/dvar.h>
#include <qcommon/net_chan_mp.h>
#include <qcommon/common.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

#include "../sdl/sdl_mainthread.h"  // Sys_ServiceMainThreadWork (NET_Sleep)

namespace {
const dvar_t *ip;
const dvar_t *port;
const dvar_t *net_noudp;

int numIP;
unsigned __int8 localIP[16][4];
int networkingEnabled;
int ip_socket = -1;
}

const char *NET_ErrorString() { return strerror(errno); }

// The main thread's idle waits (R_BeginRegistration waiting for the render thread)
// run here, so it also runs work the render thread posted for it (macOS windows).
void NET_Sleep(unsigned int msec) { Sys_ServiceMainThreadWork(); if (msec) usleep(msec * 1000u); }

// A dotted IPv4 address or a host name; the port stays 0.
int Sys_StringToSockaddr(const char *s, sockaddr *sadr) {
    sockaddr_in *sin = (sockaddr_in *)sadr;
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    if (inet_pton(AF_INET, s, &sin->sin_addr) == 1)
        return 1;
    addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(s, nullptr, &hints, &res) || !res)
        return 0;
    sin->sin_addr = ((sockaddr_in *)res->ai_addr)->sin_addr;
    freeaddrinfo(res);
    return 1;
}

void SockadrToNetadr(sockaddr *s, netadr_t *a) {
    if (s->sa_family == AF_INET) {
        const sockaddr_in *sin = (const sockaddr_in *)s;
        a->type = NA_IP;
        memcpy(a->ip, &sin->sin_addr.s_addr, 4);
        a->port = sin->sin_port;
        a->addrHandleIndex = 0;
    }
}

static void NetadrToSockadr(const netadr_t *a, sockaddr_in *s) {
    memset(s, 0, sizeof(*s));
    s->sin_family = AF_INET;
    s->sin_port = a->port;
    if (a->type == NA_BROADCAST)
        s->sin_addr.s_addr = INADDR_BROADCAST;
    else
        memcpy(&s->sin_addr.s_addr, a->ip, 4);
}

int Sys_StringToAdr(const char *s, netadr_t *a) {
    sockaddr_in sadr;
    if (!Sys_StringToSockaddr(s, (sockaddr *)&sadr))
        return 0;
    SockadrToNetadr((sockaddr *)&sadr, a);
    return 1;
}

int Sys_GetPacket(netadr_t *net_from, msg_t *net_message) {
    if (ip_socket < 0)
        return 0;
    for (;;) {
        sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t ret = recvfrom(ip_socket, net_message->data, net_message->maxsize, 0, (sockaddr *)&from, &fromlen);
        if (ret < 0) {
            // ECONNREFUSED: an ICMP port-unreachable for an earlier send (Linux),
            // Winsock's WSAECONNRESET.
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNREFUSED && errno != EINTR)
                Com_PrintError(16, "NET_GetPacket: %s\n", NET_ErrorString());
            return 0;
        }
        SockadrToNetadr((sockaddr *)&from, net_from);
        net_message->readcount = 0;
        if (ret != net_message->maxsize) {
            net_message->cursize = (int)ret;
            return 1;
        }
        Com_Printf(16, "Oversize packet from %s\n", NET_AdrToString(*net_from));
    }
}

char Sys_SendPacket(unsigned int length, unsigned __int8 *data, netadr_t to) {
    if (to.type != NA_BROADCAST && to.type != NA_IP)
        Com_Error(ERR_FATAL, "Sys_SendPacket: bad address type");
    if (ip_socket < 0)
        return 1;
    sockaddr_in addr;
    NetadrToSockadr(&to, &addr);
    if (sendto(ip_socket, data, length, 0, (sockaddr *)&addr, sizeof(addr)) >= 0)
        return 1;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 1;
    // Broadcasts can fail without a route (WSAEADDRNOTAVAIL on Windows).
    if (to.type == NA_BROADCAST && (errno == EADDRNOTAVAIL || errno == ENETUNREACH || errno == EHOSTUNREACH))
        return 1;
    Com_PrintError(16, "Sys_SendPacket: %s\n", NET_ErrorString());
    return 0;
}

bool Sys_IsLANAddress_IgnoreSubnet(netadr_t adr) {
    if (adr.type == NA_LOOPBACK || adr.type == NA_BOT)
        return 1;
    if (adr.type != NA_IP)
        return 0;
    if (adr.ip[0] == 10 || adr.ip[0] == 127)
        return 1;
    if (adr.ip[0] == 169 && adr.ip[1] == 254)
        return 1;
    if (adr.ip[0] == 172 && (adr.ip[1] & 0xF0) == 0x10)
        return 1;
    return adr.ip[0] == 192 && adr.ip[1] == 168;
}

int Sys_IsLANAddress(netadr_t adr) {
    if (Sys_IsLANAddress_IgnoreSubnet(adr))
        return 1;
    for (int i = 0; i < numIP; ++i) {
        if (adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] && adr.ip[2] == localIP[i][2])
            return 1;
    }
    return 0;
}

void Sys_ShowIP() {
    for (int i = 0; i < numIP; ++i)
        Com_Printf(16, "IP: %i.%i.%i.%i\n", localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3]);
}

// Windows resolves its own host name; the interface list is the POSIX equivalent.
static void NET_GetLocalAddress() {
    ifaddrs *list = nullptr;
    numIP = 0;
    if (getifaddrs(&list))
        return;
    for (ifaddrs *ifa = list; ifa && numIP < 16; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET || !(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK))
            continue;
        memcpy(localIP[numIP], &((sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr, 4);
        ++numIP;
    }
    freeifaddrs(list);
    Sys_ShowIP();
}

static int NET_IPSocket(const char *net_interface, int port) {
    if (net_interface)
        Com_Printf(16, "Opening IP socket: %s:%i\n", net_interface, port);
    else
        Com_Printf(16, "Opening IP socket: localhost:%i\n", port);
    int newsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (newsocket < 0) {
        if (errno != EAFNOSUPPORT)
            Com_PrintWarning(16, "WARNING: UDP_OpenSocket: socket: %s\n", NET_ErrorString());
        return -1;
    }
    int flags = fcntl(newsocket, F_GETFL, 0);
    if (flags < 0 || fcntl(newsocket, F_SETFL, flags | O_NONBLOCK) < 0) {
        Com_PrintWarning(16, "WARNING: UDP_OpenSocket: fcntl O_NONBLOCK: %s\n", NET_ErrorString());
        close(newsocket);
        return -1;
    }
    int i = 1;
    if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) < 0) {
        Com_PrintWarning(16, "WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString());
        close(newsocket);
        return -1;
    }
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    if (net_interface && *net_interface && I_stricmp(net_interface, "localhost"))
        Sys_StringToSockaddr(net_interface, (sockaddr *)&address);
    address.sin_family = AF_INET;
    address.sin_port = port == -1 ? 0 : htons((unsigned short)port);
    if (bind(newsocket, (sockaddr *)&address, sizeof(address)) < 0) {
        Com_PrintWarning(16, "WARNING: UDP_OpenSocket: bind: %s\n", NET_ErrorString());
        close(newsocket);
        return -1;
    }
    return newsocket;
}

void NET_OpenIP() {
    ip = _Dvar_RegisterString("net_ip", "localhost", 0x20u, "Network IP Address");
    port = _Dvar_RegisterInt("net_port", 3074, 0, 0xFFFF, 0x20u, "Network port");

    for (int i = 0; i < 10; ++i) {
        ip_socket = NET_IPSocket(ip->current.string, i + port->current.integer);
        if (ip_socket >= 0) {
            Dvar_SetInt((dvar_s *)port, i + port->current.integer);
            NET_GetLocalAddress();
            return;
        }
    }
    Com_PrintWarning(16, "WARNING: Couldn't allocate IP port\n");
}

bool NET_GetDvars() {
    bool modified = net_noudp && net_noudp->modified;
    net_noudp = _Dvar_RegisterBool("net_noudp", 0, 0x21u, "Disable UDP");
    return modified;
}

void NET_Config(int enableNetworking) {
    bool modified = NET_GetDvars();
    if (net_noudp->current.enabled)
        enableNetworking = 0;
    if (enableNetworking == networkingEnabled && !modified)
        return;
    bool stop = enableNetworking == networkingEnabled ? enableNetworking != 0 : !enableNetworking;
    bool start = enableNetworking != 0;
    networkingEnabled = enableNetworking;
    if (stop && ip_socket >= 0) {
        close(ip_socket);
        ip_socket = -1;
    }
    if (start && !net_noudp->current.enabled)
        NET_OpenIP();
}

void NET_Restart() { NET_Config(networkingEnabled); }

void NET_Init() {
    NET_GetDvars();
    NET_Config(1);
}
