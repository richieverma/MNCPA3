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
 * ROUTING-TABLE [Control Code: 0x02]
 * UPDATE [Control Code: 0x03]
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
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
#include "../include/routing.h" 

extern void packi16(unsigned char *buf, unsigned int i);
extern void packi32(unsigned char *buf, unsigned long int i);

extern struct routerInit *me;
extern struct timeval timeout;

char *routing_response;
//extern unsigned updates_periodic_interval;

//extern LIST_HEAD(trackUpdateList, routerInit) track_update_list;

void routing_table_response(int sock_index)
{
	int i = 0;
	uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response, *buf = (char *)malloc(16);

	payload_len = num_routers*sizeof(uint16_t)*4; // Four fields of 16 bits per router
	cntrl_response_payload = (char *) malloc(payload_len);

	LIST_FOREACH(router_itr, &router_list, next) {	    
        printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        
        packi16(buf, router_itr->router_id);
        memcpy(cntrl_response_payload + (i * 8), buf, 2);
        packi16(buf, 0);
        memcpy(cntrl_response_payload + (i * 8) + 2, buf, 2);
        packi16(buf, router_itr->next_hop);
        memcpy(cntrl_response_payload + (i * 8) + 4, buf, 2);
        packi16(buf, router_itr->cost);
        memcpy(cntrl_response_payload + (i * 8) + 6, buf, 2);
        i++;
    }	

	cntrl_response_header = create_response_header(sock_index, 2, 0, payload_len);

	response_len = CNTRL_RESP_HEADER_SIZE+payload_len;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);
	/* Copy Payload */
	memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
	free(cntrl_response_payload);

	sendALL(sock_index, cntrl_response, response_len);

	free(cntrl_response);    
}

void update_response(int sock_index, char *cntrl_payload)
{
}

void update_routing_table(char *source_ip, uint16_t source_router_port, int updated_fields, char *routing_update){

	uint16_t router_id, cost, router_table_id;
	struct routerInit *sender = NULL, *routerCostChange = NULL;
	//extern int my_table_id;
	extern unsigned route_table[5][5];

	//Find table id of router who has sent the update
	LIST_FOREACH(router_itr, &router_list, next) {	
		if (router_itr->router_port == source_router_port){
			sender = router_itr;
			struct timeval t;
			gettimeofday(&t, NULL);
			sender->last_update_time = t;			
			sender->missed_updates = 0;
			printf("Source Router Table ID:%d ROUTER_ID:%d IP:%s SOURCE ROUTER PORT:%d\n", sender->table_id, router_itr->router_id, router_itr->router_ip, source_router_port);
			break;
		}
	}

	//Update routing table for every router
	for (int i = 0; i < updated_fields; i++){
		memcpy(&router_id, routing_update + 8 +  8 + (i * 12), 2);
		router_id = ntohs(router_id);
		memcpy(&cost, routing_update + 8 +  10 + (i * 12), 2);
		cost = ntohs(cost);

		LIST_FOREACH(router_itr, &router_list, next) {	
			if (router_itr->router_id == router_id){
				router_table_id = router_itr->table_id;
				break;
			}
		}
		printf("Router ID:%d TABLE_ID:%d COST:%d\n", router_id, router_table_id, cost);
		route_table[sender->table_id][router_table_id] = cost;
	}

	//Updated Route Table
	for (int i = 0; i < num_routers; i++) {
		for (int j = 0; j < num_routers; j++){
			printf("%d ", route_table[i][j]);
		}
		printf("\n");
	}

	//Calculate change in distance cost to destinations based on the update
	unsigned original_cost, new_cost;
	for (int i = 0; i < num_routers; i++) {
		original_cost = route_table[me->table_id][i];
		new_cost = route_table[me->table_id][sender->table_id] + route_table[sender->table_id][i];
		if ( original_cost > new_cost){
			//Find the router whose cost has changed
			LIST_FOREACH(router_itr, &router_list, next) {	
				if (router_itr->table_id == i){
					router_itr->cost = new_cost;
					router_itr->next_hop = sender->router_id;
					routerCostChange = router_itr;
					printf("Cost CHANGE Router Table ID:%d ROUTER_ID:%d NEXT HOP:%d COST:%d\n", router_itr->table_id, router_itr->router_id, router_itr->next_hop, router_itr->cost);
					LIST_INSERT_HEAD(&route_update_list, routerCostChange, update);
					break;
				}
			}
			printf("Shorter path available to %dth node via sender. ORIG:%d NEW:%d\n",i,original_cost,new_cost);
			route_table[me->table_id][i] = new_cost;	
		}
	}
}

