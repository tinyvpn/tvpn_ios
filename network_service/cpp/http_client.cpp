#include <string>
#include "log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "sysutl.h"
#include "stringutl.h"
#include "fileutl.h"
#include "vpn_packet.h"
#include "obfuscation_utl.h"
#include "sockhttp.h"
//
// Created by Hench on 6/21/20.
//
static int client_sock;
static uint32_t g_iv = 0x87654321;

int connect_tcp(std::string& ip, uint16_t port, int& sock)
{
    sock =socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        INFO("socket() error");
        return 1;
    }
    client_sock = sock;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=inet_addr(ip.c_str());
    serv_addr.sin_port=htons(port);

    int bufsize_ = 1024*1024;
    setsockopt (sock, SOL_SOCKET, SO_SNDBUF,(char*) &bufsize_, (socklen_t)sizeof(bufsize_));
    setsockopt (sock, SOL_SOCKET, SO_RCVBUF,(char*) &bufsize_, sizeof bufsize_);
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) {
        return 1;
    }
    return 0;
}
const int BUF_SIZE = 4096*4;
static char g_tcp_buf[BUF_SIZE*2];
static int g_tcp_len;
int get_private_ip_http(int premium, std::string& androidId, std::string& userName, std::string& userPassword, std::string& recv_data) {
    /*
     * 6. Write the request
     */
    // int retry_left = opt.max_resend;
    int len, written, frags, ret;
    unsigned char *buf;

    std::string str_request;
    str_request += (char) premium;
    str_request += (char) androidId.size();
    str_request += androidId;
    if (premium >= 2) {
        str_request += (char) userName.size();
        str_request += userName;
        str_request += (char) userPassword.size();
        str_request += userPassword;
    }
    buf = (uint8_t * )(str_request.c_str());
    len = (int) (str_request.size());
    VpnPacket vpn_packet(4096);
    vpn_packet.push_back((uint8_t*)buf, len);
    obfuscation_utl::encode((unsigned char*)vpn_packet.data(), vpn_packet.size(), g_iv);
    //__android_log_print(ANDROID_LOG_INFO, "JNI", "send request:%s", string_utl::HexEncode(std::string((char *) buf, len)).c_str());
    sock_http::push_front_xdpi_head_1(vpn_packet);

    ret=file_utl::write(client_sock, vpn_packet.data(), vpn_packet.size());
    char ip_packet_data1[1024];
    char* ip_packet_data = ip_packet_data1;
    ret=file_utl::read(client_sock, ip_packet_data, sizeof(ip_packet_data1));
    if (ret < 4)
        return 1;
    //__android_log_print(ANDROID_LOG_INFO, "JNI", "recv response:%s", string_utl::HexEncode(std::string((char *) ip_packet_data, ret)).c_str());
    std::string http_packet;
    http_packet.assign(ip_packet_data, ret);
    int ip_packet_len = ret;

    int http_head_length,http_body_length;
    if (sock_http::pop_front_xdpi_head(http_packet, http_head_length, http_body_length) != 0) {  // decode http header fail
//        DEBUG("relay to next packet:%d,%d,current buff len:%d", conn_id, ip_packet_len, g_connections[conn_id].packet_len);
        return 1;
    }
    ip_packet_len -= http_head_length;
    ip_packet_data += http_head_length;
    obfuscation_utl::decode((unsigned char*)ip_packet_data, 4, g_iv);
    obfuscation_utl::decode((unsigned char*)ip_packet_data+4, http_body_length-4, g_iv);
    if (ip_packet_len < 4) {
//        __android_log_write(ANDROID_LOG_ERROR, "JNI", "get private ip error");
        return 1;
    }
    recv_data.assign(ip_packet_data, ip_packet_len);
    return 0;
}
int http_write(VpnPacket& vpn_packet) {
    obfuscation_utl::encode((unsigned char*)vpn_packet.data(), 4, g_iv);
    obfuscation_utl::encode((unsigned char*)vpn_packet.data()+4, vpn_packet.size()-4, g_iv);

    sock_http::push_front_xdpi_head_1(vpn_packet);
    file_utl::write(client_sock, vpn_packet.data(), vpn_packet.size());
    return 0;
}
