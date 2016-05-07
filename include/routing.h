
LIST_HEAD(routerInitHead, routerInit) router_list; 
LIST_HEAD(routerInitNeighbourHead, routerInit) router_neighbour_list; 
extern unsigned num_routers, route_table[5][5], my_router_id, my_table_id, my_data_port, my_router_port;

void routing_table_response(int sock_index);
void update_response(int sock_index, char *cntrl_payload);