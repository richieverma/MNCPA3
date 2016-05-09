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
 * SENDFILE [Control Code: 0x05]
 * SENDFILE-STATS [Control Code: 0x06]
 * LAST-DATA-PACKET [Control Code: 0x07]
 * PENULTIMATE-DATA-PACKET [Control Code: 0x08]
 */

#include <string.h>
#include <sys/socket.h> //inet_ntoa
#include <netinet/in.h> //inet_ntoa
#include <arpa/inet.h> //inet_ntoa
#include <netinet/in.h> //in_addr ntohs
#include <unistd.h> //usleep

#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/control_handler.h"
#include "../include/network_util.h"
#include "../include/init.h"
 #include "../include/sendfile.h"

extern LIST_HEAD(routerInitHead, routerInit) router_list;
extern void hexDump (char *desc, void *addr, int len);
extern unsigned dest_data_sockets[5];

extern void packi16(unsigned char *buf, unsigned int i);
extern void packi32(unsigned char *buf, unsigned long int i);

//SENDFILE CONTROL CODE 5
void sendfile_response(int sock_index, char *cntrl_payload, uint16_t payload_len)
{
	printf("\n-------SENDFILE %d-----------\n", payload_len);	

	char *dest_ip, *filename, *dip, *dest_port;
	uint8_t ttl, transfer_id;
	uint16_t seqnum, filename_length;
	struct in_addr ip;
	struct routerInit *destination, *next_hop_router;
	//FILE *f_send;

	filename_length = payload_len - 8 + 1;
	filename = malloc(filename_length);
	memset(filename, 0, filename_length);

	memcpy(&ip, cntrl_payload, 4);
    dip = inet_ntoa(ip);
	dest_ip = (char *)malloc(strlen(dip));
    strcpy(dest_ip,dip);

	memcpy(&ttl, cntrl_payload + 4, 1);
    //ttl = ntohs(ttl);

	memcpy(&transfer_id, cntrl_payload + 5, 1);
	//transfer_id = ntohs(transfer_id);	

	memcpy(&seqnum, cntrl_payload + 6, 2);
	seqnum = ntohs(seqnum);

	snprintf(filename, filename_length, "%s", cntrl_payload+8);

	printf("DEST IP:%s TTL:%d TransferID:%d SEQ:%d FILENAME:%s\n",dest_ip, ttl,transfer_id, seqnum, filename);

	//Find destination router ID
	struct routerInit *router_itr, *router_itr1;
	LIST_FOREACH(router_itr, &router_list, next) {
		printf("ROUTER_ID:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d\n",router_itr->router_id, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop);
		if (strcmp(dest_ip, router_itr->router_ip) == 0){
        	printf("DEST MATCH: ROUTER_ID:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d\n",router_itr->router_id, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop);
        	destination = router_itr;
        	break;
    	}
    }


	//Find details of next hop
	LIST_FOREACH(router_itr1, &router_list, next) {
		if (destination->next_hop == router_itr1->router_id){
        	printf("NEXT HOP MATCH: ROUTER_ID:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d\n",router_itr1->router_id, router_itr1->data_port, router_itr1->cost, router_itr1->router_ip, router_itr1->next_hop);
        	next_hop_router = router_itr1;
        	break;
    	}
    }    	

	//TODO SEND FILE
	//Create data socket to send
	printf("Creating new socket for file transfer to ROUTER:%d\n", next_hop_router->router_id);
    //int sockfilesend = create_tcp_conn(next_hop_router->router_ip, next_hop_router->data_port);
    int sockfilesend = dest_data_sockets[next_hop_router->table_id];
    if (sockfilesend <= 0){
    	sockfilesend = create_tcp_conn(next_hop_router->router_ip, next_hop_router->data_port);
    	dest_data_sockets[next_hop_router->table_id] = sockfilesend;
    }
    
            
    //READ FILE AND SEND TO THIS SOCKET
	FILE *fsend = fopen(filename, "r");

    if (fsend == NULL){
    	printf("FILENAME:%s\n", filename);
    	perror("ERROR. FILE COULD NOT BE OPENED.\n");
    	return;
    }
            
    int bytes_sent = 0, bytes_read = 0, send_flg = 1; //Total bytes to be sent
	char buffer[1024], *buf16 = (char *)malloc(16), *buf32 = (char *)malloc(32), *routing_response;
	struct sockaddr_in sa;

	routing_response = malloc(12+1024);

	//Populate routing response
	//Destination Router IP
	inet_pton(AF_INET, destination->router_ip, &(sa.sin_addr));
	memcpy(routing_response, &(sa.sin_addr), sizeof(struct in_addr));

	//Transfer ID
	memcpy(routing_response + 4, &transfer_id, 1);		

	//TTL
	memcpy(routing_response + 5, &ttl, 1);			

	fread(buffer, 1, 1024, fsend);

    /* Keep sending data till there is no data left to be sent ie to_be_sent=0 */
    while (send_flg == 1)
    {

    	memcpy(routing_response + 12, buffer, 1024);

    	if (fread(buffer, 1, 1024, fsend) != 1024){
    		printf("LAST PACKET SENDING\n");
    		packi32(buf32, 1<<31);	
			memcpy(routing_response + 8, buf32, 4);	
			send_flg = 0;		

		}
		else{
    		packi32(buf32, 0);	
			memcpy(routing_response + 8, buf32, 4);				
		
    	}
		//Read from file
    	//memset(buffer, 0, sizeof buffer);
    	bytes_read += 1024;

		//SEQNUM
    	packi16(buf16, seqnum);	
		memcpy(routing_response + 6, buf16, 2);
/*
		if (to_be_read <= 0){
			//Last packet. Set FIN bit
			printf("LAST PACKET SENDING\n");
    		packi32(buf32, 1<<31);	
			memcpy(routing_response + 8, buf32, 4);			

		}
		else{
    		packi32(buf32, 0);	
			memcpy(routing_response + 8, buf32, 4);				
		}*/

		

		bytes_sent = sendALL(sockfilesend, routing_response, 12+1024);

    	//Add packet for stats
		struct sendfileStats *s = malloc(sizeof (struct sendfileStats));
		s->transfer_id = transfer_id;
		s->ttl = ttl;
		s->seq = seqnum;
		strcpy(s->packet, routing_response);
		LIST_INSERT_HEAD(&sendfile_stats_list, s, next);

		seqnum += 1;
		//hexDump("SENDFILE:",routing_response, 12+1024);
        //printf("SENDING INFO:%s\n", buffer);
        //printf("SENDING FILE, BYTES SENT:%d\n", bytes_sent);

    }
    printf("FILE SENT\n");
    fclose(fsend);
    //close(sockfilesend);


	//SEND CONTROL RESPONSE WITH CONTROL CODE 5 AFTER LAST PACKET IS SENT
	uint16_t response_len;
	char *cntrl_response_header, *cntrl_response;

	cntrl_response_header = create_response_header(sock_index, 5, 0, 0);

	response_len = CNTRL_RESP_HEADER_SIZE;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);

	sendALL(sock_index, cntrl_response, response_len);

	free(cntrl_response);	
}

