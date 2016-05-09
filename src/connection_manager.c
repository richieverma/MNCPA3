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
#include <time.h>
#include <sys/time.h> 

#include "../include/connection_manager.h"
#include "../include/global.h"
#include "../include/control_handler.h"
#include "../include/control_header_lib.h"

fd_set master_list, watch_list;
int head_fd, flag_init = 0;
struct timeval timeout;

extern struct timeval set_new_timeout();

void main_loop()
{
    int selret, sock_index, fdaccept;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while(TRUE){
        watch_list = master_list;
        if (flag_init != 0) printf("\nTIME BEFORE NEXT TIMEOUT:%d sec\n", timeout.tv_sec);
        selret = select(head_fd+1, &watch_list, NULL, NULL, &timeout);

        if(selret < 0)
            ERROR("select failed.");

        /* Loop through file descriptors to check which ones are ready */
        if (selret == 0 && flag_init != 0){
            printf("\n----------------TIMEOUT----------------\n");  
            set_new_timeout();          
            continue;
        }

        for(sock_index=0; sock_index<=head_fd; sock_index+=1){

            if(FD_ISSET(sock_index, &watch_list)){
                printf("\n----------------Socket:%d----------------\n", sock_index);

                /* control_socket */
                if(sock_index == control_socket){
                    fdaccept = new_control_conn(sock_index);

                    /* Add to watched socket list */
                    FD_SET(fdaccept, &master_list);
                    if(fdaccept > head_fd) head_fd = fdaccept;

                    printf("\n----------------New control socket: %d----------------\n", fdaccept);
                }

                /* router_socket */
                else if(sock_index == router_socket){
                    //call handler that will call recvfrom() .....
                    printf("\n----------------New router data----------------\n");
                    router_recv_hook(sock_index);
                }

                /* data_socket */
                else if(sock_index == data_socket){
                    printf("\n----------------New Data Conn----------------\n");
                    fdaccept = new_data_conn(sock_index);

                    /* Add to watched socket list */
                    FD_SET(fdaccept, &master_list);
                    if(fdaccept > head_fd) head_fd = fdaccept;                    
                    //new_data_conn(sock_index);
                }

                /* Existing connection */
                else{
                    if(isControl(sock_index)){
                        printf("\n----------------Control Socket List----------------\n");
                        if(!control_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);
                    }
                    else if (isData(sock_index)){
                        printf("\n----------------Data Socket List----------------\n");
                        if(!data_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);    
                    }
                    else{
                        printf("---------------ERROR Unknown socket index %d", sock_index);
                    }
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