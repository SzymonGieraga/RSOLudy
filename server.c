#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <endian.h>

#define CONFIG_FILE "server.txt"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

pthread_t *threads[MAX_CLIENTS];
int thread_count = 0;


int read_config(int *port) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        printf("file error");
        return 0;
    }
    if ( fscanf(file, "%d", port) != 1) {
        printf("config error");
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    uint8_t buffer[BUFFER_SIZE];
    ssize_t received;

    while ((received = read(client_sock, buffer, BUFFER_SIZE)) > 0) {
        size_t pos = 0;
        while (pos < received) {
            uint8_t request_type = buffer[pos + 3];  // Request
            uint8_t rq_id = buffer[pos + 4];         // RQ ID

            if (request_type == 0x01) {  //pierw
                uint64_t num_bits;
                memcpy(&num_bits, &buffer[pos + 5], sizeof(uint64_t));

                // do big endian
                num_bits = be64toh(num_bits);

                double number;
                memcpy(&number, &num_bits, sizeof(double));

                printf("Od: %d Zapytanie (pierw): 0x%02X, RQ ID: %d, Value: %f\n",
                       client_sock,request_type, rq_id, number);

                double result = sqrt(number);
                printf("Od: %d: Wynik: %f\n", client_sock,result);

                uint8_t response[13];
                response[0] = 0x01;
                response[1] = 0x00;
                response[2] = 0x00;
                response[3] = 0x01;
                response[4] = rq_id;

                uint64_t result_bits;
                memcpy(&result_bits, &result, sizeof(double));
                result_bits = htobe64(result_bits);
                memcpy(&response[5], &result_bits, sizeof(uint64_t));

                write(client_sock, response, sizeof(response));
                pos += 13;
            }
            else if (request_type == 0x02) {
                printf("Zapytanie (czas), RQ ID: %d\n", rq_id);

                time_t now = time(NULL);
                struct tm *tm_info = localtime(&now);
                char time_str[20];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

                uint8_t len = strlen(time_str);

                uint8_t response[6 + len];
                response[0] = 0x01;
                response[1] = 0x00;
                response[2] = 0x00;
                response[3] = 0x02;
                response[4] = rq_id;
                response[5] = len;
                memcpy(&response[6], time_str, len);

               write(client_sock, response, sizeof(response));
                pos += 5;
            }
            else {
                printf("AJJJJJJJ: 0x%02X %zu\n", request_type, pos);
                pos += 1;
            }
        }
    }

    close(client_sock);
    printf("Klient sie rozłączył\n");
    return NULL;
}

int main() {

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        printf("Socket fail");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("setsockpt fail");
        close(server_sock);
        return 1;
    }
    int port;
    read_config(&port);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        printf("Bind error");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, MAX_CLIENTS) == -1) {
        printf("Listen error");
        close(server_sock);
        return 1;
    }

    printf("Serwer na porcie: %d...\n", port);

    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == -1) {
            printf("Accept fail");
            continue;
        }
        printf("Nowe polaczenie z: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        if (thread_count >= MAX_CLIENTS) {
            printf("Max klientow.\n");
            close(client_sock);
            continue;
        }

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, new_sock) != 0) {
            printf("Watek fail\n");
            free(new_sock);
            close(client_sock);
        } else {
            threads[thread_count++] = malloc(sizeof(pthread_t));
            *threads[thread_count - 1] = thread;
        }
    }
    for (int i = 0; i < thread_count; i++) {
        pthread_join(*threads[i], NULL);
        free(threads[i]);
    }

    close(server_sock);
    return 0;
}