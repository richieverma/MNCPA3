/**
 * @author
 * @author  Richie Verma <richieve@buffalo.edu>
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
 * INIT [Control Code: 0x01]
 */

#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <unistd.h> // for close
#include <sys/types.h>
#include <netdb.h>
#include <time.h> //timeval
#include <sys/time.h> //timeval

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/network_util.h"
#include "../include/init.h"
//#include "../include/routing.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h" 

#define INF 65535

extern fd_set master_list;
extern int head_fd, flag_init;

extern unsigned num_routers, route_table[5][5], orig_route_table[5][5], updates_periodic_interval, dest_data_sockets[5];
extern struct routerInit *me;
extern struct timeval timeout;

extern LIST_HEAD(routerInitHead, routerInit) router_list; 
extern LIST_HEAD(routerInitNeighbourHead, routerInit) router_neighbour_list; 
extern LIST_HEAD(routeUpdateList, routerInit) route_update_list; 
extern LIST_HEAD(trackUpdateList, routerInit) track_update_list; 
extern LIST_HEAD(sendfileStatsList, sendfileStats) sendfile_stats_list;
extern LIST_HEAD(fileHandleList, fileHandle) file_handle_list;

/*
** packi16() -- store a 16-bit int into a char buffer (like htons())
*/ 
void packi16(unsigned char *buf, unsigned int i)
{
    *buf++ = i>>8; *buf++ = i;
}

/*
** packi32() -- store a 32-bit int into a char buffer (like htonl())
*/ 
void packi32(unsigned char *buf, unsigned long int i)
{
    *buf++ = i>>24; *buf++ = i>>16;
    *buf++ = i>>8;  *buf++ = i;
}

/*
** unpacki32() -- unpack a 32-bit int from a char buffer (like ntohl())
*/
long int unpacki32(unsigned char *buf)
{
unsigned long int i2 = ((unsigned long int)buf[0]<<24) |
((unsigned long int)buf[1]<<16) |
((unsigned long int)buf[2]<<8) |
buf[3];
long int i;
// change unsigned numbers to signed
if (i2 <= 0x7fffffffu) { i = i2; }
else { i = -1 - (long int)(0xffffffffu - i2); }
return i;
}

//Hexdump
void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

void init_response(int sock_index, char *cntrl_payload)
{
	unsigned no_routers, router_id, router_port1, router_port2, cost;
	struct in_addr ip;
	char *router_ip;

	//printf("INIT_RESPONSE:%d\n",sock_index);
	
	memcpy(&no_routers, cntrl_payload, 2);
    no_routers = ntohs(no_routers);
    num_routers = no_routers;

    memcpy(&updates_periodic_interval, cntrl_payload+2, 2);
    updates_periodic_interval = ntohs(updates_periodic_interval);

    LIST_INIT(&router_list);
    LIST_INIT(&router_neighbour_list);
    LIST_INIT(&route_update_list);
    LIST_INIT(&track_update_list);
    LIST_INIT(&sendfile_stats_list);
    LIST_INIT(&file_handle_list);

	//printf("ROUTERS:%d UPDATES:%d\n",no_routers, updates_periodic_interval);

    for (int i = 0; i < no_routers; i++){

    	//Populate routerInit struct to keep info of all routers
    	struct routerInit *r = (struct routerInit *)malloc(sizeof(struct routerInit));

    	r->next_update_time.tv_sec = 0;
    	r->next_update_time.tv_usec = 0;
    	r->missed_updates = 0;

		memcpy(&router_id, cntrl_payload + 4 + (i*12), 2);
	    router_id = ntohs(router_id);
	    r->router_id = router_id;

		memcpy(&router_port1, cntrl_payload + 6 + (i*12), 2);
	    router_port1 = ntohs(router_port1);
	    r->router_port = router_port1;

		memcpy(&router_port2, cntrl_payload + 8 + (i*12), 2);
	    router_port2 = ntohs(router_port2);
		r->data_port = router_port2;

		memcpy(&cost, cntrl_payload + 10 + (i*12), 2);
	    cost = ntohs(cost);	
	    r->cost = cost;

		memcpy(&ip, cntrl_payload + 12 + (i*12), 4);
	    router_ip = inet_ntoa(ip);
	    r->router_ip = (char *)malloc(strlen(router_ip));
	    strcpy(r->router_ip,router_ip);

	    r->table_id = no_routers-i-1;
	    LIST_INSERT_HEAD(&router_list, r, next);


	    //Identify self parameters
	    if (cost == 0){
	    	me = r;
	    	//Initialize data socket
	    	data_socket = create_tcp_sock(r->data_port);
            /* Add to watched socket list */
            FD_SET(data_socket, &master_list);
            if(data_socket > head_fd) head_fd = data_socket;

	    	//Initialize router socket
	    	router_socket = create_udp_sock(r->router_port);
            /* Add to watched socket list */
            FD_SET(router_socket, &master_list);
            if(router_socket > head_fd) head_fd = router_socket;

            LIST_INSERT_HEAD(&track_update_list, r, track); //Add self to list of routers to be tracked for routing updates
	    }
	    else if (cost != INF){
	    	LIST_INSERT_HEAD(&router_neighbour_list, r, neighbour); //Add to list of neighbours
	    	LIST_INSERT_HEAD(&track_update_list, r, track); //Add to list of routers to be tracked for routing updates
	    }	    
    }
	//printf("MY_ID:%d MY_TABLE_ID:%d\n",me->router_id, me->table_id);
    //Initialize all entries of routing table with cost INF
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++){
			route_table[i][j] = INF;
			orig_route_table[i][j] = INF;
		}
	}

	//Populate Routing table for all routers
	LIST_FOREACH(router_itr, &router_list, next) {
		if (router_itr->cost != INF){
	    	//router_itr->next_hop = me->router_id;
	    	router_itr->next_hop = router_itr->router_id;
	    }
	    else{
	    	router_itr->next_hop = INF;
	    }	    
        //printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        route_table[me->table_id][router_itr->table_id] = router_itr->cost;
        orig_route_table[me->table_id][router_itr->table_id] = router_itr->cost;
    }



    //SEND RESPONSE FOR INIT
	uint16_t response_len;
	char *cntrl_response_header, *cntrl_response;

	cntrl_response_header = create_response_header(sock_index, 1, 0, 0);

	response_len = CNTRL_RESP_HEADER_SIZE;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, response_len);

	free(cntrl_response); 

	//Print route table
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++){
			printf("%d ", route_table[i][j]);
		}
		printf("\n");
	}
    //Send router packet to neighbours
	send_initial_routing_packet(updates_periodic_interval);	

}

