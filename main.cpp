
#include <stdio.h>
#include "inc/common.h"
#include "inc/tinyxml2.h"

using std::string;
struct status_line {
    string line;
    string method;
    string scm;
    string hostname;
    int port;
    string path;
    string version;
};

in_addr_t fake_ip, dns_ip;
double avg;  // average throughout
double alpha;
int dns_port;
double T_cur;
std::vector<int> Bitrate;  // valid Bitrates
int _Thread_local contentLength;

int getBitrate() {
    int bitrate = Bitrate[0];
    for (auto i : Bitrate)
        if (i * 1.5 < avg) bitrate = i;
    return bitrate;
}

int parseline(char *line, struct status_line *status);

int send_request(rio_t *rio, char *buf, struct status_line *status,
                 int serverfd, int clientfd);

int transmit(int readfd, int writefd, char *buf, int *count);

int interrelate(int serverfd, int clientfd, char *buf, int idling);

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
        fprintf(stderr, "usage: %s <log> <alpha> <listen-port> <fake-ip>
    <dns-ip> <dns-port> [<www-ip>]\n", argv[0]);
        exit(1);
    }
     */
    signal(SIGPIPE, SIG_IGN);

    char *logfile = argv[1];
    sscanf(argv[2], "%lf", &alpha);  // parse alpha
    int listen_port = atoi(argv[3]);
    int listenfd = Open_listenfd(listen_port);
    fake_ip = inet_addr(argv[4]);
    dns_ip = inet_addr(argv[5]);
    dns_port = atoi(argv[6]);

    FILE *f;
#ifdef VM
    f = fopen("/var/www/vod/big_buck_bunny.f4m", "rb");
#else
    f = fopen(
            "/Users/wjmzbmr/Library/Mobile "
                    "Documents/com~apple~CloudDocs/Documents/course/network/Network-Lab3/"
                    "data/big_buck_bunny.f4m",
            "rb");
#endif
    tinyxml2::XMLDocument doc;
    doc.LoadFile(f);
    if (doc.ErrorID()) {
        // load f4m file failed!!
        std::cerr << "big_buck_bunny.f4m reading error" << std::endl;
    }

    Bitrate.clear();
    tinyxml2::XMLElement *media =
            doc.FirstChildElement("manifest")->FirstChildElement("media");
    while (media) {
        int bitrate = 0;
        media->QueryIntAttribute("bitrate", &bitrate);
        Bitrate.push_back(bitrate);
        media = media->NextSiblingElement("media");
    }
    std::cout << "Avaliable bitrates:" << std::endl;
    std::for_each(Bitrate.begin(), Bitrate.end(),
                  [](int x) { std::cout << x << " "; });
    std::cout << std::endl;

    avg = Bitrate[0];

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
    status->line = line;

    // todo
    //把这段弄好看一点？

    char _line[MAXLINE];
    char _method[20];
    char _scm[20];
    char _hostname[MAXLINE];
    char _path[MAXLINE];
    char _version[20];

    if (sscanf(line, "%s %[a-z]://%[^/]%s %s", _method, _scm, _hostname, _path,
               _version) != 5) {
        if (sscanf(line, "%s %s %s", _method, _hostname, _version) != 3)
            return -1;
        status->method = _method;
        status->hostname = _hostname;
        status->version = _version;
        status->scm = status->path = "";
    } else {
        status->method = _method;
        status->hostname = _hostname;
        status->version = _version;
        status->path = _path;
        status->scm = _scm;
        status->scm += "://";
    }
    auto pos = status->hostname.find(':');
    if (pos != std::string::npos) {
        status->port = std::stoi(status->hostname.substr(pos + 1));
        status->hostname = status->hostname.substr(0, pos);
    }
    if (status->path == "") status->path = "/";
    //很僵硬的一点是，虚拟机的gcc是4.8.2的。。
    //<del>所以regex_replace的第三个参数一定要明确是string还是别的什么，不然编译不过</del>
    //wtf..gcc 4.8.2 根本没法用regex_replace
    //说不出话
    /*
#ifndef VM
    status->path =
            std::regex_replace(status->path, std::regex("big_buck_bunny.f4m"),
                               string("big_buck_bunny_nolist.f4m"));
    auto b = getBitrate();
    status->path = std::regex_replace(status->path, std::regex("1000"),
                                      std::to_string(b));
#else
     */
    pos = status->path.find("big_buck_bunny.f4m");
    if (pos != std::string::npos) {
        status->path.replace(pos, strlen("big_buck_bunny.f4m"), "big_buck_bunny_nolist.f4m");
    }
    pos = status->path.find("1000");
    auto bitrate = getBitrate();
    if (pos != std::string::npos) {
        status->path.replace(pos, strlen("1000"), std::to_string(bitrate));
    }

