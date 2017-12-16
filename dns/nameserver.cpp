//
// Created by f on 2017/12/17.
//

#include "../inc/common.h"

#define LISTENQ 1024


void*dns(void*);

int main(int argc,char**argv){
    if (argc != 6 && argc != 7) {
        fprintf(stderr, "usage: %s [-r] <log> <ip> <port> <servers> <LSAs>\n", argv[0]);
        exit(1);
    }else{
        if(argc==6&&strcmp(argv[1],"-r")){
            fprintf(stderr, "usage: %s [-r] <log> <ip> <port> <servers> <LSAs>\n", argv[0]);
            exit(1);
        }
    }
    char*logfile;
    int port;
    bool roundrobin=false;
    FILE*servers;
    FILE*LSAs;
    in_addr_t ip;


    if(argc == 6) {
        logfile = argv[1];
        ip = inet_addr(argv[2]);
        port = atoi(argv[3]);
        servers = fopen(argv[4],"r");
        LSAs =fopen(argv[5],"r");
    }else{
        roundrobin = true;
        logfile = argv[2];
        ip = inet_addr(argv[3]);
        port = atoi(argv[4]);
        servers = fopen(argv[5],"r");
        LSAs =fopen(argv[6],"r");
    }
    int listenfd;
    if((listenfd=open_listenfd(ip,port))<0){
        std::cerr << "open listenfd failed"<< std::endl;
    }

    while ("serve forever") {
        struct sockaddr clientaddr;
        socklen_t addrlen = sizeof clientaddr;
        int *clientfd = (int *) malloc(sizeof(int));
        do
            *clientfd = accept(listenfd, &clientaddr, &addrlen);
        while (*clientfd < 0);

        pthread_t tid;
        Pthread_create(&tid, NULL, dns, clientfd);
    }

}

void *dns(void *vargp) {
    Pthread_detach(Pthread_self());
    int clientfd = *(int *) vargp;
    free(vargp);
    rio_t rio;
    rio_readinitb(&rio, clientfd);
    char buf[MAXLINE];
    int flag;
}
