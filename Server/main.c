#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 8923
#define MAX_PENDING_CONNECTION 200
#define PACKET_BUFFER_SIZE 64

#include "Bool.h"
#include "Socket.h"
#include "Client.h"

int main() {
    struct Client client[FD_SETSIZE];
    bzero(&client, sizeof (client));
    fd_set allSet;

    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        perror("無效的 Socket ");
        return EXIT_FAILURE;
    }
    if (!initSocket(serverSocket, true)) {
        return EXIT_FAILURE;
    }

    struct sockaddr_in serverSocketAddress;
    bzero(&serverSocketAddress, sizeof (serverSocketAddress));
    serverSocketAddress.sin_addr.s_addr = INADDR_ANY;
    serverSocketAddress.sin_family = AF_INET;
    serverSocketAddress.sin_port = htons(PORT);
    if (bind(serverSocket, (struct sockaddr*) & serverSocketAddress, sizeof (serverSocketAddress)) != 0) {
        perror("服務端端口綁定失敗 ");
        return EXIT_FAILURE;
    }

    if (listen(serverSocket, MAX_PENDING_CONNECTION) != 0) {
        perror("服務端端口監聽失敗 ");
        return EXIT_FAILURE;
    }

    int maxFD = serverSocket, maxIndex = INVALID_SOCKET;
    for (int i = 0; i < FD_SETSIZE; ++i) {
        client[i].socketFD = INVALID_SOCKET;
    }
    FD_ZERO(&allSet);

    char packet[PACKET_BUFFER_SIZE];
    FD_SET(serverSocket, &allSet);
    printf("服務端啟動 準備接受連線 ...\n");
    while (true) {
        fd_set receiveSet = allSet;
        int readyCount = select(maxFD + 1, &receiveSet, NULL, NULL, NULL);
        if (FD_ISSET(serverSocket, &receiveSet)) {
            struct sockaddr_in clientSocketAddress;
            socklen_t clientSocketAddressLength = sizeof (clientSocketAddress);

            int clientSocket = accept(serverSocket, (struct sockaddr *) &clientSocketAddress, &clientSocketAddressLength);
            if (clientSocket == INVALID_SOCKET) {
                continue;
            }
            printf("客戶端 %s:%d 已連線到您的伺服器\n", inet_ntoa(clientSocketAddress.sin_addr), clientSocketAddress.sin_port);

            int index;
            for (index = 0; index < FD_SETSIZE; ++index) {
                if (client[index].socketFD == INVALID_SOCKET) {
                    client[index].socketFD = clientSocket;
                    break;
                }
            }
            if (FD_SETSIZE == index) {
                perror("連線拒絕 : 太多客戶端連線 ");
                continue;
            }

            FD_SET(clientSocket, &allSet);
            if (clientSocket > maxFD) {
                maxFD = clientSocket;
            }
            if (index > maxIndex) {
                maxIndex = index;
            }

            if (--readyCount < 0) {
                continue;
            }
        }

        for (int i = 0; i <= maxIndex; ++i) {
            int clientSocket = client[i].socketFD;
            if (clientSocket == INVALID_SOCKET) {
                continue;
            }

            if (FD_ISSET(clientSocket, &receiveSet)) {
                bzero(&packet, PACKET_BUFFER_SIZE);
                if (recv(clientSocket, packet, PACKET_BUFFER_SIZE, 0) <= 0) {
                    closeSocket(clientSocket);
                    FD_CLR(clientSocket, &allSet);
                    cleanClient(&client[i]);
                    printf("客戶端 Socket %d 已中斷連線\n", clientSocket);
                    continue;
                }

                if (strlen(client[i].priorityHost) <= 0) {
                    strcpy(client[i].priorityHost, packet);
                    printf("客戶端 Socket %d 已接收優先 Socket 資訊 %s\n", clientSocket, packet);

                    char priorityHost[PACKET_BUFFER_SIZE];
                    strcpy(priorityHost, client[i].priorityHost);
                    size_t size = sizeof (priorityHost);
                    bool valid = false;
                    for (int i = 0; i < size; ++i) {
                        if (priorityHost[i] == ':') {
                            valid = true;
                            break;
                        }
                    }
                    if (!valid) {
                        printf("客戶端 Socket %d 無效的位址 %s\n", clientSocket, priorityHost);
                        closeSocket(clientSocket);
                        FD_CLR(clientSocket, &allSet);
                        cleanClient(&client[i]);
                        continue;
                    }

                    char *ipString, *portString;
                    ipString = strtok(priorityHost, ":");
                    portString = strtok(NULL, ":");
                    int port = atoi(portString);
                    if (port <= 0 || port > 65535) {
                        printf("客戶端 Socket %d 無效的 Port %d\n", clientSocket, port);
                        closeSocket(clientSocket);
                        FD_CLR(clientSocket, &allSet);
                        cleanClient(&client[i]);
                        continue;
                    }

                    int prioritySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (!initSocket(prioritySocket, false)) {
                        closeSocket(prioritySocket);
                        closeSocket(clientSocket);
                        FD_CLR(clientSocket, &allSet);
                        cleanClient(&client[i]);
                        continue;
                    }
                    client[i].prioritySocketFD = prioritySocket;

                    struct sockaddr_in targetSocketAddress;
                    bzero(&targetSocketAddress, sizeof (targetSocketAddress));
                    inet_pton(AF_INET, ipString, &(targetSocketAddress.sin_addr));
                    targetSocketAddress.sin_family = AF_INET;
                    targetSocketAddress.sin_port = htons(port);

                    printf("連線到優先 Socket %d : %s\n", prioritySocket, client[i].priorityHost);
                    if (connect(prioritySocket, (struct sockaddr *) &targetSocketAddress, sizeof (targetSocketAddress)) != 0) {
                        perror("無法連線到優先 Socket ... ");
                        closeSocket(prioritySocket);
                        closeSocket(clientSocket);
                        FD_CLR(clientSocket, &allSet);
                        cleanClient(&client[i]);
                        continue;
                    }
                    printf("優先 Socket %d 連線成功\n", prioritySocket);
                } else if (strlen(packet) > 0) {
                    int cookie = atoi(packet);
                    printf("客戶端 Socket %d 接收 Cookie %d\n", clientSocket, cookie);

                    bzero(&packet, PACKET_BUFFER_SIZE);
                    sprintf(packet, "%d", cookie);
                    int prioritySocket = client[i].prioritySocketFD;

                    printf("優先 Socket %d 發送 Cookie %d\n", prioritySocket, cookie);
                    send(prioritySocket, (const char*) packet, PACKET_BUFFER_SIZE, 0);

                    printf("優先 Socket %d 結束連線\n", prioritySocket);
                    closeSocket(prioritySocket);

                    printf("客戶端 Socket %d 結束連線\n", clientSocket);
                    closeSocket(clientSocket);

                    FD_CLR(clientSocket, &allSet);
                    cleanClient(&client[i]);
                }

                if (--readyCount <= 0) {
                    break;
                }
            }
        }
    }

    closeSocket(serverSocket);
    return EXIT_SUCCESS;
}
