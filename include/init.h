#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>

struct routerInit{
	unsigned router_id;
	unsigned missed_updates; 
	struct timeval next_update_time;
	unsigned router_port;
	unsigned data_port;
	unsigned cost;
	unsigned next_hop;
	unsigned table_id;
	char *router_ip;
	LIST_ENTRY(routerInit) next;
	LIST_ENTRY(routerInit) neighbour;
	LIST_ENTRY(routerInit) update;
	LIST_ENTRY(routerInit) track;
}*router_itr;

void init_response(int sock_index, char *cntrl_payload);
void send_initial_routing_packet(unsigned updates_periodic_interval);