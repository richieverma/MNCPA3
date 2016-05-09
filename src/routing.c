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
	extern unsigned route_table[5][5], orig_route_table[5][5];
	unsigned router_id, cost;	
	struct routerInit *r;

	memcpy(&router_id, cntrl_payload, 2);
    router_id = ntohs(router_id);

    memcpy(&cost, cntrl_payload + 2, 2);
    cost = ntohs(updates_periodic_interval);

    //Find router for whih cost has changed
    LIST_FOREACH(router_itr, &router_list, next) {	
    	if (router_itr->router_id == router_id){
    		r = router_itr;
    		break;
    	}
    }

    //Update original route table
    orig_route_table[me->table_id][r->table_id] = cost;
    orig_route_table[r->table_id][me->table_id] = cost;

    //Update cost for all destinations in route table
    calculate_cost_after_routing_update();

    //Send Control response for UPDATE with no payload
	uint16_t response_len;
	char *cntrl_response_header, *cntrl_response;

	cntrl_response_header = create_response_header(sock_index, 3, 0, 0);

	response_len = CNTRL_RESP_HEADER_SIZE;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, response_len);

	free(cntrl_response);
}

void update_routing_table(char *source_ip, uint16_t source_router_port, int updated_fields, char *routing_update){

	uint16_t router_id, cost, router_table_id;
	struct routerInit *sender = NULL, *m, *itr;
	int flag = 0;
	long t;
	struct timeval min, now;
	gettimeofday(&now, NULL);

	//extern int my_table_id;
	extern unsigned route_table[5][5], orig_route_table[5][5];


	//Find table id of router who has sent the update
	LIST_FOREACH(router_itr, &router_list, next) {	
		//if (router_itr->router_port == source_router_port){
		if (strcmp(router_itr->router_ip, source_ip) == 0){
			sender = router_itr;
			struct timeval tim;
			gettimeofday(&tim, NULL);
			tim.tv_sec += updates_periodic_interval;
			sender->next_update_time = tim;			
			sender->missed_updates = 0;
			printf("Source Router Table ID:%d ROUTER_ID:%d IP:%s SOURCE ROUTER PORT:%d\n", sender->table_id, router_itr->router_id, router_itr->router_ip, source_router_port);
			break;
		}
	}

	//Set new timeout
	LIST_FOREACH(itr, &track_update_list, track){
		printf("Router ID:%d TIME:%ld\n", itr->router_id, (itr->next_update_time.tv_sec*1000000 + itr->next_update_time.tv_usec));
		if (itr->next_update_time.tv_sec == 0 && itr->next_update_time.tv_usec == 0){
			printf("Waiting for first routing packet from router: %d\n", itr->router_id);
			continue;
		}

		if (flag == 0){
			min.tv_usec = itr->next_update_time.tv_usec;
			min.tv_sec = itr->next_update_time.tv_sec;
			m = itr;
			flag = 1;
			continue;
		}
		if (itr->next_update_time.tv_sec*1000000 + itr->next_update_time.tv_usec < (min.tv_sec*1000000 + min.tv_usec)){
			min.tv_usec = itr->next_update_time.tv_usec;
			min.tv_sec = itr->next_update_time.tv_sec;
			m = itr;
		}
	}
	
	printf("Next update by router:%d\n", m->router_id);


	t = (((min.tv_sec * 1000000) + min.tv_usec) - ((now.tv_sec * 1000000) + now.tv_usec))/1000000;
	if (t < 0){
		t = 1;
	}
	printf("Time remaining t:%ld\n", t);
	timeout.tv_sec = t;
	t = (((min.tv_sec * 1000000) + min.tv_usec) - ((now.tv_sec * 1000000) + now.tv_usec)) % 1000000;
	if (t < 0){
		t = 0;
	}
	printf("Time remaining usec:%ld\n", t);
	timeout.tv_usec = t;

	//Update routing table for every router from sender
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
		orig_route_table[sender->table_id][router_table_id] = cost;

	}

	//Updated Route Table
	for (int i = 0; i < num_routers; i++) {
		for (int j = 0; j < num_routers; j++){
			printf("%d ", route_table[i][j]);
		}
		printf("\n");
	}
	calculate_cost_after_routing_update();
}

