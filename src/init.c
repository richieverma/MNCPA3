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

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/network_util.h"
#include "../include/init.h"
#include "../include/routing.h"
#include "../include/connection_manager.h"
#include "../include/control_handler.h" 

#define INF 65535

extern fd_set master_list;
extern int head_fd;

unsigned num_routers, route_table[5][5], my_router_id, my_table_id, my_data_port, my_router_port;

/*
** packi16() -- store a 16-bit int into a char buffer (like htons())
*/ 
extern void packi16(unsigned char *buf, unsigned int i);

/*
** packi32() -- store a 32-bit int into a char buffer (like htonl())
*/ 
extern void packi32(unsigned char *buf, unsigned long int i);

void init_response(int sock_index, char *cntrl_payload)
{
	unsigned no_routers, updates_periodic_interval, router_id, router_port1, router_port2, cost;
	struct in_addr ip;
	char *router_ip;

	printf("INIT_RESPONSE:%d\n",sock_index);
	
	memcpy(&no_routers, cntrl_payload, 2);
    no_routers = ntohs(no_routers);
    num_routers = no_routers;

    memcpy(&updates_periodic_interval, cntrl_payload+2, 2);
    updates_periodic_interval = ntohs(updates_periodic_interval);

    LIST_INIT(&router_list);
    LIST_INIT(&router_neighbour_list);

	printf("ROUTERS:%d UPDATES:%d\n",no_routers, updates_periodic_interval);

    for (int i = 0; i < no_routers; i++){

    	//Populate routerInit struct to keep info of all routers
    	struct routerInit *r = (struct routerInit *)malloc(sizeof(struct routerInit));

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
	    	my_router_id = router_id;
	    	my_table_id = no_routers - i - 1;
	    	my_data_port = r->data_port;
	    	my_router_port = r->router_port;
	    	
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

	    }
	    else if (cost != INF){
	    	LIST_INSERT_HEAD(&router_neighbour_list, r, neighbour); //Add to list of neighbours
	    }	    
    }

    //Initialize all entries of routing table with cost INF
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++){
			route_table[i][j] = INF;
		}
	}

	//Populate Routing table for all routers
	LIST_FOREACH(router_itr, &router_list, next) {
		if (router_itr->cost != INF){
	    	router_itr->next_hop = my_router_id;
	    }
	    else{
	    	router_itr->next_hop = INF;
	    }	    
        printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        route_table[my_table_id][router_itr->table_id] = router_itr->cost;
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

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++){
			printf("%d ", route_table[i][j]);
		}
		printf("\n");
	}
    //Send router packet to neighbours
	send_initial_routing_packet();	

}

void send_initial_routing_packet(){
	uint16_t num_update_fields = 0;
    uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response, *buf = (char *)malloc(16);

	payload_len = 8 + num_routers*sizeof(uint16_t)*6; // Four fields of 2 bytes per router, AND 1 field of 4 bytes, 8 bytes for num_update_fields, source router port, source ip address
	cntrl_response_payload = (char *) malloc(payload_len);

    
	LIST_FOREACH(router_itr, &router_neighbour_list, neighbour) {
		struct sockaddr_in sa;
		char str[INET_ADDRSTRLEN];

		// store this IP address in sa:
		inet_pton(AF_INET, router_itr->router_ip, &(sa.sin_addr));
		
		//inet_aton(router_itr->router_ip, buf);
		memcpy(cntrl_response_payload + (num_update_fields * 12), &(sa.sin_addr), sizeof(struct in_addr));
        packi16(buf, router_itr->router_port);
        memcpy(cntrl_response_payload + (num_update_fields * 12) + 4, buf, 2);
        packi16(buf, 0);
        memcpy(cntrl_response_payload + (num_update_fields * 12) + 6, buf, 2);
        packi16(buf, router_itr->router_id);
        memcpy(cntrl_response_payload + (num_update_fields * 12) + 8, buf, 2);
        packi16(buf, router_itr->cost);
        memcpy(cntrl_response_payload + (num_update_fields * 12) + 10, buf, 2);			    
        printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        num_update_fields++;
    } 

    LIST_FOREACH(router_itr, &router_neighbour_list, neighbour) {
	    cntrl_response_header = create_response_header(router_itr->router_port, 2, 0, payload_len);

		response_len = CNTRL_RESP_HEADER_SIZE+payload_len;
		cntrl_response = (char *) malloc(response_len);
		/* Copy Header */
		memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
		free(cntrl_response_header);
		/* Copy Payload */
		memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
		

		//CREATE SOCKET TO SEND INFO
		/*
	    int sock;
	    struct sockaddr_in control_addr;
	    socklen_t addrlen = sizeof(control_addr);

	    sock = socket(AF_INET, SOCK_DGRAM, 0);
	    if(sock < 0)
	        ERROR("socket() failed");*/

	    /* Make socket re-usable */
	    /*
	    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
	        ERROR("setsockopt() failed");

	    bzero(&control_addr, sizeof(control_addr));

	    control_addr.sin_family = AF_INET;
	    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	    control_addr.sin_port = htons(my_router_port);

		sendALL(sock, cntrl_response, response_len);
		*/

		int sockfd;
		struct addrinfo hints, *servinfo, *p;
		int rv;
		int numbytes;
		char buf[5];

		snprintf(buf,5,"%d",router_itr->router_port);
		printf("Neighbour PORT: %s bytes\n", buf);


		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		//if ((rv = getaddrinfo(router_itr->router_ip, itoa(router_itr->router_port), &hints, &servinfo)) != 0) {
		if ((rv = getaddrinfo(router_itr->router_ip, buf, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return;
		}
		// loop through all the results and make a socket
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("talker: socket");
				continue;
			}
			break;
		}
		if (p == NULL) {
			fprintf(stderr, "talker: failed to create socket\n");
			return;
		}
		if ((numbytes = sendto(sockfd, cntrl_response, response_len, 0, p->ai_addr, p->ai_addrlen)) == -1) {
			perror("talker: sendto");
			exit(1);
		}
		freeaddrinfo(servinfo);
		printf("talker: sent %d bytes\n", numbytes);
		close(sockfd);
		free(cntrl_response);
		//close(sock);
	} 
	free(cntrl_response_payload);
}