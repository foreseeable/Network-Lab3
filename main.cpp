
#include <stdio.h>
#include "inc/common.h"
#include "inc/tinyxml2.h"
// TODO:
struct status_line {
    char line[MAXLINE];
    char method[20];
    char scm[20];
    char hostname[MAXLINE];
    int port;
    char path[MAXLINE];
    char version[20];
};

in_addr_t fake_ip, dns_ip;
double avg; // average throughout
double alpha;
int dns_port;
std::vector<int> Bitrate; //valid Bitrates

int getBitrate() {
    int bitrate = Bitrate[0];
    for (auto i:Bitrate)
        if (i * 1.5 < avg)
            bitrate = i;
    return bitrate;
}



int parseline(char *line, struct status_line *status);

int send_request(rio_t *rio, char *buf, struct status_line *status,
                 int serverfd, int clientfd);

int transmit(int readfd, int writefd, char *buf, int *count
);

int interrelate(int serverfd, int clientfd, char *buf, int idling
);

void *proxy(void *vargp);


int main(int argc, char *argv[]) {
    /*
    namespace po = boost::program_options;
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("compression", po::value<int>(), "set compression level");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        if (vm.count("compression")) {
            std::cout << "Compression level was set to "
                 << vm["compression"].as<double>() << "."<<std::endl;
        } else {
            std::cout << "Compression level was not set."<<std::endl;
        }

    }
    catch(std::exception& e){
        std::cerr << "error: " << e.what() << std::endl;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n" <<std::endl;
    }
    if (argc != 7 && argc != 8) {
        fprintf(stderr, "usage: %s <log> <alpha> <listen-port> <fake-ip> <dns-ip> <dns-port> [<www-ip>]\n", argv[0]);
        exit(1);
    }
     */
    signal(SIGPIPE, SIG_IGN);

    char *logfile = argv[1];
    sscanf(argv[2], "%lf", &alpha);   //parse alpha
    int listen_port = atoi(argv[3]);
    int listenfd = Open_listenfd(listen_port);
    fake_ip = inet_addr(argv[4]);
    dns_ip = inet_addr(argv[5]);
    dns_port = atoi(argv[6]);


    FILE *f;
#ifdef VM
    f = fopen("/var/www/vod/big_buck_bunny.f4m", "rb");
#elseif
    f = fopen("big_buck_bunny.f4m");
#endif
    tinyxml2::XMLDocument doc;
    doc.LoadFile(f);
    if(doc.ErrorID()){
        //load f4m file failed!!
        std::cerr <<"big_buck_bunny.f4m reading error" << std::endl;
    }

    while ("serve forever") {
        struct sockaddr clientaddr;
        socklen_t addrlen = sizeof clientaddr;
        int *clientfd = (int *) Malloc(sizeof(int));
        do
            *clientfd = accept(listenfd, &clientaddr, &addrlen);
        while (*clientfd < 0);

        pthread_t tid;
        Pthread_create(&tid, NULL, proxy, clientfd);
    }
}



int parseline(char *line, struct status_line *status) {
    status->port = 80;
    strcpy(status->line, line);

    if (sscanf(line, "%s %[a-z]://%[^/]%s %s", status->method, status->scm,
               status->hostname, status->path, status->version) != 5) {
        if (sscanf(line, "%s %s %s", status->method, status->hostname,
                   status->version) != 3)
            return -1;
        *status->scm = *status->path = 0;
    } else
        strcat(status->scm, "://");

    char *pos = strchr(status->hostname, ':');
    if (pos) {
        *pos = 0;
        status->port = atoi(pos + 1);
    }
    return 0;
}

int send_request(rio_t *rio, char *buf, struct status_line *status,
                 int serverfd, int clientfd) {
    int len;
    if (strcmp(status->method, "CONNECT")) {
        len = snprintf(buf, MAXLINE,
                       "%s %s %s\r\n"
                               "Connection: close\r\n",
                       status->method, *status->path ? status->path : "/",
                       status->version);
        printf("%s", buf);
        if ((len = rio_writen(serverfd, buf, len)) < 0) return len;
        while (len != 2) {
            if ((len = rio_readlineb(rio, buf, MAXLINE)) < 0) return len;
            if (memcmp(buf, "Proxy-Connection: ", 18) == 0 ||
                memcmp(buf, "Connection: ", 12) == 0)
                continue;
            printf("%s", buf);
            if ((len = rio_writen(serverfd, buf, len)) < 0) return len;
        }
        if (rio->rio_cnt &&
            (len = rio_writen(serverfd, rio->rio_bufptr, rio->rio_cnt)) < 0)
            return len;
        return 20;
    } else {
        len = snprintf(buf, MAXLINE, "%s 200 OK\r\n\r\n", status->version);
        if ((len = rio_writen(clientfd, buf, len)) < 0) return len;
        return 300;
    }
}

int transmit(int readfd, int writefd, char *buf, int *count
) {
    int len;
    if ((len = read(readfd, buf, MAXBUF)) > 0) {
        *count = 0;
        len = rio_writen(writefd, buf, len);
    }
    return len;
}

int interrelate(int serverfd, int clientfd, char *buf, int idling
) {
    int count = 0;
    int nfds = (serverfd > clientfd ? serverfd : clientfd) + 1;
    int flag;
    fd_set rlist, xlist;
    FD_ZERO(&rlist);
    FD_ZERO(&xlist);


    while (1) {
        count++;

        FD_SET(clientfd, &rlist);
        FD_SET(serverfd, &rlist);
        FD_SET(clientfd, &xlist);
        FD_SET(serverfd, &xlist);

        struct timeval timeout = {2L, 0L};
        if ((flag = select(nfds, &rlist, NULL, &xlist, &timeout)) < 0)
            return flag;
        if (flag) {
            if (FD_ISSET(serverfd, &xlist) || FD_ISSET(clientfd, &xlist)) break;
            if (FD_ISSET(serverfd, &rlist) &&
                ((flag = transmit(serverfd, clientfd, buf, &count
                )) < 0))
                return flag;
            if (flag == 0) break;
            if (FD_ISSET(clientfd, &rlist)) {
                if ((flag = transmit(clientfd, serverfd, buf, &count
                )) < 0)
                    return flag;
                printf("%.*s\n", flag, buf);
            }
            if (flag == 0) break;
        }
        if (count >= idling) break;
    }
    return 0;
}

void *proxy(void *vargp) {
    Pthread_detach(Pthread_self());

    int serverfd;
    int clientfd = *(int *) vargp;
    free(vargp);

    rio_t rio;
    rio_readinitb(&rio, clientfd);

    struct status_line status;

    char buf[MAXLINE];
    int flag;


    if ((flag = rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        printf("%d\n", flag);
        printf("%.*s\n", flag, buf);
        if (parseline(buf, &status) < 0)
            fprintf(stderr, "parseline error: '%s'\n", buf);
        else if ((serverfd = open_clientfd(status.hostname, status.port)) < 0)
           ;// log(open_clientfd);
        else {
            //modify request..
            if ((flag = send_request(&rio, buf, &status, serverfd, clientfd)) < 0)
            ;//    log(send_request);
            else if (interrelate(serverfd, clientfd, buf, flag) < 0)
             ;//   log(interrelate);
            close(serverfd);
        }
    }
    close(clientfd);
    return NULL;
}