void calculate_cost_after_routing_update(){
	//Calculate change in distance cost to destinations based on the update
	unsigned original_cost, new_cost, min_cost, hop;
	struct routerInit *router_it, *routerCostChange = NULL;

	for (int i = 0; i < num_routers; i++) {
		original_cost = orig_route_table[me->table_id][i];
		new_cost = 65535;
		min_cost = 65535;
		hop = 65535;
		//new_cost = route_table[me->table_id][sender->table_id] + route_table[sender->table_id][i];	

		//Find min cost from all neighbours
		LIST_FOREACH(router_it, &router_neighbour_list, neighbour) {
			new_cost = orig_route_table[me->table_id][router_it->table_id] + orig_route_table[router_it->table_id][i];	
			if (new_cost < min_cost){
				min_cost = new_cost;
				hop = router_it->router_id;
			}
		}

		
		if ( original_cost > min_cost){
			//Find the router whose cost has changed
			LIST_FOREACH(router_it, &router_list, next) {	
				if (router_it->table_id == i){
					router_it->cost = min_cost;
					if (hop == me->router_id){
						router_it->next_hop = router_it->router_id;	
					}
					else{
						router_it->next_hop = hop;
					}
					routerCostChange = router_it;
					printf("Cost CHANGE Router Table ID:%d ROUTER_ID:%d NEXT HOP:%d COST:%d\n", router_it->table_id, router_it->router_id, router_it->next_hop, router_it->cost);
					break;
				}
			}
			printf("Shorter path available to %dth node via sender:%d ORIG:%d NEW:%d\n",i, hop, original_cost,min_cost);
			route_table[me->table_id][i] = min_cost;	
		}
	}
}

void populate_update_routing_packet(uint16_t *response_len){

	uint16_t num_update_fields = 0;
	char *buf = (char *)malloc(16);
	struct sockaddr_in sa;
	char str[INET_ADDRSTRLEN];
	struct routerInit *router_itr1;
/*
	LIST_FOREACH(router_itr1, &route_update_list, update) {
		num_update_fields++;
	}
*/
	LIST_FOREACH(router_itr1, &router_list, next) {
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
    
	//LIST_FOREACH(router_itr1, &route_update_list, update) {
	num_update_fields = 0;
	LIST_FOREACH(router_itr1, &router_list, next) {
		// store this IP address in sa:
		inet_pton(AF_INET, router_itr1->router_ip, &(sa.sin_addr));
		memcpy(routing_response + 8 + (num_update_fields * 12), &(sa.sin_addr), sizeof(struct in_addr));

        packi16(buf, router_itr1->router_port);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 4, buf, 2);
        
        packi16(buf, 0);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 6, buf, 2);
        
        packi16(buf, router_itr1->router_id);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 8, buf, 2);
        
        packi16(buf, router_itr1->cost);
        memcpy(routing_response + 8 +  (num_update_fields * 12) + 10, buf, 2);			    
        
        printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr1->router_id, router_itr1->router_port, router_itr1->data_port, router_itr1->cost, router_itr1->router_ip, router_itr1->next_hop, router_itr1->table_id);
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
	struct routerInit *router_itr1;

    LIST_FOREACH(router_itr1, &router_neighbour_list, neighbour) {
    	printf("=========SENDING UPDATE TO ROUTER:%d\n", router_itr1->router_id);
		//Reference Beej's Guide Section 6.3
		int sockfd;
		struct addrinfo hints, *servinfo, *p;
		int rv;
		unsigned numbytes;
		char nbuf[5];

		//CREATE UDP SOCKET TO SEND ROUTE UPDATE
		snprintf(nbuf,5,"%d",router_itr1->router_port);
		printf("Neighbour PORT: %s\n", nbuf);

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		//if ((rv = getaddrinfo(router_itr1->router_ip, itoa(router_itr1->router_port), &hints, &servinfo)) != 0) {
		if ((rv = getaddrinfo(router_itr1->router_ip, nbuf, &hints, &servinfo)) != 0) {
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
		//close(sock);
	} 
	free(routing_response);
}