//#endif

    return 0;
}

int send_request(rio_t *rio, char *buf, struct status_line *status,
                 int serverfd, int clientfd) {
    int len;
    if (status->method != "CONNECT") {
        len = snprintf(buf, MAXLINE,
                       "%s %s %s\r\n"
                               "Connection: close\r\n",
                       status->method.c_str(), status->path.c_str(),
                       status->version.c_str());
        if ((len = rio_writen(serverfd, buf, len)) < 0) return len;
        while (len != 2) {
            if ((len = rio_readlineb(rio, buf, MAXLINE)) < 0) return len;
            if (memcmp(buf, "Proxy-Connection: ", 18) == 0 ||
                memcmp(buf, "Connection: ", 12) == 0)
                continue;
            if ((len = rio_writen(serverfd, buf, len)) < 0) return len;
        }
        if (rio->rio_cnt &&
            (len = rio_writen(serverfd, rio->rio_bufptr, rio->rio_cnt)) < 0)
            return len;
        return 20;
    } else {
        len = snprintf(buf, MAXLINE, "%s 200 OK\r\n\r\n",
                       status->version.c_str());
        if ((len = rio_writen(clientfd, buf, len)) < 0) return len;
        return 300;
    }
}

int transmit(int readfd, int writefd, char *buf, int *count){
    int len;
    char*pos;
    if ((len = read(readfd, buf, MAXBUF)) > 0) {
        *count = 0;
        if((pos=strstr(buf,"Content-Length:"))){
            sscanf(pos,"Content-Length:%d",&contentLength);
        }
        len = rio_writen(writefd, buf, len);
    }
    return len;
}

int interrelate(int serverfd, int clientfd, char *buf, int idling) {
    int count = 0;
    int nfds = (serverfd > clientfd ? serverfd : clientfd) + 1;
    int flag;
    fd_set rlist, xlist;
    FD_ZERO(&rlist);
    FD_ZERO(&xlist);

    int _count = 0;

    while (1) {
        _count++;
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
                ((flag = transmit(serverfd, clientfd, buf, &count)) < 0))
                return flag;
            if (flag == 0) break;
            if (FD_ISSET(clientfd, &rlist)) {
                if ((flag = transmit(clientfd, serverfd, buf, &count)) < 0)
                    return flag;
                //printf("interrelate sending %.*s\n", flag, buf);
            }
            if (flag == 0) break;
        }
        if (count >= idling) break;
    }

    std::cerr << "interrelate times: " << _count << std::endl;
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
        else if ((serverfd = open_clientfd(const_cast<char *>(status.hostname.c_str()),
                                        status.port)) < 0);  // log(open_clientfd);
        else {
            // modify request..
            auto req_start = std::chrono::system_clock::now();

            if ((flag = send_request(&rio, buf, &status, serverfd, clientfd)) <
                0);  //    log(send_request);
            else if (interrelate(serverfd, clientfd, buf, flag) < 0);
            else {
                auto req_end = std::chrono::system_clock::now();
                std::chrono::duration<double> diff = req_end - req_start;
                double duration = diff.count(); //seconds
                double T_new = contentLength/duration;
                avg = alpha*avg +(1-alpha)*T_new;
              //  double T_new = ;
                //𝑇 = 𝛼𝑇 + (1 − 𝛼)𝑇 (1) 𝑐𝑢𝑟𝑟𝑒𝑛𝑡 𝑛𝑒𝑤 𝑐𝑢𝑟𝑟𝑒𝑛𝑡
            }
            //   log(interrelate);
            close(serverfd);
        }
    }
    close(clientfd);
    return NULL;
}
