/**
 * @connection_manager
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
 * Connection Manager listens for incoming connections/messages from the
 * controller and other routers and calls the desginated handlers.
 */

#include <sys/select.h>
 #include <sys/socket.h>
 #include <strings.h>
 #include <string.h>

#include "../include/connection_manager.h"
#include "../include/global.h"
#include "../include/control_handler.h"
#include "../include/control_header_lib.h"

fd_set master_list, watch_list;
int head_fd;

void main_loop()
{
    int selret, sock_index, fdaccept;

    while(TRUE){
        watch_list = master_list;
        selret = select(head_fd+1, &watch_list, NULL, NULL, NULL);

        if(selret < 0)
            ERROR("select failed.");

        /* Loop through file descriptors to check which ones are ready */
        for(sock_index=0; sock_index<=head_fd; sock_index+=1){

            if(FD_ISSET(sock_index, &watch_list)){

                /* control_socket */
                if(sock_index == control_socket){
                    fdaccept = new_control_conn(sock_index);

                    /* Add to watched socket list */
                    FD_SET(fdaccept, &master_list);
                    if(fdaccept > head_fd) head_fd = fdaccept;

                    printf("\n----------------New control socket----------------\n");
                }

                /* router_socket */
                else if(sock_index == router_socket){
                    //call handler that will call recvfrom() .....
                    printf("\n----------------New router data----------------\n");
                    struct sockaddr_storage their_addr;
                    char *cntrl_header, *payload;
                    socklen_t addr_len;
                    uint16_t update_len;
                    unsigned numbytes = 0;

                    /* Get control header */
                    cntrl_header = (char *) malloc(sizeof(char)*CNTRL_HEADER_SIZE);
                    bzero(cntrl_header, CNTRL_HEADER_SIZE);

                    addr_len = sizeof their_addr;
                    if (recvfrom(sock_index, cntrl_header, CNTRL_HEADER_SIZE , 0, (struct sockaddr *)&their_addr, &addr_len) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }
                    memcpy(&update_len, cntrl_header, sizeof(update_len));

                    int payload_length = update_len*12;
                    payload = (char *) malloc(sizeof(char)*payload_length);
                    printf("\nPayload Length: %d\n", payload_length);
                    while (numbytes += recvfrom(sock_index, payload, payload_length + numbytes , 0, (struct sockaddr *)&their_addr, &addr_len) < payload_length);



                }

                /* data_socket */
                else if(sock_index == data_socket){
                    //new_data_conn(sock_index);
                }

                /* Existing connection */
                else{
                    if(isControl(sock_index)){
                        if(!control_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);
                    }
                    //else if isData(sock_index);
                    else ERROR("Unknown socket index");
                }
            }
        }
    }
}

void init()
{
    printf("\nINIT\n");
    control_socket = create_control_sock();

    //router_socket and data_socket will be initialized after INIT from controller

    FD_ZERO(&master_list);
    FD_ZERO(&watch_list);

    /* Register the control socket */
    FD_SET(control_socket, &master_list);
    head_fd = control_socket;

    main_loop();
}