void send_initial_routing_packet(unsigned updates_periodic_interval){
    uint16_t response_len, num_update_fields = 0;
	char *routing_response, *buf = (char *)malloc(16);
	struct sockaddr_in sa;
	char str[INET_ADDRSTRLEN];

	response_len = 8 + num_routers*sizeof(uint16_t)*6; // Four fields of 2 bytes per router, AND 1 field of 4 bytes, 8 bytes for num_update_fields, source router port, source ip address
	routing_response = (char *) malloc(response_len);
	bzero(routing_response, sizeof(routing_response));

	//Number of update fields. Since this is first packet, routing info will be updated for all routers
    packi16(buf, num_routers);
    memcpy(routing_response, buf, 2);

    //Source router port
	packi16(buf, me->router_port);
	memcpy(routing_response + 2, buf, 2);

	//Source Router IP
	inet_pton(AF_INET, me->router_ip, &(sa.sin_addr));
	memcpy(routing_response + 4, &(sa.sin_addr), sizeof(struct in_addr));
    
	LIST_FOREACH(router_itr, &router_list, next) {

		// store this IP address in sa:
		inet_pton(AF_INET, router_itr->router_ip, &(sa.sin_addr));
		memcpy(routing_response + 8 + (num_update_fields * 12), &(sa.sin_addr), sizeof(struct in_addr));

        packi16(buf, router_itr->router_port);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 4, buf, 2);
        
        packi16(buf, 0);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 6, buf, 2);
        
        packi16(buf, router_itr->router_id);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 8, buf, 2);
        
        packi16(buf, router_itr->cost);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 10, buf, 2);			    
        
        //printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        num_update_fields++;
    } 
    free(buf);
    //hexDump ("ROUTER UPDATE 60 BYTES:", routing_response, response_len);

    LIST_FOREACH(router_itr, &router_neighbour_list, neighbour) {
		//Reference Beej's Guide Section 6.3
		int sockfd;
		struct addrinfo hints, *servinfo, *p;
		int rv;
		unsigned numbytes;
		char nbuf[5];

		dest_data_sockets[router_itr->table_id] = create_tcp_conn(router_itr->router_ip, router_itr->data_port);
	    //printf("Connection established with %d, socket:%d\n", router_itr->router_id, dest_data_sockets[router_itr->table_id]);

		snprintf(nbuf,5,"%d",router_itr->router_port);

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		//if ((rv = getaddrinfo(router_itr->router_ip, itoa(router_itr->router_port), &hints, &servinfo)) != 0) {
		if ((rv = getaddrinfo(router_itr->router_ip, nbuf, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return;
		}
		// loop through all the results and make a socket
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("socket");
				continue;
			}
			break;
		}
		if (p == NULL) {
			fprintf(stderr, "Failed to create socket\n");
			return;
		}

		if ((numbytes = sendto(sockfd, routing_response, response_len, 0, p->ai_addr, p->ai_addrlen)) == -1){
				perror("talker: sendto error");
			
			//printf("Sent %d bytes so far. Total to be sent:%d\n", numbytes, response_len);
		}

		freeaddrinfo(servinfo);
		//printf("Sent %d bytes in total. Out of:%d\n", numbytes, response_len);
		close(sockfd);
		//close(sock);
	} 
	free(routing_response);
	struct timeval t;
	gettimeofday(&t, NULL);
	t.tv_sec += updates_periodic_interval;
	me->next_update_time = t;
	timeout.tv_sec = updates_periodic_interval;
	flag_init = 1;

}