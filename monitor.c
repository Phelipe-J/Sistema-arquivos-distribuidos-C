#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\monitor\\"     //caminho da pasta do monitor

#define BACKLOG 10
#define BUFFER_SIZE 1024
#define MAX_SERVER_AMOUNT 1

SOCKET serversSockets[MAX_SERVER_AMOUNT];
int serverAmount = 0;

typedef struct File{
    char* name;
    struct File* next;
}file;

typedef struct Folder{
    char* name;
    file* first;
    file* last;
    int fileAmount;
}folder;

int listFiles(SOCKET, char*);
int deleteFile(SOCKET, folder*);
int uploadFile(SOCKET, folder*);
int downloadFile(SOCKET);

int saveMem(folder*, char*);
int addFile(folder*, char*);
int removeFile(folder*, char*);
int showFiles(folder*);

unsigned __stdcall client_handler(void *arg) {      // Thread cliente
    SOCKET clientSocket = (SOCKET)(size_t)arg;
    char recBuffer[BUFFER_SIZE];
    int bytes;
    char** files;
    //char* sendBuffer = "Mensagem Recebida.\n";
    char buffer[BUFFER_SIZE];
    int opcode = 0;

    char path[BUFFER_SIZE];
    memset(path, 0, BUFFER_SIZE);
    strcpy(path, PATH);
    strcat(path, "clientInfo.bin");

    folder* root = malloc(sizeof(folder));

    root->name = "root";
    root->fileAmount = 0;
    root->first = NULL;
    root->last = NULL;

    FILE* clientInfo = fopen(path, "rb+");

    if(clientInfo == NULL){
        FILE* clientInfo = fopen(path, "wb+");
        fclose(clientInfo);
    }
    else{

        memset(buffer, 0, BUFFER_SIZE);

        while(fgets(buffer, BUFFER_SIZE, clientInfo) != NULL){
            
            buffer[strcspn(buffer, "\n")] = 0;
            addFile(root, buffer);
            memset(buffer, 0, BUFFER_SIZE);
        }
    }

    fclose(clientInfo);

    while(1){
        showFiles(root);

        bytes = recv(clientSocket, recBuffer, BUFFER_SIZE, 0);
        if(bytes > 0){
            //printf("Mensagem recebida (%llu): %s", (unsigned long long)clientSocket, recBuffer);
            opcode = atoi(recBuffer);
            
            fflush(stdout);

            switch (opcode)
            {
            case 1:
                listFiles(clientSocket, path);
                break;

            case 2:
                deleteFile(clientSocket, root);
                saveMem(root, path);
                break;

            case 3:
                uploadFile(clientSocket, root);
                saveMem(root, path);
                break;

            case 4:
                downloadFile(clientSocket);
                break;
                    
            default:
                printf("Codigo invalido.\n");
                break;
            }
        }
        else{
            perror("recv");
            break;
        }
        memset(recBuffer, 0, BUFFER_SIZE);
    }

    closesocket(clientSocket);
    return 0;
}

unsigned __stdcall clientAccept(void *arg){

    int result;

    SOCKET clientListenSocket = (SOCKET)(size_t)arg;

    while(1){
        SOCKET clientSocket = accept(clientListenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            fprintf(stderr, "client accept failed: %d\n", WSAGetLastError());
            continue;
        }

        printf("New client connection (sock=%llu)\n", (unsigned long long)clientSocket);

        // Cria thread para o cliente
        uintptr_t threadHandle = _beginthreadex(
            NULL,       // security
            0,          // stack size (default)
            client_handler,
            (void*)(size_t)clientSocket,
            0,          // flags
            NULL        // thread id
        );
        if (threadHandle == 0) {
            fprintf(stderr, "_beginthreadex failed\n");
            closesocket(clientSocket);
        } else {
            CloseHandle((HANDLE)threadHandle);
        }
    }
}

unsigned __stdcall server_handler(void* arg){

}

int serverAccept(SOCKET serverListenSocket){
    while (serverAmount < MAX_SERVER_AMOUNT) {
        SOCKET serverSock = accept(serverListenSocket, NULL, NULL);
        if (serverSock == INVALID_SOCKET) {
            fprintf(stderr, "Server accept failed: %d\n", WSAGetLastError());
            continue;
        }

        serversSockets[serverAmount] = serverSock;
        serverAmount++;

        printf("New server connection (sock=%llu)\n", (unsigned long long)serverSock);

    }
    return 0;
}

