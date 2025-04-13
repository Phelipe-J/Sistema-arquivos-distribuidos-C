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



typedef int (*funcptr)(SOCKET, char*);

typedef struct {
    const char* name;
    funcptr func;
}command;

int listFiles(SOCKET, char*);
int deleteFile(SOCKET, char*);
int uploadFile(SOCKET, char*);
int downloadFile(SOCKET, char*);

int communication(SOCKET monitorSocket){
    int bytesSent = 0;
    int bytesRecv = 0;
    char recvBuf[BUFFER_SIZE];
    char commandBuffer[BUFFER_SIZE];

    command commands[] = {
        {"ls", listFiles},
        {"del", deleteFile},
        {"up", uploadFile},
        {"dw", downloadFile},
        {NULL, NULL}
    };

    while(1){
        if(fgets(commandBuffer, BUFFER_SIZE, stdin)){
            commandBuffer[strcspn(commandBuffer, "\n")] = '\0';

            char* commandInput = strtok(commandBuffer, " ");
            char* argument = strtok(NULL, " ");

            command* chosenCommand = NULL;

            for(int i = 0; commands[i].name != NULL; i++){
                if(strcmp(commandInput, commands[i].name) == 0){
                    chosenCommand = &commands[i];
                    break;
                }
            }

            if(chosenCommand == NULL){
                printf("comando invalido.\n");
            }
            else{
                chosenCommand->func(monitorSocket, argument);
            }
        }
    }
    return 0;
}

int listFiles(SOCKET monitorSocket, char* argument){

    send(monitorSocket, "1", 2, 0);

    char buffer[BUFFER_SIZE];
    int packageAmount = 0;

    //recv(monitorSocket, buffer, BUFFER_SIZE, 0);    //recebe quantidade de pacotes

    //packageAmount = atoi(buffer);

    /*
    for(int i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        recv(monitorSocket, buffer, BUFFER_SIZE, 0);
        printf("%s", buffer);
    }
    */
    printf("\n\n");
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);
    printf(buffer);

    return 0;
}

int deleteFile(SOCKET monitorSocket, char* fileName){
    send(monitorSocket, "2", 2, 0);

    char buffer[BUFFER_SIZE];

    send(monitorSocket, fileName, BUFFER_SIZE, 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);

    printf("%s", buffer);

    return 0;
}

int uploadFile(SOCKET monitorSocket, char* filePath){
    send(monitorSocket, "3", 2, 0);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    char* fileName;
    int packageAmount = 0;

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

    memset(buffer, 0, BUFFER_SIZE);
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

    memset(buffer, 0, BUFFER_SIZE);
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);

    printf("%s", buffer);

    return 0;
}

int downloadFile(SOCKET monitorSocket, char* fileName){
    send(monitorSocket, "4", 2, 0);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int packageAmount = 0;
    FILE* fptr;

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