
LIST_HEAD(routerInitHead, routerInit) router_list; 
extern unsigned num_routers, route_table[5][5], my_router_id, my_table_id;

void routing_table_response(int sock_index, char *cntrl_payload);
void update_response(int sock_index, char *cntrl_payload);