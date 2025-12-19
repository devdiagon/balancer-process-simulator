#include <pthread.h>

#ifndef BALANCER_H
#define BALANCER_H

#define MAX_CONNECTIONS 1024
#define BUFFER_SIZE 4096
#define LISTEN_PORT 8080

#define ISP1_IP "10.0.1.2"
#define ISP2_IP "10.0.2.2"
#define ISP_PORT 80

typedef struct {
    int client_fd;
    int server_fd;
    int link_id;               // 1 = eth_out1; 2 = eth_out2
    size_t bytes_tx;
    size_t bytes_rx;
} connection_t;

/* Tabla global de conexiones */
extern connection_t conn_table[MAX_CONNECTIONS];
extern int conn_count;
extern int active_conn_isp1;
extern int active_conn_isp2;

/* Mutex */
extern pthread_mutex_t table_mutex;
extern pthread_mutex_t rr_mutex;
extern pthread_mutex_t lc_mutex;

/* Funciones */
int select_link_round_robin();
int select_link_least_connection();
void *handle_connection(void *arg);
int connect_to_isp(int link_id);
void relay_traffic(int client_fd, int server_fd, int index);

#endif
