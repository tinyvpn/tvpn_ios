#include "network_service.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <memory.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
//#include <sys/kern_control.h>
#include <sys/uio.h>
//#include <sys/sys_domain.h>
#include <netinet/ip.h>
#include <thread>
#include <os/log.h>
#include <mutex>

#include "log.h"
#include "fileutl.h"
#include "sockutl.h"
#include "ssl_client2.h"
#include "stringutl.h"
#include "http_client.h"
#include "sockhttp.h"
#include "sysutl.h"
#include "timeutl.h"
#include "obfuscation_utl.h"

const int BUF_SIZE = 4096*4;
int g_protocol = kSslType;
uint32_t g_private_ip;  // local host order
uint32_t g_private_ip_net;  // network order
std::string global_private_ip;
std::string global_default_gateway_ip;
std::string target_test_ip; //="159.65.226.184";
uint32_t target_test_ip32;
uint16_t target_test_port;
std::string web_server_ip = "www.tinyvpn.xyz";
//std::string web_server_ip = "104.131.79.62";
static uint32_t in_traffic, out_traffic;
static int g_isRun;
static int client_sock;
static int g_in_recv_tun;
static int g_in_recv_socket;
//std::mutex g_mutex;

//int premium = 2;
std::string g_user_name;// = "dudu@163.com";
std::string g_password;// = "123456";
std::string g_device_id;
static int firstOpen=0;
//int current_traffic = 0 ;  //bytes
uint32_t g_day_traffic;
uint32_t g_month_traffic;


typedef void(*socket_recv_callback_t)(uint8_t*, long, void*);
socket_recv_callback_t g_socketRecvCallback = NULL; //Declare a global variable..
void* g_target;

