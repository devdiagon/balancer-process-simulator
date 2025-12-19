#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "balancer.h"

/* ==========================
   Variables globales
   ========================== */

connection_t conn_table[MAX_CONNECTIONS];
int conn_count = 0;
int active_conn_isp1 = 0;
int active_conn_isp2 = 0;

pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rr_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lc_mutex = PTHREAD_MUTEX_INITIALIZER;

static int rr_counter = 0;

/* ==========================
   Selección de enlace
   ========================== */

int select_link_round_robin() {
    pthread_mutex_lock(&rr_mutex);
    int link = (rr_counter++ % 2) + 1;
    pthread_mutex_unlock(&rr_mutex);
    return link;
}

int select_link_least_connection() {
    pthread_mutex_lock(&lc_mutex);

    int link;
    if (active_conn_isp1 <= active_conn_isp2) {
        active_conn_isp1++;
        link = 1;
    } else {
        active_conn_isp2++;
        link = 2;
    }

    pthread_mutex_unlock(&lc_mutex);
    return link;
}

/* ==========================
   Conectar a ISP
   ========================== */

int connect_to_isp(int link_id) {
    int sockfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket ISP");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ISP_PORT);

    if (link_id == 1)
        inet_pton(AF_INET, ISP1_IP, &addr.sin_addr);
    else
        inet_pton(AF_INET, ISP2_IP, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect ISP");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* ==========================
   Reenvío bidireccional
   ========================== */

void relay_traffic(int client_fd, int server_fd, int index) {
    char buffer[BUFFER_SIZE];
    fd_set fds;
    int maxfd = (client_fd > server_fd ? client_fd : server_fd) + 1;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(server_fd, &fds);

        if (select(maxfd, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        /* Cliente -> ISP */
        if (FD_ISSET(client_fd, &fds)) {
            int n = read(client_fd, buffer, sizeof(buffer));
            if (n <= 0) break;

            write(server_fd, buffer, n);

            pthread_mutex_lock(&table_mutex);
            conn_table[index].bytes_tx += n;
            pthread_mutex_unlock(&table_mutex);
        }

        /* ISP -> Cliente */
        if (FD_ISSET(server_fd, &fds)) {
            int n = read(server_fd, buffer, sizeof(buffer));
            if (n <= 0) break;

            write(client_fd, buffer, n);

            pthread_mutex_lock(&table_mutex);
            conn_table[index].bytes_rx += n;
            pthread_mutex_unlock(&table_mutex);
        }
    }
}

/* ==========================
   Hilo por conexión
   ========================== */

void *handle_connection(void *arg) {
    int client_fd = (intptr_t)arg;

    //int link = select_link_round_robin();
    int link = select_link_least_connection();
    int server_fd = connect_to_isp(link);
    if (server_fd < 0) {
        close(client_fd);
        return NULL;
    }

    pthread_mutex_lock(&table_mutex);
    int index = conn_count++;
    conn_table[index].client_fd = client_fd;
    conn_table[index].server_fd = server_fd;
    conn_table[index].link_id = link;
    conn_table[index].bytes_tx = 0;
    conn_table[index].bytes_rx = 0;
    pthread_mutex_unlock(&table_mutex);

    //printf("[+] Nueva conexión → eth_out%d (slot %d)\n", link, index);
    printf("[+] Nueva conexión → eth_out%d (slot %d) | Carga: ISP1=%d ISP2=%d\n",link, index, active_conn_isp1, active_conn_isp2);

    relay_traffic(client_fd, server_fd, index);

    pthread_mutex_lock(&table_mutex);
    printf("[-] Conexión cerrada (%d): TX=%zu RX=%zu\n",
           index,
           conn_table[index].bytes_tx,
           conn_table[index].bytes_rx);
    pthread_mutex_unlock(&table_mutex);

    pthread_mutex_lock(&lc_mutex);
    if (link == 1)
      active_conn_isp1--;
    else
      active_conn_isp2--;
    pthread_mutex_unlock(&lc_mutex);

    close(client_fd);
    close(server_fd);
    return NULL;
}

/* ==========================
   main
   ========================== */

int main() {
    int listen_fd;
    struct sockaddr_in addr;

    signal(SIGPIPE, SIG_IGN);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 128) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("=== Balanceador escuchando en puerto %d ===\n", LISTEN_PORT);

    while (1) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_connection, (void *)(intptr_t)client_fd);
        pthread_detach(tid);
    }

    close(listen_fd);
    return 0;
}
