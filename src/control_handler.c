/**
 * @control_handler
 * @author  Swetank Kumar Saha <swetankk@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * Handler for the control plane.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h> //struct addrinfo
#include <errno.h> //errno

#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../include/global.h"
#include "../include/network_util.h"
#include "../include/control_header_lib.h"
#include "../include/author.h"
#include "../include/init.h"
#include "../include/routing.h"
#include "../include/crash.h"
#include "../include/sendfile.h"


#ifndef PACKET_USING_STRUCT
    #define CNTRL_CONTROL_CODE_OFFSET 0x04
    #define CNTRL_PAYLOAD_LEN_OFFSET 0x06
#endif

extern long int unpacki32(unsigned char *buf);
void file_data_received(int sock_index, char *payload, uint8_t transfer_id, unsigned fin_bit);
/* Linked List for active control connections */
struct ControlConn
{
    int sockfd;
    LIST_ENTRY(ControlConn) next;
}*connection, *conn_temp;
LIST_HEAD(ControlConnsHead, ControlConn) control_conn_list;

struct DataConn
{
    int sockfd;
    LIST_ENTRY(DataConn) next;
}*dataConnection, *data_conn_temp;
LIST_HEAD(DataConnsHead, DataConn) data_conn_list;

int create_control_sock()
{
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(CONTROL_PORT);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    if(listen(sock, 5) < 0)
        ERROR("listen() failed");

    LIST_INIT(&control_conn_list);

    return sock;
}

int create_tcp_sock(uint16_t port)
{
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(port);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    if(listen(sock, 5) < 0)
        ERROR("listen() failed");

    return sock;
}

int create_tcp_conn(char *dest_ip, unsigned dest_port)
{
    struct addrinfo hints, *res, *p;
    void *addr = NULL;
  
    char ipstr[INET_ADDRSTRLEN];
    int status, sockfd;
  
    struct sockaddr_in serverAddr;
    socklen_t addr_size;
//SOCKET
  
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        int errsv = errno;
        printf("Socket Error: %d\n", errsv);              
        return -1;
    }
  
    /* Make socket re-usable */
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

/*---- Configure settings of the server address struct ----*/
  /* Address family = Internet */
    serverAddr.sin_family = AF_INET;
  /* Set port number, using htons function to use proper byte order */
    serverAddr.sin_port = htons(dest_port);
  /* Set the IP address to desired host to connect to */
    serverAddr.sin_addr.s_addr = inet_addr(dest_ip);
  /* Set all bits of the padding field to 0 */
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);    
    //CONNECT
/*---- Connect the socket to the server using the address struct ----*/
  addr_size = sizeof serverAddr;

    if (connect(sockfd, (struct sockaddr *) &serverAddr, addr_size)== -1){
        int errsv = errno;
        printf("Connect Error: %d\n", errsv);  
        close(sockfd);              
        return -1;
        //return errsv;
    }
   
    return sockfd;    
}

int create_udp_sock(uint16_t port)
{
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(port);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    return sock;
}

int new_control_conn(int sock_index)
{
    int fdaccept, caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);
    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, &caddr_len);
    if(fdaccept < 0)
        ERROR("accept() failed");

    /* Insert into list of active control connections */
    connection = malloc(sizeof(struct ControlConn));
    connection->sockfd = fdaccept;
    LIST_INSERT_HEAD(&control_conn_list, connection, next);

    return fdaccept;
}

void remove_control_conn(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next) {
        if(connection->sockfd == sock_index){
            LIST_REMOVE(connection, next); // this may be unsafe?
            free(connection);
            break;
        }
    }

    close(sock_index);
}

bool isControl(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next)
        if(connection->sockfd == sock_index) return TRUE;

    return FALSE;
}

int new_data_conn(int sock_index)
{
    int fdaccept, caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);
    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, &caddr_len);
    if(fdaccept < 0)
        ERROR("accept() failed");

    /* Insert into list of active control connections */
    dataConnection = malloc(sizeof(struct ControlConn));
    dataConnection->sockfd = fdaccept;
    LIST_INSERT_HEAD(&data_conn_list, dataConnection, next);

    return fdaccept;
}

void remove_data_conn(int sock_index)
{
    LIST_FOREACH(dataConnection, &data_conn_list, next) {
        if(dataConnection->sockfd == sock_index){
            LIST_REMOVE(dataConnection, next); // this may be unsafe?
            free(dataConnection);
            break;
        }
    }

    close(sock_index);
}

