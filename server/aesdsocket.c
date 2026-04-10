#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define DATA_FILE "/var/tmp/aesdsocketdata"
#define CHUNK_SIZE 1024

int server_sockfd = -1;
volatile sig_atomic_t keep_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        keep_running = 0;
        if (server_sockfd != -1) {
            shutdown(server_sockfd, SHUT_RDWR);
        }
    }
}

void send_back_data(int acceptfd) {
    FILE *file = fopen(DATA_FILE, "r");
    if (!file) return;

    char buf[CHUNK_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), file)) > 0) {
        send(acceptfd, buf, bytes_read, 0);
    }
    fclose(file);
}

void handle_client(int accfd, const char* ip_str) {
    char buf[CHUNK_SIZE];
    ssize_t bytes_received;
    
    FILE *file = fopen(DATA_FILE, "a+");
    if (!file) {
        perror("file open");
        return;
    }

    while ((bytes_received = recv(accfd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, bytes_received, file);
        
        if (memchr(buf, '\n', bytes_received)) {
            fflush(file); 
            send_back_data(accfd);
        }
    }

    fclose(file);
    syslog(LOG_INFO, "Closed connection from %s", ip_str);
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, "9000", &hints, &res);
    server_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_sockfd == -1) {
		perror("Open Socket failed");
		freeaddrinfo(res);
		exit(EXIT_FAILURE);
	}
    int yes = 1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(server_sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("Bind failed");
        freeaddrinfo(res);
        close(server_sockfd);
        return -1;
    }
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Fork failed");
            return -1;
        } else if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        if (setsid() == -1) exit(EXIT_FAILURE);
        if (chdir("/") == -1) exit(EXIT_FAILURE);
        
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    if (listen(server_sockfd, 10) == -1) {
        perror("Listen failed");
        return -1;
    }
    freeaddrinfo(res);

    while (keep_running) {
        addr_size = sizeof(client_addr);
        int client_fd = accept(server_sockfd, (struct sockaddr *)&client_addr, &addr_size);
        
        if (client_fd == -1) {
            if (!keep_running) break;
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
        syslog(LOG_INFO, "Accepted connection from %s", ip_str);

        handle_client(client_fd, ip_str);
        close(client_fd);
    }

    close(server_sockfd);
    unlink(DATA_FILE);
    closelog();
    
    return 0;
}