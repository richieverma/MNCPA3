#include <sys/queue.h>

struct routerInit{
	unsigned router_id;
	unsigned next_update; 
	unsigned router_port;
	unsigned data_port;
	unsigned cost;
	unsigned next_hop;
	unsigned table_id;
	char *router_ip;
	LIST_ENTRY(routerInit) next;
}*router_itr;

void init_response(int sock_index, char *cntrl_payload);