#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

void send_command(int sockfd, const char *command) {
    write(sockfd, command, strlen(command));
}

void receive_response(int sockfd, char *buffer, int size) {
    int bytes_read = read(sockfd, buffer, size - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
}

void extract_pasv_ip_port(char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;

    sscanf(response,
           "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
           &h1, &h2, &h3, &h4, &p1, &p2);

    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);

    *port = (p1 << 8) + p2;
}

int ftp_open_pasv_data_connection(int control_sock) {
    char buffer[BUFFER_SIZE];
    char ip[64];
    int port;

    send_command(control_sock, "PASV\r\n");

    receive_response(control_sock, buffer, sizeof(buffer));

    extract_pasv_ip_port(buffer, ip, &port);

    printf("PASV DATA IP: %s\n", ip);
    printf("PASV DATA PORT: %d\n", port);

    int data_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in data_addr;

    memset(&data_addr, 0, sizeof(data_addr));

    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(port);

    inet_pton(AF_INET, ip, &data_addr.sin_addr);

    if (connect(data_sock,
                (struct sockaddr *)&data_addr,
                sizeof(data_addr)) < 0) {

        perror("Data connection failed");
        close(data_sock);
        return -1;
    }

    return data_sock;
}

void ftp_list(int control_sock) {
    char buffer[BUFFER_SIZE];

    int data_sock = ftp_open_pasv_data_connection(control_sock);

    if (data_sock < 0)
        return;

    send_command(control_sock, "LIST\r\n");

    receive_response(control_sock, buffer, sizeof(buffer));

    printf("\n===== DIRECTORY LIST =====\n");

    int n;

    while ((n = read(data_sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }

    printf("\n==========================\n");

    close(data_sock);

    receive_response(control_sock, buffer, sizeof(buffer));
}

void ftp_retr(int control_sock, const char *filename) {
    char cmd[256];
    char buffer[BUFFER_SIZE];

    int data_sock = ftp_open_pasv_data_connection(control_sock);

    if (data_sock < 0)
        return;

    snprintf(cmd, sizeof(cmd), "RETR %s\r\n", filename);

    send_command(control_sock, cmd);

    receive_response(control_sock, buffer, sizeof(buffer));

    FILE *fp = fopen(filename, "wb");

    if (!fp) {
        perror("fopen");
        close(data_sock);
        return;
    }

    int n;

    while ((n = read(data_sock, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, n, fp);
    }

    fclose(fp);

    close(data_sock);

    receive_response(control_sock, buffer, sizeof(buffer));

    printf("Downloaded file: %s\n", filename);
}

void ftp_stor(int control_sock, const char *filename) {
    char cmd[256];
    char buffer[BUFFER_SIZE];

    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        perror("fopen");
        return;
    }

    int data_sock = ftp_open_pasv_data_connection(control_sock);

    if (data_sock < 0) {
        fclose(fp);
        return;
    }

    snprintf(cmd, sizeof(cmd), "STOR %s\r\n", filename);

    send_command(control_sock, cmd);

    receive_response(control_sock, buffer, sizeof(buffer));

    int n;

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        write(data_sock, buffer, n);
    }

    shutdown(data_sock, SHUT_WR);

    fclose(fp);

    close(data_sock);

    receive_response(control_sock, buffer, sizeof(buffer));

    printf("Uploaded file: %s\n", filename);
}

void ftp_delete(int control_sock, const char *filename) {
    char cmd[256];
    char buffer[BUFFER_SIZE];

    snprintf(cmd, sizeof(cmd), "DELE %s\r\n", filename);

    send_command(control_sock, cmd);

    receive_response(control_sock, buffer, sizeof(buffer));
}

void ftp_cwd(int control_sock, const char *dirname) {
    char cmd[256];
    char buffer[BUFFER_SIZE];

    snprintf(cmd, sizeof(cmd), "CWD %s\r\n", dirname);

    send_command(control_sock, cmd);

    receive_response(control_sock, buffer, sizeof(buffer));
}

void ftp_pwd(int control_sock) {
    char buffer[BUFFER_SIZE];

    send_command(control_sock, "PWD\r\n");

    receive_response(control_sock, buffer, sizeof(buffer));
}

int main() {

    int control_sockfd;

    struct sockaddr_in server_addr;

    control_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(21);

    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(control_sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];

    receive_response(control_sockfd, buffer, sizeof(buffer));

    send_command(control_sockfd, "USER user\r\n");
    receive_response(control_sockfd, buffer, sizeof(buffer));

    send_command(control_sockfd, "PASS pass\r\n");
    receive_response(control_sockfd, buffer, sizeof(buffer));

    char input[512];

    while (1) {

        printf("\nNhap lenh> ");

        fgets(input, sizeof(input), stdin);

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "LIST") == 0) {

            ftp_list(control_sockfd);

        } else if (strncmp(input, "RETR ", 5) == 0) {

            ftp_retr(control_sockfd, input + 5);

        } else if (strncmp(input, "STOR ", 5) == 0) {

            ftp_stor(control_sockfd, input + 5);

        } else if (strncmp(input, "DELE ", 5) == 0) {

            ftp_delete(control_sockfd, input + 5);

        } else if (strncmp(input, "CWD ", 4) == 0) {

            ftp_cwd(control_sockfd, input + 4);

        } else if (strcmp(input, "PWD") == 0) {

            ftp_pwd(control_sockfd);

        } else if (strcmp(input, "QUIT") == 0) {

            send_command(control_sockfd, "QUIT\r\n");

            receive_response(control_sockfd,
                             buffer,
                             sizeof(buffer));

            break;

        } else {

            printf("Unknown command\n");

            printf("Supported:\n");
            printf("LIST\n");
            printf("RETR <file>\n");
            printf("STOR <file>\n");
            printf("DELE <file>\n");
            printf("CWD <dir>\n");
            printf("PWD\n");
            printf("QUIT\n");
        }
    }

    close(control_sockfd);

    return 0;
}