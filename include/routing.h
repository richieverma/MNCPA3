
LIST_HEAD(routerInitHead, routerInit) router_list; 
LIST_HEAD(routerInitNeighbourHead, routerInit) router_neighbour_list; 
LIST_HEAD(routeUpdateList, routerInit) route_update_list; 
LIST_HEAD(trackUpdateList, routerInit) track_update_list; 

unsigned num_routers, route_table[5][5], updates_periodic_interval;
struct routerInit *me;

void routing_table_response(int sock_index);
void update_response(int sock_index, char *cntrl_payload);
void update_routing_table(char *source_ip, uint16_t source_router_port, int num_update_fields, char *routing_update);
void populate_update_routing_packet(uint16_t *response_len);
void send_update_routing_packet();
void set_new_timeout();