int listFiles(SOCKET clientSocket, char* path){
    printf("listar arquivos.\n");

    int packageAmount = 0;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    FILE* fptr = fopen(path, "rb");

    fseek(fptr, 0L, SEEK_END);
    packageAmount = ceil((float)ftell(fptr)/BUFFER_SIZE);

    sprintf(buffer, "%d", packageAmount);

    send(clientSocket, buffer, strlen(buffer), 0);  // Envia quantidade de pacotes
    memset(buffer, 0, BUFFER_SIZE);

    rewind(fptr);

    for(int i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        fread(buffer, 1, BUFFER_SIZE, fptr);
        send(clientSocket, buffer, BUFFER_SIZE, 0);
    }

    printf("list executado.\n");

    fclose(fptr);
    return 0;
}

int deleteFile(SOCKET clientSocket, folder* fold){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    printf("remover arquivo.\n");

    send(serversSockets[0], "2", 2, 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(clientSocket, buffer, BUFFER_SIZE, 0);   // Recebe informações do arquivo (nome)
    send(serversSockets[0], buffer, strlen(buffer), 0);

    removeFile(fold, buffer);

    memset(buffer, 0, BUFFER_SIZE);
    recv(serversSockets[0], buffer, BUFFER_SIZE, 0);
    send(clientSocket, buffer, strlen(buffer), 0);

    printf("%s\n", buffer);

    return 0;
}

int saveMem(folder* fold, char* path){

    FILE* newFile = fopen(path, "wb");

    file* temp = fold->first;
    for(int i = 0; i < fold->fileAmount; i++){
        fprintf(newFile, "%s\n", temp->name);
        temp = temp->next;
    }

    fclose(newFile);

    return 0;
}

int addFile(folder* fold, char* fileName){
    file* newFile = malloc(sizeof(file));

    char* name = malloc(sizeof(char) * strlen(fileName));
    strcpy(name, fileName);

    newFile->name = name;
    newFile->next = NULL;

    if(fold ->first == NULL){
        fold->first = newFile;
        fold->last = newFile;
    }
    else if(strcmp(fileName, fold->first->name) < 0){
        newFile->next = fold->first;
        fold->first = newFile;
    }
    else{
        file* current = fold->first;

        while(current->next != NULL && strcmp(current->next->name, fileName) < 0){
            current = current->next;
        }

        newFile->next = current->next;
        current->next = newFile;
    }

    fold->fileAmount++;

    return 0;
}

int removeFile(folder* fold, char* fileName){

    if(fold->first == NULL){
        printf("erro em removeFile, folder vazio.\n");
        return 1;
    }

    file* current = fold->first;
    file* previous = NULL;
    
    while(current != NULL){
        if(strcmp(current->name, fileName) == 0){
            if(previous == NULL){       // primeiro da lista
                fold->first = current->next;
                if(current->next == NULL){      // unico da lista
                    fold->last = NULL;
                } 
            }
            else{
                previous->next = current->next;
            }

            
            free(current->name);
          
            
            free(current);
            fold->fileAmount--;

            return 0;
        }
        previous = current;
        current = current->next;
    }

    printf("erro em reveFile, file nao encontrado");
    return 1;
}

int showFiles(folder* fold){
    file* temp = fold->first;
    for(int i = 0; i < fold->fileAmount; i++){
        printf("%s\n", temp->name);
        temp = temp->next;
    }
}

int uploadFile(SOCKET clientSocket, folder* fold){
    char buffer[BUFFER_SIZE];
    int packageAmount = 0;
    int i = 0;
    printf("inserir arquivo.\n");

    send(serversSockets[0], "3", 2, 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(clientSocket, buffer, BUFFER_SIZE, 0);   // Recebe informações do arquivo
    send(serversSockets[0], buffer, BUFFER_SIZE, 0);

    // Tokeniza e separa as informações
    char* token = strtok(buffer, ";");
    packageAmount = atoi(token);

    token = strtok(NULL, ";");
    char* fileName = malloc(sizeof(token));
    strcpy(fileName, token);

    printf("file name: %s\n", fileName);
    printf("pkg amount: %d\n", packageAmount);

    memset(buffer, 0, BUFFER_SIZE);
    recv(serversSockets[0], buffer, BUFFER_SIZE, 0);

    if(strcmp(buffer, "1") != 0){
        printf("Arquivo ja existe.\n");
        strcpy(buffer, "Erro: Arquivo ja existe.\n");
        send(clientSocket, buffer, (int)strlen(buffer), 0);
        return 1;
    }

    send(clientSocket, buffer, (int)strlen(buffer), 0);   // Envia informação que está tudo certo e pode começar a mandar os pacotes
    memset(buffer, 0, BUFFER_SIZE);

    for(i = 0; i < packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        recv(clientSocket, buffer, BUFFER_SIZE, 0);
        //fprintf(fptr, buffer);
        send(serversSockets[0], buffer, BUFFER_SIZE, 0);
    }

    memset(buffer, 0, BUFFER_SIZE);
    recv(serversSockets[0], buffer, BUFFER_SIZE, 0);
    send(clientSocket, buffer, strlen(buffer), 0);

    addFile(fold, fileName);
    free(fileName);

    return 0;
}

int downloadFile(SOCKET clientSocket){
    printf("baixar arquivo.\n");

    char buffer[BUFFER_SIZE];
    int packageAmount = 0;

    send(serversSockets[0], "4", 2, 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(clientSocket, buffer, BUFFER_SIZE, 0);   // Recebe nome do arquivo
    send(serversSockets[0], buffer, strlen(buffer), 0);     // envia nome do arquivo pro servidor

    memset(buffer, 0, BUFFER_SIZE);
    recv(serversSockets[0], buffer, BUFFER_SIZE, 0);   // Recebe se o arquivo foi aberto
    send(clientSocket, buffer, strlen(buffer), 0);

    if(strcmp(buffer, "1") != 0){
        printf("%s", buffer);
        return 1;
    }

    memset(buffer, 0, BUFFER_SIZE);
    recv(serversSockets[0], buffer, BUFFER_SIZE, 0);    //Recebe quantidade de pacotes
    send(clientSocket, buffer, strlen(buffer), 0);      // envia quantidade de pacotes pro client

    packageAmount = atoi(buffer);

    for(int i = 0; i <packageAmount; i++){
        memset(buffer, 0, BUFFER_SIZE);
        recv(serversSockets[0], buffer, BUFFER_SIZE, 0);
        send(clientSocket, buffer, BUFFER_SIZE, 0);
    }

    printf("Download concluido.\n");
    
    return 0;
}

int main() {
    WSADATA wsaData;
    int result;

    // Inicializa WinSock
    result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", result);
        return 1;
    }

    struct addrinfo hints = {0}, *res = NULL, *server = NULL;
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // para bind

    // Obter endereços para bind
    result = getaddrinfo(NULL, CLIENTS_PORT, &hints, &res);
    if (result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerrorA(result));
        WSACleanup();
        return 1;
    }

    result = getaddrinfo(NULL, SERVERS_PORT, &hints, &server);
    if (result != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerrorA(result));
        WSACleanup();
        return 1;
    }

    // Cria socket de escuta
    SOCKET clientListenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (clientListenSocket == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    SOCKET serverListenSocket = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if(serverListenSocket == INVALID_SOCKET){
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    // Reuso de endereço
    BOOL opt = TRUE;
    setsockopt(clientListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    setsockopt(serverListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // Bind
    result = bind(clientListenSocket, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    if (result == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    result = bind(serverListenSocket, server->ai_addr, (int)server->ai_addrlen);
    freeaddrinfo(server);
    if(result == SOCKET_ERROR){
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    // Listen
    result = listen(clientListenSocket, BACKLOG);
    if (result == SOCKET_ERROR) {
        fprintf(stderr, "listen failed: %d\n", WSAGetLastError());
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    result = listen(serverListenSocket, BACKLOG);
    if (result == SOCKET_ERROR) {
        fprintf(stderr, "listen failed: %d\n", WSAGetLastError());
        closesocket(clientListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Servidor para client ouvindo na porta %s\n", CLIENTS_PORT);
    printf("Servidor para server ouvindo na porta %s\n", SERVERS_PORT);

    // Inciar thread de aceitação de client
    uintptr_t threadHandle = _beginthreadex(
        NULL,       // security
        0,          // stack size (default)
        clientAccept,
        (void*)(size_t)clientListenSocket,
        0,          // flags
        NULL        // thread id
    );
    if (threadHandle == 0) {
        fprintf(stderr, "_beginthreadex failed\n");
        closesocket(clientListenSocket);
    } else {
        CloseHandle((HANDLE)threadHandle);
    }

    // Loop de aceitação de servidor
    
    
    while (1) {

        serverAccept(serverListenSocket);


    }
    

    // Nunca chega aqui.
    //closesocket(clientListenSocket);
    //WSACleanup();
    return 0;
}