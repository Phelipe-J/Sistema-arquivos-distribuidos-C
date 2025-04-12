#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "info.h"
//OBS MONITOR_IP foi definido em info.h

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\server\\"      //Caminho da pasta do servidor
#define BUFFER_SIZE    1024

int listFiles();
int deleteFile(SOCKET);
int uploadFile(SOCKET);
int downloadFile(SOCKET);

int communication(SOCKET monitorSocket){
    
    char buffer[BUFFER_SIZE];
    int bytes = 0;
    
    while(1){
        
        bytes = recv(monitorSocket, buffer, BUFFER_SIZE, 0);

        if(bytes > 0){
            int opcode = atoi(buffer);

            switch (opcode)
            {
            case 1:
                /* code */
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
                break;
            }
        }
    }
}

int deleteFile(SOCKET monitorSocket){
    char buffer[BUFFER_SIZE];
    char path[BUFFER_SIZE];

    int result = 1;

    strcpy(path, PATH);

    printf("Remover arquivo.\n");

    recv(monitorSocket, buffer, BUFFER_SIZE, 0);       //recebe nome do arquivo

    strcat(path, buffer);

    result = remove(path);

    memset(buffer, 0, BUFFER_SIZE);

    if(result == 0){
        //sucesso
        strcpy(buffer, "Arquivo Removido.\n");
        send(monitorSocket, buffer, strlen(buffer), 0);
        return 0;
    }
    else{
        strcpy(buffer, "Falha na remocao.\n\n");
        send(monitorSocket, buffer, strlen(buffer), 0);
        return 1;
    }
    return 1;
}

int uploadFile(SOCKET monitorSocket){
    char buffer[BUFFER_SIZE];
    int packageAmount = 0;
    int i = 0;
    char path[BUFFER_SIZE];

    strcpy(path, PATH);

    printf("inserir arquivo.\n");

    
    memset(buffer, 0, BUFFER_SIZE);
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);   // Recebe informações do arquivo

    // Tokeniza e separa as informações
    char* token = strtok(buffer, ";");
    packageAmount = atoi(token);

    token = strtok(NULL, ";");
    char* fileName = malloc(sizeof(token));
    strcpy(fileName, token);

    strcat(path, fileName);

    printf("pkg amount: %d\n", packageAmount);

    FILE* fptr = 0;

    // Verifica se arquivo já existe
    fptr = fopen(path, "rb");
    if(fptr != NULL){
        printf("Arquivo ja existe.\n");
        strcpy(buffer, "Erro: Arquivo ja existe.\n");
        send(monitorSocket, buffer, (int)strlen(buffer), 0);
        fclose(fptr);
        return 1;
    }

    strcpy(buffer, "1");
    send(monitorSocket, buffer, (int)strlen(buffer), 0);   // Envia informação que está tudo certo e pode começar a mandar os pacotes
    memset(buffer, 0, BUFFER_SIZE);

    printf("file name: %s\n", fileName);

    // Cria e abre arquivo
    fptr = fopen(path, "wb");
    free(fileName);
    if(fptr == NULL){
        perror("arquivo nao foi criado");
    }

    // Recebe os pacotes e preenche o arquivo
    for(i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        recv(monitorSocket, buffer, BUFFER_SIZE, 0);
        //fprintf(fptr, buffer);
        fwrite(buffer, 1, BUFFER_SIZE, fptr);
    }

    fclose(fptr);

    printf("arquivo criado.\n");
    memset(buffer, 0, BUFFER_SIZE);
    strcpy(buffer, "Arquivo Criado.\n");
    send(monitorSocket, buffer, strlen(buffer), 0);

    return 0;
}

int downloadFile(SOCKET monitorSocket){
    printf("upload arquivo.\n");

    int packageAmount = 0;
    char buffer[BUFFER_SIZE];
    char path[BUFFER_SIZE];

    memset(path, 0, BUFFER_SIZE);
    strcpy(path, PATH);

    memset(buffer, 0, BUFFER_SIZE);
    recv(monitorSocket, buffer, BUFFER_SIZE, 0);    //recebe nome do arquivo

    strcat(path, buffer);

    memset(buffer, 0, BUFFER_SIZE);

    FILE* fptr = fopen(path, "rb");

    if(fptr == NULL){
        printf("falha ao abrir arquivo.\n");
        strcpy(buffer, "Falha ao abrir arquivo.\n");
        send(monitorSocket, buffer, strlen(buffer), 0);
        return 1;
    }

    strcpy(buffer, "1");
    send(monitorSocket, buffer, strlen(buffer), 0);
    memset(buffer, 0, BUFFER_SIZE);

    fseek(fptr, 0L, SEEK_END);
    packageAmount = ceil((float)ftell(fptr)/BUFFER_SIZE);

    sprintf(buffer, "%d", packageAmount);

    send(monitorSocket, buffer, strlen(buffer), 0);  // Envia quantidade de pacotes
    memset(buffer, 0, BUFFER_SIZE);

    rewind(fptr);

    for(int i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        fread(buffer, 1, BUFFER_SIZE, fptr);
        send(monitorSocket, buffer, BUFFER_SIZE, 0);
    }

    printf("Download concluido.\n");
    fclose(fptr);
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET monitorSocket = INVALID_SOCKET;
    struct addrinfo hints = {0}, *result = NULL;
    
    // Inicializa WinSock
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup falhou\n");
        return 1;
    }

    // Prepara hints para getaddrinfo
    hints.ai_family   = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM;   // TCP

    // Resolve o endereço do servidor
    if (getaddrinfo(MONITOR_IP, SERVERS_PORT, &hints, &result) != 0) {
        fprintf(stderr, "getaddrinfo falhou\n");
        WSACleanup();
        return 1;
    }

    // Cria o socket
    monitorSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (monitorSocket == INVALID_SOCKET) {
        fprintf(stderr, "socket falhou: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Conecta ao servidor
    if (connect(monitorSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "connect falhou: %d\n", WSAGetLastError());
        closesocket(monitorSocket);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);
    printf("Conectado ao monitor %s:%s\n", MONITOR_IP, SERVERS_PORT);


    communication(monitorSocket);

    closesocket(monitorSocket);
    WSACleanup();
    return 0;
}