//CONTROL CODE 6
void sendfile_stats_response(int sock_index, char *cntrl_payload)
{
	uint8_t transfer_id, num_packets = 0, i = 0, ttl = 0;
	char *buf16 = (char *)malloc(16);
	struct sendfileStats *s;

	memcpy(&transfer_id, cntrl_payload, 1);

	printf("Transfer ID:%d \n", transfer_id);

	LIST_FOREACH(s, &sendfile_stats_list, next){
		if (s->transfer_id == transfer_id){
			num_packets++;
			ttl = s->ttl;
		}
	}
	printf("TTL:%d\n", ttl);
	uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

	payload_len = 4 + num_packets * 2;
	cntrl_response_payload = malloc(payload_len);

	memcpy(cntrl_response_payload, &transfer_id, 1);
	memcpy(cntrl_response_payload + 1, &ttl, 1);
	packi16(buf16, 0);
	memcpy(cntrl_response_payload + 2, buf16, 2);

	LIST_FOREACH(s, &sendfile_stats_list, next){
		if (s->transfer_id == transfer_id){
			printf("SEQ:%d\n", s->seq);
			packi16(buf16, s->seq);
			memcpy(cntrl_response_payload + 4 + (i * 2), buf16, 2);
			i++;
		}
	}	

	cntrl_response_header = create_response_header(sock_index, 6, 0, payload_len);

	response_len = CNTRL_RESP_HEADER_SIZE + payload_len;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);
	/* Copy Payload */
	memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
	free(cntrl_response_payload);

	sendALL(sock_index, cntrl_response, response_len);
	//hexDump("SENDFILE STATS", cntrl_response, response_len);

	free(cntrl_response);    

}

//CONTROL CODE 7
void last_data_packet_response(int sock_index)
{
	struct sendfileStats *s;
	uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

	payload_len = 1036;
	cntrl_response_payload = malloc(payload_len);

	LIST_FOREACH(s, &sendfile_stats_list, next){
		memcpy(cntrl_response_payload, s->packet, payload_len);
		break;
	}	

	cntrl_response_header = create_response_header(sock_index, 7, 0, payload_len);

	response_len = CNTRL_RESP_HEADER_SIZE + payload_len;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);
	/* Copy Payload */
	memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
	free(cntrl_response_payload);

	sendALL(sock_index, cntrl_response, response_len);
	//hexDump("LAST DATA", cntrl_response, response_len);

	free(cntrl_response); 

}

//CONTROL CODE 8
void penultimate_data_packet_response(int sock_index)
{
	uint8_t i = 1;
	struct sendfileStats *s;
	uint16_t payload_len, response_len;
	char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

	payload_len = 1036;
	cntrl_response_payload = malloc(payload_len);

	LIST_FOREACH(s, &sendfile_stats_list, next){
		if (i == 2){
			memcpy(cntrl_response_payload, s->packet, payload_len);
			break;
		}
		i++;
		
	}	

	cntrl_response_header = create_response_header(sock_index, 7, 0, payload_len);

	response_len = CNTRL_RESP_HEADER_SIZE + payload_len;
	cntrl_response = (char *) malloc(response_len);
	/* Copy Header */
	memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
	free(cntrl_response_header);
	/* Copy Payload */
	memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
	free(cntrl_response_payload);

	sendALL(sock_index, cntrl_response, response_len);
	//hexDump("PENULTIMATE DATA", cntrl_response, response_len);
	free(cntrl_response);	

}