int network_service::init_vpn(std::string log_file)
{
    //log_file.erase(0, 7);
    log_file += "/tlog.txt";
    OpenFile(log_file.c_str());
    //printf("tvpn log file:%s\n", log_file.c_str());
    std::string strtemp = "tvpn log file:";
    strtemp += log_file;
    os_log(OS_LOG_DEFAULT, "tvpn %{public}s", strtemp.c_str());
    return 0;
}
int network_service::init() {
    //signal(SIGKILL, quit_signal_handler);
    //signal(SIGINT, quit_signal_handler);
    
    
    SetLogLevel(0);
    firstOpen = 1;
    
/*    system("route -n get default | grep 'gateway' | awk '{print $2}' > /tmp/default_gateway.txt");
    file_utl::read_file("/tmp/default_gateway.txt", global_default_gateway_ip);
    if(global_default_gateway_ip.size() > 0)
    {
        //remove \n \r
        const int last_char = *global_default_gateway_ip.rbegin();
        if( (last_char == '\n') || (last_char == '\r') )
            global_default_gateway_ip.resize(global_default_gateway_ip.size()-1);
    }*/
    string_utl::set_random_http_domains();
    sock_http::init_http_head();

    return 0;
}
static char g_tcp_buf[BUF_SIZE*2];
static int g_tcp_len;
int write_tun(char* ip_packet_data, int ip_packet_len){
    int len;
    if (g_tcp_len != 0) {
        if (ip_packet_len + g_tcp_len > sizeof(g_tcp_buf)) {
            ERROR("relay size over %lu", sizeof(g_tcp_buf));
            g_tcp_len = 0;
            return 1;
        }
        memcpy(g_tcp_buf + g_tcp_len, ip_packet_data, ip_packet_len);
        ip_packet_data = g_tcp_buf;
        ip_packet_len += g_tcp_len;
        g_tcp_len = 0;
        DEBUG2("relayed packet:%d", ip_packet_len);
    }

    while(1) {
        if (ip_packet_len == 0)
            break;
        // todo: recv from socket, send to utun1
        if (ip_packet_len < sizeof(struct ip) ) {
            ERROR("less than ip header:%d.", ip_packet_len);
            memcpy(g_tcp_buf, ip_packet_data, ip_packet_len);
            g_tcp_len = ip_packet_len;
            break;
        }
        struct ip *iph = (struct ip *)ip_packet_data;
        len = ntohs(iph->ip_len);

        if (ip_packet_len < len) {
            if (len > BUF_SIZE) {
                ERROR("something error1.%x,%x,data:%s",len, ip_packet_len, string_utl::HexEncode(std::string(ip_packet_data,ip_packet_len)).c_str());
                g_tcp_len = 0;
            } else {
                DEBUG2("relay to next packet:%d,current buff len:%d", ip_packet_len, g_tcp_len);
                if (g_tcp_len == 0) {
                    memcpy(g_tcp_buf +g_tcp_len, ip_packet_data, ip_packet_len);
                    g_tcp_len += ip_packet_len;
                }
            }
            break;
        }

        if (len > BUF_SIZE) {
            ERROR("something error.%x,%x",len, ip_packet_len);
            g_tcp_len = 0;
            break;
        } else if (len == 0) {
            ERROR("len is zero.%x,%x",len, ip_packet_len); //string_utl::HexEncode(std::string(ip_packet_data,ip_packet_len)).c_str());
            g_tcp_len = 0;
            break;
        }

        char ip_src[INET_ADDRSTRLEN + 1];
        char ip_dst[INET_ADDRSTRLEN + 1];
        inet_ntop(AF_INET,&iph->ip_src.s_addr,ip_src, INET_ADDRSTRLEN);
        inet_ntop(AF_INET,&iph->ip_dst.s_addr,ip_dst, INET_ADDRSTRLEN);

        DEBUG2("send to utun, from(%s) to (%s) with size:%d",ip_src,ip_dst,len);
        //os_log(OS_LOG_DEFAULT, "tvpn send to tun, from(%{public}s) to (%{public}s) size:%d,%{public}s",
          //     ip_src,ip_dst,len,string_utl::HexEncode(std::string(ip_packet_data, len)).c_str());
        //os_log(OS_LOG_DEFAULT, "tvpn send to tun, from(%{public}s) to (%{public}s) with size:%d",ip_src,ip_dst,len);

        if (g_socketRecvCallback!= NULL)
            g_socketRecvCallback((uint8_t*)ip_packet_data, len, g_target);
        
        ip_packet_len -= len;
        ip_packet_data += len;
    }
    return 0;
}
int write_tun_http(char* ip_packet_data, int ip_packet_len) {
    static uint32_t g_iv = 0x87654321;
    int len;
    if (g_tcp_len != 0) {
        if (ip_packet_len + g_tcp_len > sizeof(g_tcp_buf)) {
            INFO("relay size over %d", sizeof(g_tcp_buf));
            g_tcp_len = 0;
            return 1;
        }
        memcpy(g_tcp_buf + g_tcp_len, ip_packet_data, ip_packet_len);
        ip_packet_data = g_tcp_buf;
        ip_packet_len += g_tcp_len;
        g_tcp_len = 0;
        INFO("relayed packet:%d", ip_packet_len);
    }
    std::string http_packet;
    int http_head_length, http_body_length;
    while (1) {
        if (ip_packet_len == 0)
            break;
        http_packet.assign(ip_packet_data, ip_packet_len);
        if (sock_http::pop_front_xdpi_head(http_packet, http_head_length, http_body_length) != 0) {  // decode http header fail
            DEBUG2("relay to next packet:%d,current buff len:%d", ip_packet_len, g_tcp_len);
            if (g_tcp_len == 0) {
                memcpy(g_tcp_buf + g_tcp_len, ip_packet_data, ip_packet_len);
                g_tcp_len += ip_packet_len;
            }
            break;
        }
        ip_packet_len -= http_head_length;
        ip_packet_data += http_head_length;
        obfuscation_utl::decode((unsigned char *) ip_packet_data, 4, g_iv);
        obfuscation_utl::decode((unsigned char *) ip_packet_data + 4, http_body_length - 4, g_iv);

        struct ip *iph = (struct ip *) ip_packet_data;
        len = ntohs(iph->ip_len);
        char ip_src[INET_ADDRSTRLEN + 1];
        char ip_dst[INET_ADDRSTRLEN + 1];
        inet_ntop(AF_INET, &iph->ip_src.s_addr, ip_src, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &iph->ip_dst.s_addr, ip_dst, INET_ADDRSTRLEN);

        DEBUG2("send to tun,http, from(%s) to (%s) with size:%d, header:%d,body:%d", ip_src,
              ip_dst, len, http_head_length, http_body_length);
        //os_log(OS_LOG_DEFAULT,"tvpn send to tun,http, from(%{public}s) to (%{public}s) with size:%d, header:%d,body:%d,%{public}s", ip_src,
        //  ip_dst, len, http_head_length, http_body_length,string_utl::HexEncode(std::string(ip_packet_data,len)).c_str());
        
        //sys_utl::tun_dev_write(g_fd_tun_dev, (void *) ip_packet_data, len);
        if (g_socketRecvCallback!= NULL)
            g_socketRecvCallback((uint8_t*)ip_packet_data, len, g_target);

        ip_packet_len -= http_body_length;
        ip_packet_data += http_body_length;
    }
    return 0;
}

