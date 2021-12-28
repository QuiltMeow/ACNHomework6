#pragma once

#define INVALID_SOCKET -1

#define KEEP_ALIVE_TIME 60
#define KEEP_ALIVE_INTERVAL 5

bool initSocket(int socketFD, bool reuse) {
    BOOL enable = TRUE;
    int enableSize = sizeof (enable);

    if (reuse) {
        if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &enable, enableSize) != 0) {
            perror("Socket 無法設定重複使用狀態 ");
            return false;
        }
    }

    if (setsockopt(socketFD, SOL_SOCKET, SO_KEEPALIVE, &enable, enableSize) != 0) {
        perror("Socket 無法設定 Keep Alive 狀態 ");
        return false;
    }

    if (setsockopt(socketFD, IPPROTO_TCP, TCP_NODELAY, &enable, enableSize) != 0) {
        perror("Socket 無法設定 No Delay 狀態 ");
        return false;
    }

    int keepAliveTime = KEEP_ALIVE_TIME;
    if (setsockopt(socketFD, IPPROTO_TCP, TCP_KEEPIDLE, &keepAliveTime, sizeof (keepAliveTime)) != 0) {
        perror("Socket 無法設定 Keep Alive 時間 ");
        return false;
    }

    int keepAliveInterval = KEEP_ALIVE_INTERVAL;
    if (setsockopt(socketFD, IPPROTO_TCP, TCP_KEEPINTVL, &keepAliveInterval, sizeof (keepAliveInterval)) != 0) {
        perror("Socket 無法設定 Keep Alive 間隔 ");
        return false;
    }
    return true;
}

void closeSocket(int socketFD) {
    shutdown(socketFD, SHUT_RDWR);
    close(socketFD);
}
