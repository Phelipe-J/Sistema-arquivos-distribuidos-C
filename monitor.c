#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include "fileStructure.h"

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\monitor\\"     //caminho da pasta do monitor

#define BACKLOG 10
#define BUFFER_SIZE 1024
#define MAX_SERVER_AMOUNT 1

SOCKET serversSockets[MAX_SERVER_AMOUNT];
int serverAmount = 0;

int listFiles(SOCKET, folder*);
int deleteFile(SOCKET, folder*);
int uploadFile(SOCKET, folder*);
int downloadFile(SOCKET);

file* addFile(file*, char*);
folder* addFolder(folder*, char*, folder*);
file* findFile(file*, char*);
folder* findFolder(folder*, char*);
file* removeFile(file*, char*);
void freeFiles(file*);
void freeFolders(folder*);
folder* removeFolder(folder*, char*);
void saveFiles(file*, FILE*);
void saveFolders(folder*, FILE*);
int memSave(folder*, char*);
char* getLine(FILE*);
folder* memLoad(FILE*);
void getFiles(file*, char*);
void getFolders(folder*, char*);

unsigned __stdcall client_handler(void *arg) {      // Thread cliente
    SOCKET clientSocket = (SOCKET)(size_t)arg;
    char* clientName = "client1";
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

    FILE* clientInfo = fopen(path, "rb");

    if(clientInfo == NULL){
        FILE* clientInfo = fopen(path, "wb+");
        fprintf(clientInfo, "%s<\n>\n", clientName);
        rewind(clientInfo);
    }
    
    folder* root = memLoad(clientInfo);

    fclose(clientInfo);

    folder* currentFolder = root;

    while(1){
        bytes = recv(clientSocket, recBuffer, BUFFER_SIZE, 0);
        if(bytes > 0){
            //printf("Mensagem recebida (%llu): %s", (unsigned long long)clientSocket, recBuffer);
            opcode = atoi(recBuffer);
            
            fflush(stdout);

            switch (opcode)
            {
            case 1:
                listFiles(clientSocket, currentFolder);
                break;

            case 2:
                deleteFile(clientSocket, currentFolder);
                memSave(currentFolder, path);
                break;

            case 3:
                uploadFile(clientSocket, currentFolder);
                memSave(currentFolder, path);
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

file* addFile(file* root, char* name){
    if(root == NULL){
        file* newFile = malloc(sizeof(file));

        if(newFile == NULL){
            printf("Arquivo invalido por algum motivo.\n");
            return NULL;
        }

        char* newFileName = malloc(strlen(name));
        strcpy(newFileName, name);

        newFile->name = newFileName;
        newFile->left = NULL;
        newFile->right = NULL;

        return newFile;
    }

    int cmp = strcmp(name, root->name);
    if(cmp < 0){
        root->left = addFile(root->left, name);
    }
    else if(cmp > 0){
        root->right = addFile(root->right, name);
    }
    else{
        printf("arquivo com esse nome ja existe");
    }

    return root;
}

folder* addFolder(folder* root, char* name, folder* parent){
    if(root == NULL){
        folder* newFolder = malloc(sizeof(folder));

        if(newFolder == NULL){
            printf("Arquivo invalido por algum motivo.\n");
            return NULL;
        }

        //printf("strlen: %d", strlen(name));
        char* newFolderName = malloc(sizeof(char) * strlen(name));

        if(newFolderName == NULL){
            perror("Erro ao alocar nome");
            printf("strlen: %s", strlen(name));
        }

        strcpy(newFolderName, name);

        newFolder->name = newFolderName;
        newFolder->parentFolder = parent;
        newFolder->files = NULL;
        newFolder->left = NULL;
        newFolder->right = NULL;
        newFolder->subFolders = NULL;

        return newFolder;
    }

    int cmp = strcmp(name, root->name);

    if(cmp < 0){
        root->left = addFolder(root->left, name, parent);
    }
    else if(cmp > 0){
        root->right = addFolder(root->right, name, parent);
    }
    else{
        printf("pasta com esse nome ja existe");
    }

    return root;
}

file* findFile(file* root, char* name){
    if(root == NULL){
        printf("Arquivo invalido.\n");
        return NULL;
    }

    int cmp = strcmp(name, root->name);

    if(cmp == 0){
        return root;
    }
    else if(cmp < 0){
        return findFile(root->left, name);
    }
    else{
        return findFile(root->right, name);
    }
}

folder* findFolder(folder* root, char* name){

    if(root == NULL){
        printf("Pasta invalida.\n");
        return NULL;
    }

    int cmp = strcmp(name, root->name);

    if(cmp == 0){
        return root;
    }
    else if(cmp < 0){
        return findFolder(root->left, name);
    }
    else{
        return findFolder(root->right, name);
    }
}

file* removeFile(file* root, char* name){
    if(root == NULL) return NULL;

    int cmp = strcmp(name, root->name);

    if(cmp < 0){
        return removeFile(root->left, name);
    }
    else if(cmp > 0){
        return removeFile(root->right, name);
    }
    else{
        if(root->left == NULL){
            file* temp = root->right;
            free(root->name);
            free(root);
            return temp;
        }
        else if(root->right == NULL){
            file* temp = root->left;
            free(root->name);
            free(root);
            return temp;
        }
        else{
            file* child = root->right;
            while(child->left != NULL){
                child = child->left;
            }
            free(root->name);
            root->name = child->name;       // Passar todas a informações de arquivo pro novo cara
            root->right = removeFile(root->right, child->name);
        }
    }
    return root;
}

void freeFiles(file* root){
    if(root == NULL) return;

    freeFiles(root->left);
    freeFiles(root->right);
    free(root->name);
    free(root);
}

void freeFolders(folder* root){
    if(root == NULL) return;

    freeFolders(root->left);
    freeFolders(root->right);
    freeFiles(root->files);
    freeFolders(root->subFolders);
    free(root->name);
    free(root);
}

folder* removeFolder(folder* root, char* name){
    if(root == NULL) return NULL;

    int cmp = strcmp(name, root->name);

    if(cmp < 0){
        return removeFolder(root->left, name);
    }
    else if(cmp > 0){
        return removeFolder(root->right, name);
    }
    else{
        if(root->left == NULL){
            folder* temp = root->right;

            freeFiles(root->files);
            freeFolders(root->subFolders);

            free(root->name);
            free(root);
            return temp;
        }
        else if(root->right == NULL){
            folder* temp = root->left;

            freeFiles(root->files);
            freeFolders(root->subFolders);

            free(root->name);
            free(root);
            return temp;
        }
        else{
            folder* child = root->right;
            while(child->left != NULL){
                child = child->left;
            }
            free(root->name);
            root->name = child->name;
            root->right = removeFolder(root->right, child->name);
        }
    }
    return root;
}

void saveFiles(file* root, FILE* fptr){

    if(root == NULL) return;

    fprintf(fptr, "%s:", root->name);

    saveFiles(root->right, fptr);
}

void saveFolders(folder* root, FILE* fptr){
    if(root == NULL) return;

    saveFolders(root->left, fptr);

    fprintf(fptr, "%s<", root->name);

    saveFiles(root->files,fptr);
    fprintf(fptr, "\n");
    saveFolders(root->subFolders, fptr);
    fprintf(fptr, ">\n", root->name);
    saveFolders(root->right, fptr);
}

int memSave(folder* root, char* path){
    FILE* fptr = fopen(path, "wb");

    saveFolders(root, fptr);

    fclose(fptr);
    return 0;
}

char* getLine(FILE* fptr){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    char* fullLine = NULL;
    size_t lineLength = 0;

    while(fgets(buffer, BUFFER_SIZE, fptr)){
        size_t bufferLength = strlen(buffer);
        char* newLine = realloc(fullLine, lineLength + bufferLength + 1);
        fullLine = newLine;
        memcpy(fullLine + lineLength, buffer, bufferLength+1);
        lineLength += bufferLength;

        if(fullLine[lineLength-1] == '\n' || feof(fptr)){
            fullLine[lineLength-1] = '\0';

            //printf("%s", fullLine);
            //printf("\nfile pos: %d\n", (int)ftell(fptr));

            lineLength = 0;
            return fullLine;
        }
    }
}

folder* memLoad(FILE* fptr){
    folder* root = malloc(sizeof(folder));
    char* line = getLine(fptr);

    char* fileItems = strchr(line, '<');

    *fileItems = '\0';

    char* folderName = malloc(strlen(line));
    strcpy(folderName,line);
    //printf("\nline: %s\n", folderName);

    root->name = folderName;
    root->left = NULL;
    root->right = NULL;
    root->parentFolder = NULL;
    root->subFolders = NULL;
    root->files = NULL;

    fileItems++;
    char* token = strtok(fileItems, ":");
    while(token){
        //printf("f: %s\n", token);
        root->files = addFile(root->files, token);
        token = strtok(NULL, ":");
    }

    free(line);

    folder* currentFolder = root;

    while(!feof(fptr)){
        line = getLine(fptr);
        if(line == NULL) break;
        if(line[0] == '>'){
            if(currentFolder->parentFolder != NULL) currentFolder = currentFolder->parentFolder;
            continue;
        }

        char* fileItems = strchr(line, '<');
        *fileItems = '\0';

        char* subFolderName = malloc(strlen(line));
        strcpy(subFolderName, line);
        currentFolder->subFolders = addFolder(currentFolder->subFolders, subFolderName, currentFolder);
        
        folder* tempfolder = findFolder(currentFolder->subFolders, subFolderName);

        if(tempfolder != NULL) currentFolder = tempfolder;
        
        fileItems++;
        char* token = strtok(fileItems, ":");
        while(token){
            //printf("f: %s\n", token);
            currentFolder->files = addFile(currentFolder->files, token);
            token = strtok(NULL, ":");
        }

        free(line);
    }

    return root;
}

void getFiles(file* root, char* storage){
    if(root != NULL){
        getFiles(root->left, storage);
        strcat(storage, root->name);
        strcat(storage, "\n");
        getFiles(root->right, storage);
    }
}

void getFolders(folder* root, char* storage){
    if(root != NULL){
        getFolders(root->left, storage);
        strcat(storage, root->name);
        strcat(storage, "\n");
        getFolders(root->right, storage);
    }
}

int listFiles(SOCKET clientSocket, folder* root){
    printf("listar arquivos.\n");

    int packageAmount = 0;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Fazer algo pra caso seja maior que o buffer
    sprintf(buffer, "Pasta: %s\nArquivos:\n", root->name);
    getFiles(root->files, buffer);
    strcat(buffer, "\nPastas:\n");
    getFolders(root->subFolders, buffer);

    send(clientSocket, buffer, BUFFER_SIZE, 0);

    printf("list executado.\n");

    return 0;
}

int deleteFile(SOCKET clientSocket, folder* root){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    printf("remover arquivo.\n");

    send(serversSockets[0], "2", 2, 0);

    memset(buffer, 0, BUFFER_SIZE);
    recv(clientSocket, buffer, BUFFER_SIZE, 0);   // Recebe informações do arquivo (nome)
    send(serversSockets[0], buffer, strlen(buffer), 0);

    root->files = removeFile(root->files, buffer);

    memset(buffer, 0, BUFFER_SIZE);
    recv(serversSockets[0], buffer, BUFFER_SIZE, 0);
    send(clientSocket, buffer, strlen(buffer), 0);

    printf("%s\n", buffer);

    return 0;
}

int uploadFile(SOCKET clientSocket, folder* root){
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

    root->files = addFile(root->files, fileName);
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