int network_service::	get_vpnserver_ip(std::string user_name, std::string password, std::string device_id, long premium, std::string country_code, void(*trafficCallback)(long, long,long,long,void*), void* target){
    struct hostent *h;
    if((h=gethostbyname(web_server_ip.c_str()))==NULL) {
        return 1;
    }
    std::string web_ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    INFO("web ip:%s", web_ip.c_str());
    struct sockaddr_in serv_addr;
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        INFO("socket() error");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(web_ip.c_str());
    serv_addr.sin_port=htons(60315);
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) {
        INFO("connect() error!");
        return 1;
    }
    INFO("connect web server ok.");
    
    std::string strtemp;
    strtemp += (char)0;
    strtemp += (char)premium;
    strtemp += country_code;
    if (premium <= 1)
        strtemp += device_id;
    else
        strtemp += user_name;
    INFO("send:%s", string_utl::HexEncode(strtemp).c_str());
    int ret=file_utl::write(sock, (char*)strtemp.c_str(), (int)strtemp.size());
    char ip_packet_data[BUF_SIZE];
    ret=file_utl::read(sock, ip_packet_data, BUF_SIZE);

    ip_packet_data[ret] = 0;
    INFO("recv from web_server:%s", string_utl::HexEncode(std::string(ip_packet_data,ret)).c_str());
    //current_traffic = 0;
    int pos = 0;
    uint32_t day_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t day_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    g_day_traffic = day_traffic;
    g_month_traffic = month_traffic;
    if (premium<=1){
        if (day_traffic > day_limit) {
            trafficCallback(day_traffic,month_traffic,day_limit, month_limit, target);
            return 2;
        }
    } else{
        if (month_traffic > month_limit) {
            trafficCallback(day_traffic,month_traffic,day_limit, month_limit, target);
            return 2;
        }
    }
    trafficCallback(day_traffic,month_traffic,day_limit, month_limit, target);
    
    std::string recv_ip(ip_packet_data + 16, ret-16);
    std::vector<std::string> recv_data;
    string_utl::split_string(recv_ip,',', recv_data);
    if(recv_data.size() < 1) {
        ERROR("recv server ip data error.");
        //tunnel.close();
        return 1;
    }
    INFO("recv:%s", recv_data[0].c_str());
    std::vector<std::string>  server_data;
    string_utl::split_string(recv_data[0],':', server_data);

    if(server_data.size() < 3) {
        ERROR("parse server ip data error.");
        //tunnel.close();
        return 1;
    }
    //Log.i(TAG, "data:" + server_data[0]+","+server_data[1]+","+server_data[2]);
    g_protocol = std::stoi(server_data[0]);
    target_test_ip = server_data[1];
    target_test_ip32 = ntohl(inet_addr(target_test_ip.c_str()));
    //g_ip = "192.168.50.218";
    target_test_port = std::stoi(server_data[2]);
    INFO("protocol:%d,%s,%d",g_protocol, target_test_ip.c_str(),target_test_port);
    return 0;
}
void tun_and_socket(void(*stopCallback)(long, void*), void(*trafficCallback)(long, long,long,long,void*), void* target)
{
    INFO("start ssl thread,client_sock:%d", client_sock);
    os_log(OS_LOG_DEFAULT, "tvpn start ssl thread,client_sock:%d", client_sock);
    g_isRun= 1;
    g_in_recv_tun = 1;
    g_tcp_len = 0;
    in_traffic = 0;
    out_traffic = 0;
    //std::thread tun_thread(client_recv_tun, client_sock);

    g_in_recv_socket = 1;
    int ip_packet_len;
    char buffer_data[BUF_SIZE*16];
    int index = 0;
    char* ip_packet_data;
    int ret;
    time_t lastTime = time_utl::localtime();
    time_t currentTime;
    time_t recvTime = time_utl::localtime();
    //time_t sendTime = time_utl::localtime();

    fd_set fdsr;
    int maxfd;
    while(g_isRun == 1){
        FD_ZERO(&fdsr);
        FD_SET(client_sock, &fdsr);
        //FD_SET(g_fd_tun_dev, &fdsr);
        maxfd = client_sock;//std::max(client_sock, g_fd_tun_dev);
        struct timeval tv_select;
        tv_select.tv_sec = 2;
        tv_select.tv_usec = 0;
        os_log(OS_LOG_DEFAULT, "tvpn prepare read:%d", client_sock);
        int nReady = select(maxfd + 1, &fdsr, NULL, NULL, &tv_select);
        if (nReady < 0) {
            ERROR("select error:%d", nReady);
            os_log(OS_LOG_DEFAULT, "tvpn select error:%d", nReady);
            break;
        } else if (nReady == 0) {
            INFO("select timeout");
            os_log(OS_LOG_DEFAULT, "tvpn select timeout");
            continue;
        }
        
        if (FD_ISSET(client_sock, &fdsr)) {  // recv from socket
           // std::lock_guard<std::mutex> lck (g_mutex);
            ip_packet_data = buffer_data + (index++)%16 * BUF_SIZE;
            ip_packet_len = 0;
            if (g_protocol == kSslType) {

                ret = ssl_read(ip_packet_data, ip_packet_len);
                if (ret != 0) {
                    ERROR("ssl_read error");
                    break;
                }
            } else if (g_protocol == kHttpType) {
                os_log(OS_LOG_DEFAULT, "tvpn start read http");
                ip_packet_len = file_utl::read(client_sock, ip_packet_data, BUF_SIZE);
            } else {
                ERROR("protocol error.");
                break;
            }
            if (ip_packet_len == 0) {
                os_log(OS_LOG_DEFAULT, "tvpn ssl recv empty.");
                continue;
            }
            in_traffic += ip_packet_len;
            DEBUG2("recv from socket, size:%d", ip_packet_len);
            os_log(OS_LOG_DEFAULT, "tvpn recv from socket, size:%d", ip_packet_len);
            if (g_protocol == kSslType) {
                if (write_tun((char *) ip_packet_data, ip_packet_len) != 0) {
                    ERROR("write_tun error");
                    break;
                }
            } else if (g_protocol == kHttpType){
                if (write_tun_http((char *) ip_packet_data, ip_packet_len) != 0) {
                    ERROR("write_tun error");
                    break;
                }
            }
            recvTime = time_utl::localtime();
        }
        currentTime = time_utl::localtime();
        if (currentTime - recvTime > 60) { //|| currentTime - sendTime > 60) {
            ERROR("send or recv timeout");
            break;
        }
        if (currentTime - lastTime >= 1) {
            trafficCallback(g_day_traffic + (in_traffic + out_traffic)/1024, g_month_traffic+ (in_traffic + out_traffic)/1024, 0,0, target);
            lastTime = time_utl::localtime();
        }
    }

    os_log(OS_LOG_DEFAULT, "tvpn main thread stop");
    if(g_protocol == kSslType)
        ssl_close();
    else if (g_protocol == kHttpType)
        close(client_sock);
    //close(g_fd_tun_dev);
    g_isRun = 0;
    stopCallback(1, target);
    
    return ;
}
int network_service::start_vpn(std::string user_name, std::string password, std::string device_id, long premium, std::string country_code, void(*stopCallback)(long, void*), void(*trafficCallback)(long, long,long,long,void*), void(*get_ip_callback)(long, long,long,void*), void* target)
{
    if(firstOpen==0) {
        init();
    }
    g_user_name = user_name;
    g_password = password;
    g_device_id = device_id;
    // get vpnserver ip
    int ret = get_vpnserver_ip(user_name, password, device_id, premium, country_code, trafficCallback, target);
    if ( ret == 1) {
        stopCallback(1, target);
        return 1;
    } else if (ret == 2) {
        stopCallback(1, target);
        return 2;
    }
    
    if (connect_server(premium)!=0){
        stopCallback(1, target);
        return 1;
    }
    
    get_ip_callback(g_private_ip, target_test_ip32, target_test_port, target);
    
    std::thread tun_socket_thread(::tun_and_socket, stopCallback, trafficCallback, target);
    tun_socket_thread.detach();
/*    ret = tun_and_socket(trafficCallback, target);
    if (ret != 0) {
        stopCallback(1, target);
        return ret;
    }*/
    //stopCallback(1, target);
    return 0;
}

