#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "Bool.h"
#include "Socket.h"

#define PACKET_BUFFER_SIZE 64
#define CONNECT_HOST "10.0.0.1" // 連線目標不同時需修改
#define CONNECT_PORT 8923
#define RANDOM_PORT 0
#define MAX_PENDING_CONNECTION 200

int main() {
    srand(time(NULL));
    char hostBuffer[PACKET_BUFFER_SIZE];
    if (gethostname(hostBuffer, sizeof (hostBuffer)) != 0) {
        perror("查詢主機名稱時發生錯誤 ");
        return EXIT_FAILURE;
    }
    printf("本機名稱 : %s\n", hostBuffer);

    int normalSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int prioritySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (normalSocket == INVALID_SOCKET || prioritySocket == INVALID_SOCKET) {
        perror("無效的 Socket ");
        return EXIT_FAILURE;
    }

    if (!initSocket(normalSocket) || !initSocket(prioritySocket)) {
        return EXIT_FAILURE;
    }

    struct sockaddr_in prioritySocketAddress;
    bzero(&prioritySocketAddress, sizeof (prioritySocketAddress));
    prioritySocketAddress.sin_addr.s_addr = INADDR_ANY;
    prioritySocketAddress.sin_family = AF_INET;
    prioritySocketAddress.sin_port = htons(RANDOM_PORT);
    if (bind(prioritySocket, (struct sockaddr*) & prioritySocketAddress, sizeof (prioritySocketAddress)) != 0) {
        perror("服務端端口綁定失敗 ");
        return EXIT_FAILURE;
    }

    if (listen(prioritySocket, MAX_PENDING_CONNECTION) != 0) {
        perror("服務端端口監聽失敗 ");
        return EXIT_FAILURE;
    }

    struct sockaddr_in listenSocketAddress;
    socklen_t listenSocketAddressLength = sizeof (listenSocketAddress);
    if (getsockname(prioritySocket, (struct sockaddr *) &listenSocketAddress, &listenSocketAddressLength) != 0) {
        perror("取得優先 Socket 資訊失敗 ");
        return EXIT_FAILURE;
    }

    int listenPort = ntohs(listenSocketAddress.sin_port);
    char listenSocketAddressString[PACKET_BUFFER_SIZE];
    bzero(&listenSocketAddressString, PACKET_BUFFER_SIZE);
    sprintf(listenSocketAddressString, "%s:%d", hostBuffer, listenPort);
    printf("優先 Socket 位址 : %s\n", listenSocketAddressString);

    struct sockaddr_in targetSocketAddress;
    bzero(&targetSocketAddress, sizeof (targetSocketAddress));
    inet_pton(AF_INET, CONNECT_HOST, &(targetSocketAddress.sin_addr));
    targetSocketAddress.sin_family = AF_INET;
    targetSocketAddress.sin_port = htons(CONNECT_PORT);

    printf("使用普通 Socket 與伺服器建立連線 ...\n");
    if (connect(normalSocket, (struct sockaddr *) &targetSocketAddress, sizeof (targetSocketAddress)) != 0) {
        perror("無法與伺服器建立連線 ... ");
        return EXIT_FAILURE;
    }

    send(normalSocket, (const char*) listenSocketAddressString, PACKET_BUFFER_SIZE, 0);
    printf("已送出優先 Socket 資訊\n");

    struct sockaddr_in priorityAcceptSocketAddress;
    socklen_t priorityAcceptSocketAddressLength = sizeof (priorityAcceptSocketAddress);
    printf("等待伺服器與優先 Socket 建立連線 ...\n");
    int priorityAcceptSocket = accept(prioritySocket, (struct sockaddr*) &priorityAcceptSocketAddress, &priorityAcceptSocketAddressLength);
    if (priorityAcceptSocket == INVALID_SOCKET) {
        perror("優先 Socket 接受連線失敗 ");
        return EXIT_FAILURE;
    }

    char packet[PACKET_BUFFER_SIZE];
    int cookie = rand();
    printf("使用普通 Socket 傳輸 Cookie 資訊 : %d\n", cookie);
    bzero(&packet, PACKET_BUFFER_SIZE);
    sprintf(packet, "%d", cookie);
    send(normalSocket, (const char*) packet, PACKET_BUFFER_SIZE, 0);

    bzero(&packet, PACKET_BUFFER_SIZE);
    if (recv(priorityAcceptSocket, packet, PACKET_BUFFER_SIZE, 0) <= 0) {
        perror("優先 Socket 中斷連線 ");
        return EXIT_FAILURE;
    }
    int receiveCookie = atoi(packet);
    printf("收到優先 Socket Cookie 資訊 : %d\n", receiveCookie);

    bool validate = false;
    if (receiveCookie == cookie) {
        validate = true;
        printf("Cookie 資訊正確\n");
    } else {
        printf("錯誤的 Cookie 資訊\n");
        closeSocket(priorityAcceptSocket);
    }

    printf("結束連線\n");
    if (validate) {
        closeSocket(priorityAcceptSocket);
    }

    closeSocket(normalSocket);
    closeSocket(prioritySocket);
    return EXIT_SUCCESS;
}
