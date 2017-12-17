//
// Created by f on 2017/12/17.
//
#include "common.h"
#include "mydns.h"
#include <cstdlib>


int dns_socket;

int init_mydns(const char *dns_ip, unsigned int dns_port, const char *client_ip) {
    //message = genMessage(query, 1)[0];
    if (dns_socket = socket(AF_INET, SOCK_DGRAM, 0) < 0)return -1;

    struct sockaddr_in localaddr;
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr(client_ip);
    localaddr.sin_port = 0;  // Any local port will do
    bind(dns_socket, (struct sockaddr *) &localaddr, sizeof(localaddr));
    struct sockaddr_in dns_addr;
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_addr.s_addr = inet_addr(dns_ip);
    dns_addr.sin_port = dns_port;
    return connect(dns_socket, (SA *) &dns_addr, sizeof dns_addr);
}

struct DNSHeader {
    unsigned int ID :16;
    unsigned int QR :1;
    unsigned int OPCODE :4;
    unsigned int AA :1;
    unsigned int TC :1;
    unsigned int RD :1;
    unsigned int RA :1;
    unsigned int Z :3;

    unsigned int RCODE :4;
    unsigned int QDCOUNT :16;
    unsigned int ANCOUNT :16;
    unsigned int NSCOUNT :16;
    unsigned int ARCOUNT :16;
};
struct DNSQuery {
    unsigned int QTYPE:16;
    unsigned int QCLASS:16;
};


int resolve(const char *node, const char *service,
            const struct addrinfo *hints, struct addrinfo **res) {
    unsigned char buf[MAXBUF];
    auto dns = (DNSHeader *) buf;
    dns->ID = (unsigned short) htons(getpid());
    dns->QR = 0; //This is a query
    dns->OPCODE = 0; //This is a standard query
    dns->AA = 0; //Not Authoritative
    dns->TC = 0; //This message is not truncated
    dns->RD = 0; //Recursion Desired
    dns->RA = 0; //Recursion not available! hey we dont have it (lol)
    dns->Z = 0;
    dns->RCODE = 0;
    dns->QDCOUNT = htons(1); //we have only 1 question
    dns->ANCOUNT = 0;
    dns->NSCOUNT = 0;
    dns->ARCOUNT = 0;

    std::vector<unsigned char> lens;
    int len = strlen(node);
    for (int i = 0, j; i < len; i = j + 1) {
        for (j = i; j < len; j++)
            if (node[j] == '.') {
                lens.push_back(j - i);
                break;
            }
        if (j == len)lens.push_back(j - i);
    }

    unsigned char *QNAME = buf + (sizeof (DNSHeader)), *QNAME_p = QNAME;
    *(QNAME_p++) = lens[0];
    for (int i = 0, j = 0; i < len; i++) {
        if (node[i] == '.') {
            j++;
            *(QNAME_p++) = lens[j];
        } else {
            *(QNAME_p++) = node[i];
        }
    }
    *(QNAME_p++) = 0;
    auto query = (DNSQuery *) QNAME_p;
    query->QTYPE = 1;
    query->QCLASS = 1;
    if (len = rio_writen(dns_socket, buf, MAXBUF) < 0)return -1;
    fd_set rlist, xlist;
    FD_ZERO(&rlist);
    FD_ZERO(&xlist);

    int nfds = dns_socket + 1;
    FD_SET(dns_socket, &rlist);
    FD_SET(dns_socket, &xlist);
    bool flag;

    struct timeval timeout = {2L, 0L};
    for(int _=0;_<2;_++) {
        if ((flag = select(nfds, &rlist, NULL, &xlist, &timeout)) < 0)
            return flag;
        if (flag) {
            if (FD_ISSET(dns_socket, &xlist)) return -1;
            if (FD_ISSET(dns_socket, &rlist)) {
                if ((len = read(dns_socket, buf, MAXBUF)) > 0) {
                    QNAME_p = buf + (sizeof(DNSHeader));
                    while(*QNAME_p){
                        QNAME_p+=(*QNAME_p)+1;
                    }
                    QNAME_p++;
                    //skip QTYPE QCLASS
                    QNAME_p+=4;
                    //now we are at RR

                    while(*QNAME_p){
                        QNAME_p+=(*QNAME_p)+1;
                    }
                    QNAME_p+=10;
                    //skip RR_TYPE RR_CLASS RR_TTL RR_DL
                    std::string ipstr="";
                    ipstr.append(std::to_string(*QNAME_p++));
                    ipstr.append(std::to_string(*QNAME_p++));
                    ipstr.append(std::to_string(*QNAME_p++));
                    ipstr.append(std::to_string(*QNAME_p++));
                    *res =new addrinfo;
                    (*res)->ai_family=AF_INET;
                    (*res)->ai_socktype = SOCK_STREAM; /* Open a connection */
                    (*res)->ai_protocol = 0;
                    (*res)->ai_addr = new sockaddr;
                    sockaddr_in*addr=(sockaddr_in*)(*res)->ai_addr;
                    addr->sin_family=AF_INET;
                    addr->sin_port=atoi(service);
                    inet_aton(ipstr.c_str(),&(addr->sin_addr));
                    (*res)->ai_addrlen = sizeof(sockaddr);
                    (*res)->ai_next = NULL;
                    (*res)->ai_canonname = NULL;
                    return 0;
                }
            }
        }
    }

    std::cerr << "name lookup time out!" << std::endl;
    return -1;
}


int mydns_freeaddrinfo(struct addrinfo *p) {
    delete p->ai_addr;
    delete p;
}
