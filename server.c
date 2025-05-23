#include "communication.h"
#include "info.h"
//OBS MONITOR_IP foi definido em info.h

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\server\\"      //Caminho da pasta do servidor

SOCKET connectionSocket;

int deleteFile();
int uploadFile();
int downloadFile();
int createFolder();
int copyFile();

BOOL WINAPI consoleHandler(DWORD signal){
    if(signal == CTRL_CLOSE_EVENT){
        printf("Fechando aplicação...\n");
        shutdown(connectionSocket, SD_BOTH);
        closesocket(connectionSocket);
        WSACleanup();
    }
    return TRUE;
}

int communication(){
    
    char buffer[BUFFER_SIZE];
    int bytes = 0;
    package* pkg;
    
    while(1){
        bytes = receivePackage(connectionSocket, &pkg, 0);

        if(strcmp(pkg->type, DATA) != 0){
            sendError(connectionSocket, UNEXPECTED_TYPE, "Tipo de pacote inesperado.\n");
            free(pkg);
            continue;
        }

        if(bytes > 0){
            int opcode = atoi(pkg->data);

            switch (opcode)
            {
            case DEL:
                deleteFile();
                break;

            case UP:
                uploadFile();
                break;

            case DW:
                downloadFile();
                break;

            case CF:
                createFolder();
                break;

            case CP:
                copyFile();
                break;
            default:
                break;
            }
        }

        free(pkg);
    }
}

int deleteFile(){
    char path[BUFFER_SIZE];
    package* pkg;

    int result = 1;

    strcpy(path, PATH);

    printf("Remover arquivo.\n");

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){        //recebe nome do arquivo
        printf("Conexao com monitor perdida.\n");
        return -1;
    }
    if(strcmp(pkg->type, DATA) != 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    strcat(path, pkg->data);
    free(pkg);
    printf("path: %s\n", path);

    result = remove(path);

    if(result == 0){
        //sucesso
        pkg = fillPackage(DATA, NORMAL, "1\0", 2);
        sendPackage(connectionSocket, pkg, 0);
        free(pkg);
        return 0;
    }
    else{
        printf("Falha na remocao.\n");
        sendError(connectionSocket, UNKNOWN_ERROR, "Falha na remocao.\n");
        return -1;
    }
    return -1;
}

int uploadFile(){
    int packageAmount = 0;
    int i = 0;
    char path[BUFFER_SIZE];
    package* pkg;

    strcpy(path, PATH);

    printf("inserir arquivo.\n");

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){        // Recebe informações do arquivo (namePath)
        printf("Conexao com monitor perdida.\n");
        free(pkg);
        return -1;
    }
    if(strcmp(pkg->type, DATA) != 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    strcat(path, pkg->data);

    free(pkg);

    FILE* fptr = 0;

    // Verifica se arquivo já existe
    fptr = fopen(path, "rb");
    if(fptr != NULL){
        printf("Arquivo ja existe.\n");
        sendError(connectionSocket, NORMAL, "Arquivo ja existe.\n");
        fclose(fptr);
        return -1;
    }

    printf("path: %s\n", path);

    // Cria e abre arquivo
    fptr = fopen(path, "wb");
    
    if(fptr == NULL){
        perror("arquivo nao foi criado");
        sendError(connectionSocket, UNKNOWN_ERROR, "O arquivo nao pode ser criado por algum motivo.\n");
        return -1;
    }

    pkg = fillPackage(DATA, NORMAL, "1\0", 2);
    sendPackage(connectionSocket, pkg, 0);              // Envia informação que está tudo certo e pode começar a mandar os pacotes
    free(pkg);

    while(1){
        int result = receivePackage(connectionSocket, &pkg, 0);
        if(result <= 0){
            printf("Conexao com monitor perdida.\n");
            fclose(fptr);
            remove(path);
            free(pkg);
            return -1;
        }
        if(strcmp(pkg->type, ERRO) == 0){
            printf("ERRO: %s: %s", pkg->code, pkg->data);
            fclose(fptr);
            remove(path);
            free(pkg);
            return -1;
        }

        if(strcmp(pkg->type, FEOF) == 0){
            break;
        }

        fwrite(pkg->data, 1, pkg->dataSize, fptr);

        free(pkg);
    }

    fclose(fptr);
    free(pkg);

    printf("arquivo criado.\n");

    pkg = fillPackage(DATA, NORMAL, "Arquivo Criado\n", strlen("Arquivo Criado\n")+1);
    sendPackage(connectionSocket, pkg, 0);
    
    return 0;
}

