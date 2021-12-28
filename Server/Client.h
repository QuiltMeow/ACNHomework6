#pragma once

struct Client {
    char priorityHost[PACKET_BUFFER_SIZE];
    int socketFD;
    int prioritySocketFD;
};

void cleanClient(struct Client* client) {
    client->socketFD = INVALID_SOCKET;
    bzero(&(client->priorityHost), PACKET_BUFFER_SIZE);
    client->prioritySocketFD = INVALID_SOCKET;
}