void set_new_timeout(){

	extern unsigned route_table[5][5];
	struct timeval min, now, next_time;
	int flag = 0, flag1 = 0, delay = 0, already_missed[] = {0,0,0,0,0};
	struct routerInit *m;

	gettimeofday(&now, NULL);

	printf("TIME NOW:%ld SEC:%ld\n", now.tv_sec*1000000 + now.tv_usec, now.tv_sec);

	next_time.tv_sec = now.tv_sec + updates_periodic_interval;
	next_time.tv_usec = now.tv_usec;

	
	unsigned long t = 0;

	LIST_FOREACH(router_itr, &track_update_list, track){
		printf("Router ID:%d TIME:%ld\n", router_itr->router_id, (router_itr->next_update_time.tv_sec*1000000 + router_itr->next_update_time.tv_usec));

		//if (((router_itr->next_update_time.tv_sec * 1000000) + router_itr->next_update_time.tv_usec) <= ((now.tv_sec*1000000) + now.tv_usec)){
		if (router_itr->next_update_time.tv_sec <= now.tv_sec){
			next_time.tv_sec += delay;
			delay += 2;
			
			if (router_itr->router_id == me->router_id){
				printf("Timeout passed already for ME with router ID:%d\n", router_itr->router_id);
				flag1 = 1;
				router_itr->next_update_time = next_time;
				send_update_routing_packet();	
			}
			else{
				
				router_itr->next_update_time = next_time;
				router_itr->missed_updates += 1;
				already_missed[router_itr->table_id] = 1;

				printf("Timeout passed Missed %d updates from:%u\n",router_itr->missed_updates, router_itr->router_id);
				if (router_itr->missed_updates >= 3){
					orig_route_table[me->table_id][router_itr->table_id] = 65535;
					router_itr->next_hop = 65535;
					LIST_REMOVE(router_itr, track);
					calculate_cost_after_routing_update(router_itr);
				}
			}
		}
	}

	LIST_FOREACH(router_itr, &track_update_list, track){
		printf("Router ID:%d TIME:%ld\n", router_itr->router_id, (router_itr->next_update_time.tv_sec*1000000 + router_itr->next_update_time.tv_usec));

		if (flag == 0){
			min.tv_usec = router_itr->next_update_time.tv_usec;
			min.tv_sec = router_itr->next_update_time.tv_sec;
			m = router_itr;
			flag = 1;
			continue;
		}
		if (router_itr->next_update_time.tv_sec*1000000+router_itr->next_update_time.tv_usec < (min.tv_sec*1000000 + min.tv_usec)){
			min.tv_usec = router_itr->next_update_time.tv_usec;
			min.tv_sec = router_itr->next_update_time.tv_sec;
			m = router_itr;
		}
	}
	
	printf("Next update by router:%d\n", m->router_id);


	t = (((min.tv_sec * 1000000) + min.tv_usec) - ((now.tv_sec * 1000000) + now.tv_usec))/1000000;
	if (t < 0){
		t = 1;
	}	
	printf("Time remaining t:%ld\n", t);
	timeout.tv_sec = t;
	t = (((min.tv_sec * 1000000) + min.tv_usec) - ((now.tv_sec * 1000000) + now.tv_usec)) % 1000000;
	if (t < 0){
		t = 0;
	}
	printf("Time remaining usec:%ld\n", t);
	timeout.tv_usec = t;

	if (m == me && flag1 == 0){
		me->next_update_time = next_time;
		printf("SEND PERIODIC ROUTING UPDATE router ID:%d\n", me->router_id);
		send_update_routing_packet();		
	}
	else if (m != me){
		if (already_missed[m->table_id] == 0){
			m->next_update_time = next_time;
			m->missed_updates += 1;
			printf("Missed %d updates from:%u\n",m->missed_updates, m->router_id);
			if (m->missed_updates >= 3){
				orig_route_table[me->table_id][m->table_id] = 65535;
				m->next_hop = 65535;
				LIST_REMOVE(m, track);
				calculate_cost_after_routing_update(m);
			}
		}
	}



}