int network_service::connect_server(long premium)
{
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
       INFO("socket() error");
       return 1;
    }

    //std::string strIp = "159.65.226.184";
    //uint16_t port = 14455;
    if (g_protocol == kSslType) {
       if (init_ssl_client() != 0) {
           ERROR( "init ssl fail.");
           return 1;
       }
       INFO("connect ssl");
       connect_ssl(target_test_ip, target_test_port, sock);
       if (sock == 0) {
           ERROR("sock is zero.");
           return 1;
       }
    } else if (g_protocol == kHttpType) {
       if (connect_tcp(target_test_ip, target_test_port, sock) != 0)
           return 1;
    } else {
       ERROR( "protocol errror.");
       return 1;
    }
    client_sock = sock;

    INFO("connect ok.");
    std::string strPrivateIp;
    //INFO("get private_ip");
    if (g_protocol == kSslType){
       //std::string strId = "IOS.00000001";
       //std::string strPassword = "123456";
       
       get_private_ip(premium,g_device_id, g_user_name, g_password, strPrivateIp);
    }
    else if (g_protocol == kHttpType) {
        if (get_private_ip_http(premium, g_device_id, g_user_name, g_password, strPrivateIp) != 0) {
            return 1;
        }
    }
    g_private_ip = *(uint32_t*)strPrivateIp.c_str();

    global_private_ip = socket_utl::socketaddr_to_string(g_private_ip);
    INFO("private_ip:%s", global_private_ip.c_str());
    g_private_ip_net = g_private_ip;
    g_private_ip = ntohl(g_private_ip);

    return 0;
}
int network_service::stop_vpn(long value)
{
    g_isRun = 0;
    return 0;
}
int network_service::connect_web_server(std::string user_name, long premium, uint32_t* private_ip )
{
    return 0;
}
int network_service::login(std::string user_name, std::string password, std::string device_id,
                           void(*trafficCallback)(long, long,long,long,long,long, void*), void* target)
{
    if(firstOpen==0) {
        init();
    }
    
    struct hostent *h;
    if((h=gethostbyname(web_server_ip.c_str()))==NULL) {
        return 1;
    }
    std::string web_ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    INFO("web ip1:%s", web_ip.c_str());
    struct sockaddr_in serv_addr;
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        INFO("socket() error");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(web_ip.c_str());
    serv_addr.sin_port=htons(60315);
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) {
        INFO("connect() error!");
        return 1;
    }
    INFO("connect web server ok.");

    std::string strtemp;
    strtemp += (char)1;
    strtemp += device_id;
    strtemp += (char)'\n';
    strtemp += user_name;
    strtemp += (char)'\n';
    strtemp += password;

    int ret=file_utl::write(sock, (char*)strtemp.c_str(), (int)strtemp.size());
    char ip_packet_data[BUF_SIZE];
    ret=file_utl::read(sock, ip_packet_data, BUF_SIZE);
    if (ret < 2 + 4*sizeof(uint32_t))
        return 1;
    ip_packet_data[ret] = 0;
    INFO("recv from web_server:%s", string_utl::HexEncode(std::string(ip_packet_data,ret)).c_str());
    int pos=0;
    int ret1 = ip_packet_data[pos++];
    int ret2 = ip_packet_data[pos++];
    uint32_t day_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t day_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    pos += sizeof(uint32_t);
    uint32_t month_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    
    close(sock);
    INFO("recv login:%d,%d,%x,%x,%x,%x" , ret1 , ret2, day_traffic , month_traffic, day_limit,month_limit);
    g_user_name = user_name;
    g_password = password;
    
    trafficCallback(day_traffic,month_traffic,day_limit,month_limit,ret1,ret2,target);
    return 0;
}
int network_service::premium(std::string user_name, void(*trafficCallback)(long, long,long,long,long,long, void*), void* target)
{
    struct hostent *h;
    if((h=gethostbyname(web_server_ip.c_str()))==NULL) {
        return 1;
    }
    std::string web_ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    INFO("premium, web ip1:%s", web_ip.c_str());
    struct sockaddr_in serv_addr;
    int sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        INFO("socket() error");
        return 1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(web_ip.c_str());
    serv_addr.sin_port=htons(60315);
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) {
        INFO("connect() error!");
        return 1;
    }
    INFO("connect web server ok.");

    std::string strtemp;
    strtemp += (char)2;
    strtemp += (char)2;
    strtemp += user_name;

    int ret=file_utl::write(sock, (char*)strtemp.c_str(), (int)strtemp.size());
    char ip_packet_data[BUF_SIZE];
    ret=file_utl::read(sock, ip_packet_data, BUF_SIZE);
    if (ret < 2 )
        return 1;
    ip_packet_data[ret] = 0;
    //INFO("recv from web_server:%s", string_utl::HexEncode(std::string(ip_packet_data,ret)).c_str());
    int pos=0;
    int ret1 = ip_packet_data[pos++];
    int ret2 = ip_packet_data[pos++];
    uint32_t day_traffic =0;
    uint32_t month_traffic =0;
    uint32_t day_limit = 0;
    uint32_t month_limit = 0;
    if (ret1 == 0) {
        day_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
        pos += sizeof(uint32_t);
        month_traffic = ntohl(*(uint32_t*)(ip_packet_data + pos));
        pos += sizeof(uint32_t);
        day_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
        pos += sizeof(uint32_t);
        month_limit = ntohl(*(uint32_t*)(ip_packet_data + pos));
    }
    
    close(sock);
    INFO("recv login:%d,%d,%x,%x,%x,%x" , ret1 , ret2, day_traffic , month_traffic, day_limit,month_limit);
    
    trafficCallback(day_traffic,month_traffic,day_limit,month_limit,ret1,ret2,target);
    return 0;
}
int network_service::send_packet(uint8_t* packet, long len)
{
    static VpnPacket vpn_packet(4096);
    int readed_from_tun;
    vpn_packet.reset();
    vpn_packet.push_back(packet, (uint32_t)len);
    readed_from_tun = (int)len;  //sys_utl::tun_dev_read(g_fd_tun_dev, vpn_packet.data(), vpn_packet.remain_size());
    vpn_packet.set_back_offset(vpn_packet.front_offset()+readed_from_tun);
    
    struct ip *iph = (struct ip *)vpn_packet.data();

    char ip_src[INET_ADDRSTRLEN + 1];
    char ip_dst[INET_ADDRSTRLEN + 1];
    inet_ntop(AF_INET,&iph->ip_src.s_addr,ip_src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET,&iph->ip_dst.s_addr,ip_dst, INET_ADDRSTRLEN);

    if(g_private_ip_net != iph->ip_src.s_addr) {
        ERROR("src_ip mismatch:%x,%x,%s",g_private_ip_net, iph->ip_src.s_addr, string_utl::HexEncode(std::string((char*)packet, len)).c_str());
        os_log(OS_LOG_DEFAULT, "tvpn src_ip mismatch:%x,%x,%{public}s",g_private_ip_net, iph->ip_src.s_addr,
               string_utl::HexEncode(std::string((char*)packet, len)).c_str());
        return 1;
    }
    DEBUG2("recv from tun, from(%s) to (%s) with size:%d, protocol:%d",ip_src,ip_dst,readed_from_tun, g_protocol);


//        os_log(OS_LOG_DEFAULT, "tvpn send to socket, from(%{public}s) to (%{public}s) with size:%d",ip_src,ip_dst,readed_from_tun);

//        os_log(OS_LOG_DEFAULT, "tvpn send to socket, from(%{public}s) to (%{public}s) with size:%d, %{public}s %{public}s",ip_src,ip_dst,readed_from_tun,
  //             string_utl::HexEncode(std::string((char*)vpn_packet.data(), sizeof(struct ip))).c_str(),
    //           string_utl::HexEncode(std::string((char*)vpn_packet.data() + sizeof(struct ip), readed_from_tun - sizeof(struct ip))).c_str());
    out_traffic += readed_from_tun;

    //std::lock_guard<std::mutex> lck (g_mutex);
    if (g_protocol == kSslType) {
        if (ssl_write(vpn_packet.data(), readed_from_tun) != 0) {
            ERROR("ssl_write error");
            return 1;
        }
    } else if (g_protocol == kHttpType){
        http_write(vpn_packet);
        os_log(OS_LOG_DEFAULT, "tvpn write http:%d", vpn_packet.size());
    }
    //sendTime = time_utl::localtime();
    //os_log(OS_LOG_DEFAULT, "tvpn write ssl ok.");
    return 0;
}
int network_service::set_socket_recv(void(*socketRecvCallback)(uint8_t* data, long len, void* target), void*target)
{
    g_socketRecvCallback =socketRecvCallback;
    g_target = target;
    return 0;
}
