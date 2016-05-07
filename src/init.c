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

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/network_util.h"
#include "../include/init.h"
#include "../include/routing.h"
 #include "../include/connection_manager.h"

#define INF 65535

unsigned num_routers, route_table[5][5], my_router_id, my_table_id;

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

	printf("ROUTERS:%d UPDATES:%d\n",no_routers, updates_periodic_interval);

    for (int i = 0; i < no_routers; i++){
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

	    if (cost == 0){
	    	my_router_id = router_id;
	    	my_table_id = no_routers - i - 1;

	    }

		memcpy(&ip, cntrl_payload + 12 + (i*12), 4);
	    router_ip = inet_ntoa(ip);
	    r->router_ip = (char *)malloc(strlen(router_ip));
	    strcpy(r->router_ip,router_ip);

	    r->table_id = no_routers-i-1;
	    LIST_INSERT_HEAD(&router_list, r, next);
    }

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++){
			route_table[i][j] = INF;
		}
	}

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

    //SEND RESPONSE
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

}