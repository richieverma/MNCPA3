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

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/network_util.h"

void sendfile_response(int sock_index, char *cntrl_payload)
{
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