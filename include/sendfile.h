#include <sys/queue.h>

struct sendfileStats{
	uint8_t transfer_id;
	uint8_t ttl;
	uint16_t seq;
	char packet[1036]; 
	LIST_ENTRY(sendfileStats) next;
};

LIST_HEAD(sendfileStatsList, sendfileStats) sendfile_stats_list;

void sendfile_response(int sock_index, char *cntrl_payload, int payload_len);
void sendfile_stats_response(int sock_index, char *cntrl_payload);
void last_data_packet_response(int sock_index, char *cntrl_payload);
void penultimate_data_packet_response(int sock_index, char *cntrl_payload);