int downloadFile(){
    printf("download arquivo.\n");

    int packageAmount = 0;
    char buffer[DATA_SIZE];
    char path[BUFFER_SIZE];
    package* pkg;

    memset(path, 0, BUFFER_SIZE);
    strcpy(path, PATH);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){        //recebe nome do arquivo
        printf("Conexao com monitor perdida.\n");
        free(pkg);
        return -1;
    }
    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    char fileName[BUFFER_SIZE];
    strcpy(fileName, pkg->data);
    free(pkg);
    strcat(path, fileName);

    FILE* fptr = fopen(path, "rb");

    if(fptr == NULL){
        printf("falha ao abrir arquivo.\n");
        sendError(connectionSocket, NORMAL, "Arquivo nao encontrado.\n");

        return -1;
    }

    pkg = fillPackage(DATA, NORMAL, "1\0", 2);
    sendPackage(connectionSocket, pkg, 0);              // Envia informação que está tudo certo e pode começar a receber os pacotes
    free(pkg);

    while(!feof(fptr)){
        memset(buffer, 0, DATA_SIZE);
        int dataSize = fread(buffer, 1, DATA_SIZE, fptr);

        pkg = fillPackage(DATA, NORMAL, buffer, dataSize);
        if(sendPackage(connectionSocket, pkg, 0) <= 0){
            printf("Conexao com servidor perdida.\n");
            free(pkg);
            return -1;
        }

        free(pkg);
    }

    fclose(fptr);

    pkg = fillPackage(FEOF, NORMAL, "Acabou o arquivo.\n", strlen("Acabou o arquivo.\n")+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    printf("Download concluido.\n");
    return 0;
}

int copyFile(){
    package* fileName;
    if(receivePackage(connectionSocket, &fileName, 0) <= 0){        //recebe nome do arquivo
        printf("Conexao com monitor perdida.\n");
        free(fileName);
        return -1;
    }
    if(strcmp(fileName->type, ERRO) == 0){
        printf("ERRO: %s: %s", fileName->code, fileName->data);
        free(fileName);
        return -1;
    }

    package* copyName;
    if(receivePackage(connectionSocket, &copyName, 0) <= 0){        //recebe nome da cópia
        printf("Conexao com monitor perdida.\n");
        free(fileName);
        free(copyName);
        return -1;
    }
    if(strcmp(copyName->type, ERRO) == 0){
        printf("ERRO: %s: %s", copyName->code, copyName->data);
        free(fileName);
        free(copyName);
        return -1;
    }
    
    char filePath[BUFFER_SIZE];
    strcpy(filePath, PATH);
    strcat(filePath, fileName->data);

    FILE* filePtr = fopen(filePath, "rb");

    if(filePtr == NULL){
        printf("Falha ao abrir o arquivo.\n");
        sendError(connectionSocket, UNKNOWN_ERROR, "Falha ao abrir o arquivo.\n");
        free(fileName);
        free(copyName);
        return -1;
    }

    char copyPath[BUFFER_SIZE];
    strcpy(copyPath, PATH);
    strcat(copyPath, copyName->data);

    FILE* copyPtr = fopen(copyPath, "wb");

    if(copyPtr == NULL){
        printf("Falha ao criar a copia.\n");
        sendError(connectionSocket, UNKNOWN_ERROR, "Falha ao criar a copia.\n");
        fclose(filePtr);
        free(fileName);
        free(copyName);
        return -1;
    }
    free(fileName);
    free(copyName);

    char* buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int dataSize = 0;

    while(!feof(filePtr)){
        dataSize = fread(buffer, 1, BUFFER_SIZE, filePtr);
        fwrite(buffer, 1, dataSize, copyPtr);
        memset(buffer, 0, BUFFER_SIZE);
    }

    fclose(filePtr);
    fclose(copyPtr);

    package* pkg = fillPackage(DATA, NORMAL, "Copia criada.\n", strlen("Copia criada.\n")+1);
    sendPackage(connectionSocket, pkg, 0);                      // Envia resultado
    free(pkg);

    return 0;
}

int main() {
    WSADATA wsaData;
    connectionSocket = INVALID_SOCKET;
    struct addrinfo hints = {0}, *result = NULL;
    
    SetConsoleCtrlHandler(consoleHandler, TRUE);    // Manipulador de controle (para fechar o socket quando fechar a janela)

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
    connectionSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (connectionSocket == INVALID_SOCKET) {
        fprintf(stderr, "socket falhou: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Conecta ao servidor
    if (connect(connectionSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "connect falhou: %d\n", WSAGetLastError());
        closesocket(connectionSocket);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);
    printf("Conectado ao monitor %s:%s\n", MONITOR_IP, SERVERS_PORT);


    communication(connectionSocket);

    closesocket(connectionSocket);
    WSACleanup();
    return 0;
}
