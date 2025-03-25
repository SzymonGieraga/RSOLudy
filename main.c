#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <endian.h>

#define CONFIG_FILE "config.txt"
#define BUFFER_SIZE 1024

int read_config(char *server_ip, int *port) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        printf("file error");
        return 0;
    }
    if (fscanf(file, "%15s", server_ip) != 1 || fscanf(file, "%d", port) != 1) {
        printf("config error");
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

void send_request(int sock, uint8_t request_type, double number, uint8_t rq_id) {
    uint8_t buffer[13];

    buffer[0] = 0x00;
    buffer[1] = 0x00;
    buffer[2] = 0x00;
    buffer[3] = request_type;
    buffer[4] = rq_id;

    if (request_type == 0x01) {
        uint64_t num_bits;
        memcpy(&num_bits, &number, sizeof(double));

        num_bits = htobe64(num_bits);

        memcpy(&buffer[5], &num_bits, sizeof(uint64_t));

        printf("Wysłano liczbe: %f (W byte: %lu)\n", number, num_bits);
        write(sock, buffer, 13);
    }else if (request_type == 0x02) {
        write(sock, buffer, 5);
    }
}

void receive_response(int sock) {
    uint8_t buffer[BUFFER_SIZE];
    ssize_t received;

    received = read(sock, buffer, BUFFER_SIZE);
    if (received > 0) {
        uint8_t response_type = buffer[3];
        uint8_t rq_id = buffer[4];

        if (response_type == 0x01) {
            uint64_t result_bits;
            memcpy(&result_bits, &buffer[5], sizeof(uint64_t));

            result_bits = be64toh(result_bits);

            double result;
            memcpy(&result, &result_bits, sizeof(double));

            printf("Odpowiedź (RQ ID %d): Pierwiastek = %.2f\n", rq_id, result);
        }
        else if (response_type == 0x02) {
            uint8_t length = buffer[5];  // Changed from uint16_t to uint8_t

            char time_str[length + 1];
            memcpy(time_str, &buffer[6], length);  // Changed from buffer[7]
            time_str[length] = '\0';

            printf("Odpowiedź (RQ ID %d): Czas serwera = %s\n", rq_id, time_str);
        }
        else {
            printf("Nieznany typ odpowiedzi: 0x%02X\n", response_type);
        }
    } else {
        printf("Coś nie tak z odpowiedzia");
    }
}

int main() {
    char server_ip[16];
    int port;

    if (!read_config(server_ip, &port)) {
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Socket error");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        printf("Conn error");
        close(sock);
        return 1;
    }

    printf("Połączono na %s na porcie%d\n", server_ip, port);

    uint8_t rq_id = 1;
    while (1) {
        printf("\nWybierz:\n");
        printf("1 - PIERWIASTEK\n");
        printf("2 - CZAS\n");
        printf("0 - WYJSCIE\n");
        printf("WYBOR: ");

        int choice;
        scanf("%d", &choice);

        if (choice == 0) {
            printf("Klient END!\n");
            break;
        }

        if (choice == 1) {
            double number =0;
            while (1) {
                printf("Liczba: ");
                if (scanf("%lf", &number)!=1 || number < 0) {
                    printf("Podaj właściwą liczbę!\n");
                }else {
                    break;
                }
            }
            send_request(sock, 0x01, number, rq_id++);
        }
        else if (choice == 2) {
            send_request(sock, 0x02, 0, rq_id++);
        }
        else {
            printf("Invalid. \n");
            continue;
        }

        receive_response(sock);
    }

    close(sock);
    return 0;
}