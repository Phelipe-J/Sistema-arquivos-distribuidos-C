#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "info.h"
//OBS MONITOR_IP foi definido em info.h

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"


#define BUFFER_SIZE    1024
#define OPCODE_SIZE 5

int listFiles(SOCKET);
int deleteFile(SOCKET);
int uploadFile(SOCKET);
int downloadFile(SOCKET);

int communication(SOCKET monitorSocket){
    int bytesSent = 0;
    int bytesRecv = 0;
    char msg[BUFFER_SIZE];
    char recvBuf[BUFFER_SIZE];
    int opcode = 0;
    
    while(1){
        if(fgets(msg, OPCODE_SIZE, stdin)){
            opcode = atoi(msg);
            bytesSent = send(monitorSocket, msg, (int)strlen(msg), 0);

            if (bytesSent == SOCKET_ERROR) {
                fprintf(stderr, "send falhou: %d\n", WSAGetLastError());
                closesocket(monitorSocket);
                WSACleanup();
                return 1;
            }
            else{
                switch (opcode){
                case 1:
                    listFiles(monitorSocket);
                    break;

                case 2:
                    deleteFile(monitorSocket);
                    break;

                case 3:
                    uploadFile(monitorSocket);
                    break;

                case 4:
                    downloadFile(monitorSocket);
                    break;
                    
                default:
                    printf("Codigo invalido.\n");
                    break;
                }
            }
            memset(msg, 0, BUFFER_SIZE);
        }
    }
    return 0;
}

int listFiles(SOCKET monitorSocket){
    char buffer[BUFFER_SIZE];
    int packageAmount = 0;

    recv(monitorSocket, buffer, BUFFER_SIZE, 0);    //recebe quantidade de pacotes

    packageAmount = atoi(buffer);

    for(int i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        recv(monitorSocket, buffer, BUFFER_SIZE, 0);
        printf("%s", buffer);
    }

    return 0;
}

int deleteFile(SOCKET monitorSocket){
    char buffer[BUFFER_SIZE];
    char fileName[BUFFER_SIZE];

    fgets(fileName, BUFFER_SIZE, stdin);
    fileName[strcspn(fileName, "\n")] = 0;

    send(monitorSocket, fileName, BUFFER_SIZE, 0);

    recv(monitorSocket, buffer, BUFFER_SIZE, 0);

    printf("%s", buffer);

    return 0;
}

int uploadFile(SOCKET monitorSocket){
    char buffer[BUFFER_SIZE];
    char filePath[BUFFER_SIZE];
    char* fileName;
    int packageAmount = 0;
    
    printf("Insira o caminho do arquivo.\n");
    fgets(filePath, BUFFER_SIZE, stdin);
    filePath[strcspn(filePath, "\n")] = 0; 
    fileName = strrchr(filePath, '\\');
    fileName++;

    printf("file name: %s\n", fileName);

    FILE* fptr = fopen(filePath, "rb");

    fseek(fptr, 0L, SEEK_END);
    packageAmount = ceil((float)ftell(fptr)/BUFFER_SIZE);

    printf("pkg amount: %d\n", packageAmount);

    sprintf(buffer, "%d;%s", packageAmount, fileName);

    printf("buffer: %s\n", buffer);

    send(monitorSocket, buffer, strlen(buffer), 0);  // Envia informações do arquivo
    memset(buffer, 0, BUFFER_SIZE);

    rewind(fptr);

    recv(monitorSocket, buffer, BUFFER_SIZE, 0); // Espera resposta para começar a enviar os pacotes

    if(strcmp(buffer, "1") == 0){

        memset(buffer, 0, BUFFER_SIZE);

        for(int i = 0; i < packageAmount; i++){
            fread(buffer, 1, BUFFER_SIZE, fptr);
            send(monitorSocket, buffer, BUFFER_SIZE, 0);
            memset(buffer, 0, BUFFER_SIZE);
        }
    }
    else{
        printf("%s", buffer);
        return 1;
    }

    fclose(fptr);

    recv(monitorSocket, buffer, BUFFER_SIZE, 0);

    printf("%s", buffer);

    return 0;
}

int downloadFile(SOCKET monitorSocket){
    char buffer[BUFFER_SIZE];
    char fileName[BUFFER_SIZE];
    int packageAmount = 0;
    FILE* fptr;

    fgets(fileName, BUFFER_SIZE, stdin);
    fileName[strcspn(fileName, "\n")] = 0;

    fptr = fopen(fileName, "wb");

    send(monitorSocket, fileName, BUFFER_SIZE, 0);      // Envia nome do arquivo

    memset(buffer, 0, BUFFER_SIZE);
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);    //recebe se o servidor abriu o arquivo

    if(strcmp(buffer, "1") != 0){
        printf("%s", buffer);
        fclose(fptr);
        return 1;
    }

    memset(buffer, 0, BUFFER_SIZE);
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);    //recebe quantidade de pacotes
    packageAmount = atoi(buffer);

    for(int i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        recv(monitorSocket, buffer, BUFFER_SIZE, 0);
        fwrite(buffer, 1, BUFFER_SIZE, fptr);
    }

    fclose(fptr);

    printf("Download concluido.\n");

    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET monitorSocket = INVALID_SOCKET;
    struct addrinfo hints = {0}, *result = NULL;
    
    // 1. Inicializa WinSock
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup falhou\n");
        return 1;
    }

    // 2. Prepara hints para getaddrinfo
    hints.ai_family   = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM;   // TCP

    // 3. Resolve o endereço do servidor
    if (getaddrinfo(MONITOR_IP, CLIENTS_PORT, &hints, &result) != 0) {
        fprintf(stderr, "getaddrinfo falhou\n");
        WSACleanup();
        return 1;
    }

    // 4. Cria o socket
    monitorSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (monitorSocket == INVALID_SOCKET) {
        fprintf(stderr, "socket falhou: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // 5. Conecta ao servidor
    if (connect(monitorSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "connect falhou: %d\n", WSAGetLastError());
        closesocket(monitorSocket);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);
    printf("Conectado ao servidor %s:%s\n", MONITOR_IP, CLIENTS_PORT);
    
    communication(monitorSocket);

    closesocket(monitorSocket);
    WSACleanup();
    return 0;
}
