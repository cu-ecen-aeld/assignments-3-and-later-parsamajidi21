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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define DATA_FILE "/var/tmp/aesdsocketdata"
#define CHUNK_SIZE 1024

int server_sockfd = -1;
volatile sig_atomic_t keep_running = 1;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_handler {
    pthread_t thread_id;
    SLIST_ENTRY(thread_handler) next_thread;
} thread_handler;

typedef struct client_handler {
    char* ip_addr;
    int client_fdesc;
} client_handler;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        keep_running = 0;
        if (server_sockfd != -1) {
            shutdown(server_sockfd, SHUT_RDWR);
        }
    }
}

int send_back_data(int acceptfd) {
    FILE *file = fopen(DATA_FILE, "r");
    if (!file) return -1;

    char buf[CHUNK_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), file)) > 0) {
        size_t total_sent = 0;
        while (total_sent < bytes_read) {
            ssize_t sent = send(acceptfd, buf + total_sent, bytes_read - total_sent, 0);
            if (sent == -1) {
                perror("Error in send back");
                fclose(file);
                return -1;
            }
            total_sent += sent;
        }
    }
    fclose(file);
    return 0;
}

void handle_client(int accfd, const char* ip_str) {
    char buf[CHUNK_SIZE];
    ssize_t bytes_received;

    size_t recv_buf_size = CHUNK_SIZE * 4;
    size_t recv_buf_len  = 0;
    char  *recv_buf      = malloc(recv_buf_size);
    if (!recv_buf) {
        syslog(LOG_ERR, "Failed to allocate receive buffer");
        return;
    }

    while (keep_running) {
        bytes_received = recv(accfd, buf, sizeof(buf), 0);

        if (bytes_received == 0) {
            syslog(LOG_INFO, "Client %s closed connection", ip_str);
            break;
        } else if (bytes_received < 0) {
            if (keep_running) perror("recv failed");
            break;
        }

        if (recv_buf_len + (size_t)bytes_received > recv_buf_size) {
            recv_buf_size = (recv_buf_len + (size_t)bytes_received) * 2;
            char *tmp = realloc(recv_buf, recv_buf_size);
            if (!tmp) {
                syslog(LOG_ERR, "Failed to reallocate receive buffer");
                break;
            }
            recv_buf = tmp;
        }

        memcpy(recv_buf + recv_buf_len, buf, bytes_received);
        recv_buf_len += bytes_received;

        char *newline;
        size_t processed = 0;
        while ((newline = memchr(recv_buf + processed,'\n',recv_buf_len - processed)) != NULL) {
            size_t line_len = (newline - (recv_buf + processed)) + 1; 

            pthread_mutex_lock(&file_mutex);
            FILE *file = fopen(DATA_FILE, "a");
            if (file) {
                fwrite(recv_buf + processed, 1, line_len, file);
                fclose(file);
            }
            if (send_back_data(accfd) == -1) {
                syslog(LOG_ERR, "send_back_data failed for client %s", ip_str);
            }
            pthread_mutex_unlock(&file_mutex);

            processed += line_len;
        }

        if (processed > 0 && processed < recv_buf_len) {
            memmove(recv_buf, recv_buf + processed, recv_buf_len - processed);
        }
        recv_buf_len -= processed;

        if (recv_buf_len > 0) {
            pthread_mutex_lock(&file_mutex);
            FILE *file = fopen(DATA_FILE, "a");
            if (file) {
                fwrite(recv_buf, 1, recv_buf_len, file);
                fclose(file);
            }
            if (send_back_data(accfd) == -1) {
                syslog(LOG_ERR, "send_back_data failed for client %s", ip_str);
            }
            pthread_mutex_unlock(&file_mutex);
            recv_buf_len = 0;
        }
    }

    free(recv_buf);
}

void* handle_client_thread(void* accfd) {
    client_handler* acceptfd = (client_handler*) accfd;
    int accept_fd  = acceptfd->client_fdesc;
    char* ip_str   = acceptfd->ip_addr;

    handle_client(accept_fd, ip_str);

    shutdown(accept_fd, SHUT_RDWR);
    close(accept_fd);
    free(acceptfd->ip_addr);
    free(acceptfd);
    return NULL;
}

void* timestamp_handler(void* arg) {
    (void)arg;
    while (keep_running) {
        for (int i = 0; i < 10 && keep_running; i++) {
            sleep(1);
        }
        if (!keep_running) break;

        time_t rawtime;
        struct tm *timeinfo;
        char time_stamp[100];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(time_stamp, sizeof(time_stamp), "timestamp: %a, %d %b %Y %T %z\n", timeinfo);

        pthread_mutex_lock(&file_mutex);
        FILE *file = fopen(DATA_FILE, "a");
        if (file) {
            fputs(time_stamp, file);
            fclose(file);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
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
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

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
    freeaddrinfo(res);
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

    pthread_t time_thread;
    pthread_create(&time_thread, NULL, timestamp_handler, NULL);

    SLIST_HEAD(thread_list, thread_handler);
    struct thread_list head;
    SLIST_INIT(&head);

    while (keep_running) {
        addr_size = sizeof(client_addr);
        client_handler *args = calloc(1, sizeof(client_handler));
        if (!args) {
            syslog(LOG_ERR, "calloc failed for client_handler");
            continue;
        }

        args->client_fdesc = accept(server_sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (args->client_fdesc == -1) {
            free(args);
            if (!keep_running) break;
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
        syslog(LOG_INFO, "Accepted connection from %s", ip_str);

        args->ip_addr = strdup(ip_str);
        if (!args->ip_addr) {
            close(args->client_fdesc);
            free(args);
            continue;
        }

        thread_handler *my_threads = calloc(1, sizeof(thread_handler));
        if (!my_threads) {
            close(args->client_fdesc);
            free(args->ip_addr);
            free(args);
            continue;
        }

        if (pthread_create(&my_threads->thread_id, NULL,
                           handle_client_thread, args) == 0) {
            SLIST_INSERT_HEAD(&head, my_threads, next_thread);
        } else {
            free(my_threads);
            close(args->client_fdesc);
            free(args->ip_addr);
            free(args);
        }
    }

    pthread_join(time_thread, NULL);

    thread_handler *item;
    while (!SLIST_EMPTY(&head)) {
        item = SLIST_FIRST(&head);
        pthread_join(item->thread_id, NULL);
        SLIST_REMOVE_HEAD(&head, next_thread);
        free(item);
    }

    close(server_sockfd);
    unlink(DATA_FILE);
    closelog();

    return 0;
}