void populate_update_routing_packet(uint16_t *response_len){

	uint16_t num_update_fields = 0;
	char *buf = (char *)malloc(16);
	struct sockaddr_in sa;
	char str[INET_ADDRSTRLEN];
/*
	LIST_FOREACH(router_itr, &route_update_list, update) {
		num_update_fields++;
	}
*/
	LIST_FOREACH(router_itr, &router_list, next) {
		printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
		num_update_fields++;
	}

	printf("num_update_fields:%d\n",num_update_fields);

	*response_len = 8 + num_update_fields*sizeof(uint16_t)*6; // Four fields of 2 bytes per router, AND 1 field of 4 bytes, 8 bytes for num_update_fields, source router port, source ip address
	routing_response = (char *) malloc(*response_len);
	bzero(routing_response, sizeof(routing_response));

	//Number of update fields. Since this is first packet, routing info will be updated for all routers
    packi16(buf, num_update_fields);
    memcpy(routing_response, buf, 2);

    //Source router port
	packi16(buf, me->router_port);
	memcpy(routing_response + 2, buf, 2);

	//Source Router IP
	inet_pton(AF_INET, me->router_ip, &(sa.sin_addr));
	memcpy(routing_response + 4, &(sa.sin_addr), sizeof(struct in_addr));
    
	//LIST_FOREACH(router_itr, &route_update_list, update) {
	num_update_fields = 0;
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
        
        printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        num_update_fields++;
    } 
    free(buf);

    //while (route_update_list.lh_first != NULL){          /* Delete. */
    //	LIST_REMOVE(route_update_list.lh_first, update);
    //}
    //return routing_response;
}

void send_update_routing_packet(){
    //hexDump ("ROUTER UPDATE 60 BYTES:", routing_response, response_len);
    uint16_t response_len;
	populate_update_routing_packet(&response_len);

    LIST_FOREACH(router_itr, &router_neighbour_list, neighbour) {
		//Reference Beej's Guide Section 6.3
		int sockfd;
		struct addrinfo hints, *servinfo, *p;
		int rv;
		unsigned numbytes;
		char nbuf[5];

		snprintf(nbuf,5,"%d",router_itr->router_port);
		printf("Neighbour PORT: %s\n", nbuf);
		printf("Size of routing packet: %d\n", response_len);

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
		printf("Sent %d bytes in total. Out of:%d\n", numbytes, response_len);
		close(sockfd);
		printf("Sent and socket closed\n");
		//close(sock);
	} 
	free(routing_response);
}

void set_new_timeout(){
	struct timeval min, now;
	int flag = 0;
	struct routerInit *m;

	LIST_FOREACH(router_itr, &track_update_list, track){
		printf("Router ID:%d TIME:%ld\n", router_itr->router_id, (router_itr->last_update_time.tv_sec*1000000 + router_itr->last_update_time.tv_usec));
		if (flag == 0){
			min.tv_usec = router_itr->last_update_time.tv_usec;
			min.tv_sec = router_itr->last_update_time.tv_sec;
			m = router_itr;
			flag = 1;
			continue;
		}
		if (router_itr->last_update_time.tv_sec*1000000+router_itr->last_update_time.tv_usec < (min.tv_sec*1000000 + min.tv_usec)){
			min.tv_usec = router_itr->last_update_time.tv_usec;
			min.tv_sec = router_itr->last_update_time.tv_sec;
			m = router_itr;
		}
	}
	gettimeofday(&now, NULL);
	printf("Next update by router:%d\n", m->router_id);
	unsigned long t;
	t = updates_periodic_interval - (((now.tv_sec * 1000000) + now.tv_usec) - ((min.tv_sec * 1000000) + min.tv_usec))/1000000;
	printf("Time remaining:%ld\n", t);
	timeout.tv_sec = t;
	t = ((now.tv_sec * 1000000 + now.tv_usec) - (min.tv_sec * 1000000 + min.tv_usec))%1000000;
	printf("Time remaining usec:%ld\n", t);
	timeout.tv_usec = t;

	if (m == me){
		me->last_update_time = now;
		send_update_routing_packet();		
	}
	else{
		m->last_update_time = now;
		m->missed_updates += 1;
	}



}