bool isData(int sock_index)
{
    LIST_FOREACH(dataConnection, &data_conn_list, next)
        if(dataConnection->sockfd == sock_index) return TRUE;

    return FALSE;
}

bool control_recv_hook(int sock_index)
{
    char *cntrl_header, *cntrl_payload;
    uint8_t control_code;
    uint16_t payload_len;

    /* Get control header */
    cntrl_header = (char *) malloc(sizeof(char)*CNTRL_HEADER_SIZE);
    bzero(cntrl_header, CNTRL_HEADER_SIZE);

    if(recvALL(sock_index, cntrl_header, CNTRL_HEADER_SIZE) < 0){
        remove_control_conn(sock_index);
        free(cntrl_header);
        return FALSE;
    }

    /* Get control code and payload length from the header */
    #ifdef PACKET_USING_STRUCT
        /** ASSERT(sizeof(struct CONTROL_HEADER) == 8) 
          * This is not really necessary with the __packed__ directive supplied during declaration (see control_header_lib.h).
          * If this fails, comment #define PACKET_USING_STRUCT in control_header_lib.h
          */
        BUILD_BUG_ON(sizeof(struct CONTROL_HEADER) != CNTRL_HEADER_SIZE); // This will FAIL during compilation itself; See comment above.

        struct CONTROL_HEADER *header = (struct CONTROL_HEADER *) cntrl_header;
        control_code = header->control_code;
        payload_len = ntohs(header->payload_len);
    #endif
    #ifndef PACKET_USING_STRUCT
        memcpy(&control_code, cntrl_header+CNTRL_CONTROL_CODE_OFFSET, sizeof(control_code));
        memcpy(&payload_len, cntrl_header+CNTRL_PAYLOAD_LEN_OFFSET, sizeof(payload_len));
        payload_len = ntohs(payload_len);
    #endif

    free(cntrl_header);

    /* Get control payload */
    if(payload_len != 0){
        cntrl_payload = (char *) malloc(sizeof(char)*payload_len);
        bzero(cntrl_payload, payload_len);

        if(recvALL(sock_index, cntrl_payload, payload_len) < 0){
            remove_control_conn(sock_index);
            free(cntrl_payload);
            return FALSE;
        }
    }

    /* Triage on control_code */
    switch(control_code){
        case 0: author_response(sock_index);
                break;
        case 1: init_response(sock_index, cntrl_payload);
                break;
        case 2: routing_table_response(sock_index);
                break;
        case 3: update_response(sock_index, cntrl_payload);
                break;
        case 4: crash_response(sock_index);
                break;
        case 5: sendfile_response(sock_index, cntrl_payload, payload_len);
                break;
        case 6: sendfile_stats_response(sock_index, cntrl_payload);
                break;
        case 7: last_data_packet_response(sock_index);
                break; 
        case 8: penultimate_data_packet_response(sock_index);
                break;                                                                 
        /* .......
        case 1: init_response(sock_index, cntrl_payload);
                break;

            .........
           ....... 
         ......*/
    }

    if(payload_len != 0) free(cntrl_payload);
    return TRUE;
}

extern void hexDump (char *desc, void *addr, int len);

bool router_recv_hook(int sock_index)
{
    struct sockaddr_storage their_addr;
    struct in_addr ip;
    socklen_t addr_len;
    addr_len = sizeof their_addr;


    char routing_update[69], *source_ip, *s_ip;
    uint16_t num_update_fields, source_router_port;
    unsigned numbytes = 0, numbytes_source = 0;
    unsigned routing_packet_length = 0;

    numbytes = recvfrom(sock_index, routing_update, 69, 0, (struct sockaddr *)&their_addr, &addr_len);  

    //Get Number of updated fields to calculate size of packet
    memcpy(&num_update_fields, routing_update, 2);
    num_update_fields = ntohs(num_update_fields);

    memcpy(&source_router_port, routing_update + 2, 2);
    source_router_port = ntohs(source_router_port);

    memcpy(&ip, routing_update + 4, 4);
    s_ip = inet_ntoa(ip);
    source_ip = (char *)malloc(strlen(s_ip));
    strcpy(source_ip,s_ip); 
    
    //Calculate routing packet length based on number of fields
    routing_packet_length = (12 * num_update_fields) + 8;
     

    //printf("\nTotal Bytes received: %d\n", numbytes);

    //hexDump("RECEIVED ROUTING UPDATE:",routing_update, routing_packet_length);
    update_routing_table(source_ip, source_router_port, num_update_fields, routing_update);

}

