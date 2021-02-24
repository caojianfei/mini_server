#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <threads.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#define PORT 3000
#define HOST INADDR_ANY

int handle(void *);

int main() {
    int server_fd, bind_res, listen_res, epfd;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket invoke error: ");
        return 1;
    }

    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htons(HOST);
    server_addr.sin_family = AF_INET;

    if ((bind_res = bind(server_fd, (const struct sockaddr *) &server_addr, sizeof(struct sockaddr))) == -1) {
        perror("bind invoke error: ");
        printf("bind res %d\n", bind_res);
        return 2;
    }

    if ((listen_res = listen(server_fd, 20))) {
        perror("listen invoke error: ");
        printf("listen res %d\n", listen_res);
        return 3;
    }

    printf("listen in port: %d\n", PORT);

    struct epoll_event ep_event, ep_events[20];
    int ep_evt_num;
    ep_event.events = EPOLLIN;
    ep_event.data.fd = server_fd;
    epfd = epoll_create(256);
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ep_event);


    struct sockaddr_in client_addr;
    socklen_t sin_size = sizeof(struct sockaddr_in);
    int conn_fd;
    thrd_t conn_thread;
    // memset(&client_addr, 0, sizeof(struct sockaddr_in));
    while (1) {
        ep_evt_num = epoll_wait(epfd, (struct epoll_event *) &ep_events, 20, 100);
        if (ep_evt_num > 0) {
            // printf("ep_evt_num: %d\n", ep_evt_num);
            conn_fd = accept(server_fd, (struct sockaddr *) &client_addr, &sin_size);
            if (conn_fd == -1) {
                if (errno == EAGAIN) {
                    sleep(1);
                    continue;
                }
                printf("error no: %d\n", EAGAIN);
                perror("accept error");
                exit(0);
            }

            printf("accept new connection: %s, conn_fd: %d\n", inet_ntoa(client_addr.sin_addr), conn_fd);

            int conn_fd_thrd = conn_fd;

            if (thrd_create(&conn_thread, handle, &conn_fd_thrd) != thrd_success) {
                perror("thrd_create error: ");
                continue;
            }

            if (thrd_detach(conn_thread) != thrd_success) {
                perror("thrd_detach error: ");
                continue;
            }
        }
    }
}

int handle(void *argv) {
    int conn_fd = *((int *) argv);
    printf("accept new conn, fd: %d\n", conn_fd);

    int flags = fcntl(conn_fd, F_GETFL);
    printf("flags: %d\n", flags);

    char *message = "welcome";
    size_t messlen = strlen(message);
    size_t send_size = send(conn_fd, message, messlen, 0);
    if (send_size <= 0) {
        printf("send message error");
        thrd_exit(0);
    }

    const int buff_size = 5;
    char recv_buff[buff_size];
    while (1) {
        memset(&recv_buff, 0, sizeof(recv_buff));
        ssize_t recv_size = recv(conn_fd, recv_buff, buff_size, 0);
        if (recv_size <= 0) {
            printf("recv message error. errno: %d, recv_size: %zd \n", errno, recv_size);
            close(conn_fd);
            thrd_exit(0);
        }

        printf("recv message: %s\n", recv_buff);
    }
}
