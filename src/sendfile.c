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

extern LIST_HEAD(routerInitHead, routerInit) router_list;

//SENDFILE CONTROL CODE 5
void sendfile_response(int sock_index, char *cntrl_payload, int payload_len)
{
	printf("\n-------SENDFILE-----------\n");	
	char *dest_ip, *filename, *dip, *dest_port;
	uint8_t ttl, transfer_id, filename_length;
	uint16_t seqnum;
	struct in_addr ip;
	struct routerInit *destination;

	filename_length = payload_len - 8;
	filename = malloc(filename_length);

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

	memcpy(filename, cntrl_payload + 8, filename_length);

	printf("DEST IP:%s TTL:%d TransferID:%d SEQ:%d FILENAME:%s\n",dest_ip, ttl,transfer_id, seqnum, filename);

	//Find destination router ID
	LIST_FOREACH(router_itr, &router_list, next) {
		if (strcmp(dest_ip, router_itr->router_ip) == 0){
        	printf("MATCH: ROUTER_ID:%d DATA_PORT:%d COST:%d ROUTER_IP:%s NEXT_HOP:%d\n",router_itr->router_id, router_itr->data_port, router_itr->cost, router_itr->router_ip, router_itr->next_hop);
        	destination = router_itr;
        	break;
    	}
    }	

	//TODO SEND FILE
	//Create data socket to send
    int sockfilesend = create_tcp_conn(dest_ip, destination->data_port);
            
            
    //READ FILE AND SEND TO THIS SOCKET
    int f = open(filename, O_RDONLY);
    struct stat file_stat;
    fstat(f, &file_stat);
    char file_details[50];
    sprintf(file_details,"<FILE> %d %s",file_stat.st_size, filename);
            
    //Send filesize, filename to client
    send(sockfilesend, file_details, strlen(file_details), 0);
    usleep(100000);
            
    int bytes_sent = 0; // Sent so far
    int to_be_sent = file_stat.st_size; //Total bytes to be sent
    off_t offset = 0;
            
    /* Keep sending data till there is no data left to be sent ie to_be_sent=0 */
    while (((bytes_sent = sendfile(sockfilesend, f, &offset, 256)) > 0) && (to_be_sent > 0))
    {
        to_be_sent -= bytes_sent;
    }    



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

void sendfile_stats_response(int sock_index, char *cntrl_payload)
{
}

void last_data_packet_response(int sock_index, char *cntrl_payload)
{
}

void penultimate_data_packet_response(int sock_index, char *cntrl_payload)
{
}