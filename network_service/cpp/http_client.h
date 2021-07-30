//
// Created by Hench on 6/21/20.
//

#ifndef TINYVPN_HTTP_CLIENT_H
#define TINYVPN_HTTP_CLIENT_H
#include "vpn_packet.h"
int connect_tcp(std::string& ip, uint16_t port, int& sock);
int get_private_ip_http(int premium, std::string& androidId, std::string& userName, std::string& userPassword, std::string& recv_data);
int http_write(VpnPacket& vpn_packet);
#endif //TINYVPN_HTTP_CLIENT_H
