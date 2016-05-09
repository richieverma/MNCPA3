#ifndef CONTROL_HANDLER_H_
#define CONTROL_HANDLER_H_

int create_control_sock();
int create_tcp_sock(uint16_t port);
int create_udp_sock(uint16_t port);
int create_tcp_conn(char *dest_ip, unsigned dest_port);
int new_control_conn(int sock_index);
bool isControl(int sock_index);
int new_data_conn(int sock_index);
bool isData(int sock_index);
bool control_recv_hook(int sock_index);
bool router_recv_hook(int sock_index);
bool data_recv_hook(int sock_index);
void file_data_received(int sock_index, char *payload, uint8_t transfer_id, unsigned fin_bit);

#endif