bool data_recv_hook(int sock_index)
{
    char packet[1036], *dest_ip, *dip, fin[32], payload[1024];
    uint8_t ttl, transfer_id;
    uint16_t seq;
    unsigned fin_bit = 0;
    int nbytes = 0;
    struct in_addr ip;

    memset(packet, 0, sizeof packet);

    //Socket closed
    nbytes = recvALL(sock_index, packet, 1036);
    if(nbytes <= 0){
        //Remove socket
        printf("Socket to be closed\n");
        remove_data_conn(sock_index);
        return FALSE;
        //return TRUE;
    }

    //Read IP
    memcpy(&ip, packet, 4);
    dip = inet_ntoa(ip);
    dest_ip = (char *)malloc(strlen(dip));
    strcpy(dest_ip,dip);
    //printf("DATARECVHOOK dest_ip:%s\n", dest_ip);

    //Read transfr_id, ttl, seq
    memcpy(&transfer_id, packet + 4, 1);
    //printf("DATARECVHOOK transfer_id:%d\n", transfer_id);
    memcpy(&ttl, packet + 5, 1);
    //printf("DATARECVHOOK ttl:%d\n", ttl);
    memcpy(&seq, packet + 6, 2);
    seq = ntohs(seq);
    //printf("DATARECVHOOK seq:%d\n", seq);

    //Set new ttl to packet and add it to list of packets sent for this transfer id
    if (ttl != 0){
        ttl -= 1;
        memcpy(packet + 5, &ttl, 1);  
        struct sendfileStats *s = malloc(sizeof (struct sendfileStats));
        s->ttl = ttl;
        s->transfer_id = transfer_id;
        s->seq = seq;
        strcpy(s->packet, packet);
        LIST_INSERT_HEAD(&sendfile_stats_list, s, next);
    }

    //Check if file is for me
    if (strcmp(dest_ip, me->router_ip) == 0){
        //File for me
        memcpy(fin, packet + 8, 4);  
        memcpy(payload, packet + 12, 1024); 
        fin_bit = unpacki32(fin)>>31;
        file_data_received(sock_index, payload, transfer_id, fin_bit);
        return TRUE;
    }
    

    if (ttl <= 0){
        printf("PACKET DROPPED:%d\n", transfer_id);
        return TRUE;  
    }

    //Find next hop
    struct routerInit *itr, *next_hop_router = NULL;
    LIST_FOREACH(itr, &router_list, next){
        if (strcmp(dest_ip, itr->router_ip) == 0){
            next_hop_router = itr;
            break;
        }
    }

    //printf("Forward to ROUTER:%d :%s\n",next_hop_router->router_id, packet);
    //Create data socket to send
    //int sockfilesend = create_tcp_conn(next_hop_router->router_ip, next_hop_router->data_port);
    int sockfilesend = dest_data_sockets[next_hop_router->table_id];
    sendALL(sockfilesend, packet, 12+1024);
    //close(sockfilesend);

    //close(sockfilesend); 
    //remove_data_conn(sock_index);
    return TRUE;
}

void file_data_received(int sock_index, char *payload, uint8_t transfer_id, unsigned fin_bit){
    //printf("FILE FOR ME FINBIT:%d\n", fin_bit);
    struct fileHandle *fh = NULL, *itr;
    FILE *f;
    char filename[50];

    LIST_FOREACH(itr, &file_handle_list, next){
        if (itr->transfer_id == transfer_id){
            fh = itr;
            break;
        }
    }

    //File does not exist
    if (fh == NULL){
        fh = malloc(sizeof (struct fileHandle));
        fh->transfer_id = transfer_id;
        snprintf(filename, 50, "file-%d",transfer_id);
        f = fopen(filename, "w");
        fh->f = f;
        fh->socket = sock_index;
        fh->isopen = TRUE;
        memset(fh->contents, 0, sizeof fh->contents);
        LIST_INSERT_HEAD(&file_handle_list, fh, next);
    }
    //printf("WRITE TO FILE:%s\n", payload);
    strcat(fh->contents, payload);

    if (fin_bit != 0){
        fh->isopen = FALSE;
        fwrite(fh->contents, sizeof(char), strlen(fh->contents)-1, fh->f);
        fclose(fh->f);
    }

    if(!fh->isopen){
        printf("FILE CLOSED. TRANSFER COMPLETE\n");
    }


}
