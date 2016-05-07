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
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/queue.h>

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/network_util.h"
#include "../include/init.h"
#include "../include/routing.h"

void routing_table_response(int sock_index, char *cntrl_payload)
{
	int i = 0;
	uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

	payload_len = num_router*sizeof uint16_t*4; // Four fields of 16 bits per router
	cntrl_response_payload = (char *) malloc(payload_len);

	LIST_FOREACH(router_itr, &router_list, next) {	    
        printf("ROUTER_ID:%d ROUTER_PORT:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d TABLE_ID:%d\n",router_itr->router_id, router_itr->router_port, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop, router_itr->table_id);
        
        memcpy(cntrl_response_payload + (i * 16), htons(router_itr->router_id), 2);
        memcpy(cntrl_response_payload + (i * 16) + 2, htons(0), 2);
        memcpy(cntrl_response_payload + (i * 16) + 4, htons(router_itr->next_hop), 2);
        memcpy(cntrl_response_payload + (i * 16) + 6, htons(router_itr->cost), 2);
        i++;
    }	

	cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);

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