//
// Created by f on 2017/12/17.
//

#include <map>
#include <queue>
#include "inc/common.h"

void *dns(void *);

char *logfile;
int port;
bool roundrobin = false;
FILE *servers;
FILE *LSAs;
in_addr_t ip;
std::vector<unsigned int> server_list;
unsigned int server_cur;
std::map<std::string, std::vector<std::string>> G; //graph


in_addr_t getBestServer(in_addr_t client) {
    in_addr client_addr;
    client_addr.s_addr = client;
    std::string client_ip(inet_ntoa(client_addr));

    std::queue<std::string> Q;
    std::map<std::string, int> dis;
    //边权都是1的dijkstra真的不就是bfs吗

    for (auto it = G.begin(); it != G.end(); ++it) {
        dis[it->first] = -1;
    }
    dis[client_ip] = 0;

    //std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, std::greater<std::pair<int, std::string>>> Q;
    Q.push(client_ip);
    for (; !Q.empty(); Q.pop()) {
        std::string u = Q.front();
        int disu=dis[u];
        for(auto it = G[u].begin();it!=G[u].end();++it){
            std::string v=*it;
            if(!dis.count(v)){
                std::cerr<<"dijkstra failed!!!"<<std::endl;
                exit(-1);
            }
            int disv=dis[v];
            if(disv==-1||disv>disu+1){
                dis[v]=disu+1;
                Q.push(v);
            }
        }
    }
    auto best=server_list[0];
    int bestdis = -1;
    for(auto it=server_list.begin();it!=server_list.end();++it){
        in_addr server_addr;
        server_addr.s_addr=*it;
        std::string server_ip(inet_ntoa(server_addr));
        if(!dis.count(server_ip)){
            std::cerr<<"dijkstra failed!!!"<<std::endl;
            exit(-1);
        }
        if(dis[server_ip]==-1){
            continue;
        }
        if(bestdis==-1||bestdis>dis[server_ip]){
            best=*it;
            bestdis=dis[server_ip];
        }
    }
    if(bestdis==-1){
        std::cerr<<"dijkstra failed!!!"<<std::endl;
        exit(-1);
    }
    return best;
}

void genMessage(unsigned char *buf, const char *query, int fromclient, sockaddr addr) {
    DNSHeader *dns = (DNSHeader *) buf;
    dns->AA = 1;
    if (strcmp(query, "video.pku.edu.cn")) {
        dns->RCODE = 3;
        return;
    }
    unsigned char *QNAME = buf + (sizeof(DNSHeader)), *QNAME_p = QNAME;

    while (*QNAME_p) {
        QNAME_p += (*QNAME_p) + 1;
    }
    QNAME_p++;
    int tot = QNAME_p - QNAME;
    QNAME_p += 4;

    unsigned char *RESNAME = QNAME_p;
    for (int i = 0; i < tot; i++)
        RESNAME[i] = QNAME[i];
    QNAME_p = RESNAME + tot;
    DNSRecord *rec = (DNSRecord *) QNAME_p;
    rec->TYPE = 1;
    rec->CLASS = 1;
    rec->TTL = 0;
    rec->RDLENGTH = 4;
    if (roundrobin) {
        in_addr_t *ip = (in_addr_t *) QNAME_p + sizeof(DNSRecord);
        *ip = htonl(server_list[server_cur]);
        server_cur = (server_cur + 1) % server_list.size();
    } else {
        sockaddr_in *add = (sockaddr_in *) &addr;
        in_addr_t *ip = (in_addr_t *) QNAME_p + sizeof(DNSRecord);
        in_addr_t clientip = add->sin_addr.s_addr;
        *ip = htonl(getBestServer(clientip));
    }
}

void loadServers() {
    char buf[MAXLINE];
    while (fscanf(servers, "%s", buf) == 1) {
        server_list.push_back(inet_addr(buf));
    }
    server_cur = 0;
}

void loadLSAs() {
    char buf[MAXLINE];
    char nodename[MAXLINE];
    int round;
    std::map<std::string, std::pair<int, std::string>> lsarec;
    G.clear();
    while (fgets(buf, MAXLINE, LSAs)) {
        sscanf(buf,"%s%d", nodename, &round);
        std::string current_node = nodename, adjacent;
        sscanf(buf,"%s", nodename);
        adjacent = nodename;
        if (!lsarec.count(current_node) || lsarec[current_node].first < round) {
            lsarec[current_node] = std::make_pair(round, adjacent);
        }
    }
    for (auto it = lsarec.begin(); it != lsarec.end(); ++it) {
        std::string current_node = it->first;
        std::string adjacent = it->second.second;
        while (adjacent.length() > 0) {
            auto pos = adjacent.find(',');
            std::string node;
            if (pos != std::string::npos) {
                node = adjacent.substr(0, pos);
                adjacent = adjacent.substr(pos + 1);
            } else {
                node = adjacent;
                adjacent = "";
            }
            G[current_node].push_back(node);
        }
    }

}

int main(int argc, char **argv) {
    if (argc != 6 && argc != 7) {
        fprintf(stderr, "usage: %s [-r] <log> <ip> <port> <servers> <LSAs>\n", argv[0]);
        exit(1);
    } else {
        if (argc == 6 && strcmp(argv[1], "-r")) {
            fprintf(stderr, "usage: %s [-r] <log> <ip> <port> <servers> <LSAs>\n", argv[0]);
            exit(1);
        }
    }


    if (argc == 6) {
        logfile = argv[1];
        ip = inet_addr(argv[2]);
        port = atoi(argv[3]);
        servers = fopen(argv[4], "r");
        LSAs = fopen(argv[5], "r");
    } else {
        roundrobin = true;
        logfile = argv[2];
        ip = inet_addr(argv[3]);
        port = atoi(argv[4]);
        servers = fopen(argv[5], "r");
        LSAs = fopen(argv[6], "r");
    }
    loadServers();
    if (!roundrobin) {
        loadLSAs();
    }
    int listenfd;
    if ((listenfd = open_listenfd(ip, port)) < 0) {
        std::cerr << "open listenfd failed" << std::endl;
    }

    while ("serve forever") {
        struct sockaddr clientaddr;
        socklen_t addrlen = sizeof clientaddr;
        int *clientfd = (int *) malloc(sizeof(int) + addrlen);
        do
            *clientfd = accept(listenfd, &clientaddr, &addrlen);
        while (*clientfd < 0);
        *(sockaddr *) (clientfd + 1) = clientaddr;

        pthread_t tid;
        Pthread_create(&tid, NULL, dns, clientfd);
    }

}

void *dns(void *vargp) {
    Pthread_detach(Pthread_self());
    int clientfd = *(int *) vargp;
    sockaddr clientaddr = *(sockaddr *) ((int *) vargp + 1);
    free(vargp);
    rio_t rio;
    rio_readinitb(&rio, clientfd);
    unsigned char buf[MAXLINE];
    int flag;
    if ((flag = rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        unsigned char *QNAME = buf + (sizeof(DNSHeader)), *QNAME_p = QNAME;

        std::string hostname = "";
        int l = *(QNAME_p++);
        while (l) {
            hostname.append(1, *(QNAME_p++));
            l--;
            if (l == 0) {
                l = *(QNAME_p++);
                if (l)
                    hostname = hostname + '.';
            }
        }
        genMessage(buf, hostname.c_str(), 0, clientaddr);
        rio_writen(clientfd,buf,MAXLINE);
    }
    close(clientfd);
}
