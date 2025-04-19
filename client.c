#include "communication.h"

//OBS MONITOR_IP foi definido em info.h

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\client\\"

SOCKET connectionSocket;

typedef int (*funcptr)(char*);

typedef struct {
    const char* name;
    funcptr func;
}command;

int listFiles(char*);
int deleteFile(char*);
int uploadFile(char*);
int downloadFile(char*);
int downloadFolder(char*);
int goInFolder(char*);
int goBackFolder(char*);
int createFolder(char*);
int deleteFolder(char*);
int copyFile(char*);
int copyFolder(char*);

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
    int bytesSent = 0;
    int bytesRecv = 0;
    char recvBuf[BUFFER_SIZE];
    char commandBuffer[BUFFER_SIZE];

    command commands[] = {
        {"ls", listFiles},
        {"del", deleteFile},
        {"up", uploadFile},
        {"dw", downloadFile},
        {"dwf", downloadFolder},
        {"go", goInFolder},
        {"bck", goBackFolder},
        {"cf", createFolder},
        {"dlf", deleteFolder},
        {"cp", copyFile},
        {"cpf", copyFolder},
        {NULL, NULL}
    };

    while(1){
        fflush(stdout);
        if(fgets(commandBuffer, BUFFER_SIZE, stdin)){
            if(strlen(commandBuffer) <= 1) continue;
            commandBuffer[strcspn(commandBuffer, "\n")] = '\0';

            char* commandInput = strtok(commandBuffer, " ");
            char* argument = strtok(NULL, "");

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
                chosenCommand->func(argument);
            }
        }
    }
    return 0;
}

int listFiles(char* argument){
    char c[3];
    sprintf(c, "%d", LS);

    package* code = fillPackage(DATA, NORMAL, c, strlen(c)+1);

    sendPackage(connectionSocket, code, 0);
    free(code);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int packageAmount = 0;

    printf("\n\n");

    package* msg;
    if(receivePackage(connectionSocket, &msg, 0) <= 0){
        printf("Conexao perdida com o servidor.\n");
        free(msg);
        return -1;
    }
    
    if(strcmp(msg->type, DATA) != 0){
        printf("Erro %s: $s", msg->code, msg->data);
        free(msg);
        return -1;
    }

    printf(msg->data);


    free(msg);

    return 0;
}

int deleteFile(char* fileName){
    char c[3];
    sprintf(c, "%d", DEL);

    package* code = fillPackage(DATA, NORMAL, c, strlen(c)+1);

    sendPackage(connectionSocket, code, 0);                // envia operação
    free(code);

    package* pkg = fillPackage(DATA, NORMAL, fileName, strlen(fileName)+1);
    sendPackage(connectionSocket, pkg, 0);
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, DATA) != 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    printf("%s", pkg->data);

    free(pkg);
    return 0;
}

int uploadFile(char* filePath){
    char c[3];
    sprintf(c, "%d", UP);

    package* code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    sendPackage(connectionSocket, code, 0);                // envia operação
    free(code);

    char* fileName;
    int packageAmount = 0;
    package* pkg;

    fileName = strrchr(filePath, '\\');
    fileName++;

    FILE* fptr = fopen(filePath, "rb");

    fseek(fptr, 0L, SEEK_END);

    pkg = fillPackage(DATA, NORMAL, fileName, strlen(fileName)+1);

    if(sendPackage(connectionSocket, pkg, 0) <= 0){                 // Envia informações do arquivo
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    rewind(fptr);           

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){                 // Espera resposta para começar a enviar os pacotes
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, DATA) != 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    free(pkg);

    char buffer[DATA_SIZE];
    while(!feof(fptr)){                                 // Envia os pacotes
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

    if(receivePackage(connectionSocket, &pkg, 0) > 0){
        printf("%s", pkg->data);
    }
    else{
        printf("Conexao com servidor perdida, resultado incerto.\n");
    }

    free(pkg);
    return 0;
}

int downloadFile(char* fileName){
    FILE* fptr;

    char path[BUFFER_SIZE];
    memset(path, 0, BUFFER_SIZE);
    strcpy(path, PATH);
    strcat(path, fileName);

    fptr = fopen(path, "rb");

    if((fptr != NULL)){
        printf("Arquivo ja existe no destino.\n");
        fclose(fptr);

        return -1;
    }

    char c[3];
    sprintf(c, "%d", DW);

    package* code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    sendPackage(connectionSocket, code, 0);                // envia operação
    free(code);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int packageAmount = 0;

    package* pkg = fillPackage(DATA, NORMAL, fileName, strlen(fileName)+1);
    sendPackage(connectionSocket, pkg, 0);
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){                 // Espera resposta para começar a enviar os pacotes
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, DATA) != 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    fptr = fopen(path, "wb");

    while(1){
        free(pkg);
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
            free(pkg);
            break;
        }

        fwrite(pkg->data, 1, pkg->dataSize, fptr);
    }

    fclose(fptr);

    printf("Download concluido.\n");

    return 0;
}

int downloadFolder(char* folderName){
    char c[3];
    sprintf(c, "%d", DWF);

    package* code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    sendPackage(connectionSocket, code, 0);                // envia operação
    free(code);

    package* pkg;
    pkg = fillPackage(DATA, NORMAL, folderName, strlen(folderName)+1);
    sendPackage(connectionSocket, pkg, 0);                      // envia nome da pasta pro monitor
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){             // recebe se pasta não existe ou quantidade de arquivos
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    int netInt = 0;
    memcpy(&netInt, pkg->data, sizeof(int));

    int fileAmount = ntohl(netInt);

    char path[BUFFER_SIZE];
    strcpy(path, PATH);
    strcat(path, folderName);

    free(pkg);

    int check = mkdir(path);                    // Criar pasta aqui

    if(check < 0){
        printf("Pasta nao pode ser criada.\n");
        sendError(connectionSocket, UNKNOWN_ERROR, "Client nao conseguiu criar pasta.\n");
        return -1;
    }

    pkg = fillPackage(DATA, NORMAL, "1\0", 2);
    sendPackage(connectionSocket, pkg, 0);
    free(pkg);

    FILE* fptr;
    char* filePath;

    for(int i = 0; i < fileAmount; i++){
        if(receivePackage(connectionSocket, &pkg, 0) <= 0){             // recebe nome do arquivo
            printf("Conexao com servidor perdida.\n");
            free(pkg);
            return -1;
        }

        if(strcmp(pkg->type, ERRO) == 0){
            printf("ERRO: %s: %s", pkg->code, pkg->data);
            free(pkg);
            return -1;
        }

        filePath = malloc(strlen(pkg->data) + strlen(path) + 2);
        strcpy(filePath, path);
        strcat(filePath, "\\");
        strcat(filePath, pkg->data);

        fptr = fopen(filePath, "wb");
        free(pkg);

        // receber arquivo
        while(1){
            int result = receivePackage(connectionSocket, &pkg, 0);
            if(result <= 0){
                printf("Conexao com monitor perdida.\n");
                fclose(fptr);
                remove(filePath);
                free(pkg);
                free(filePath);
                return -1;
            }
            if(strcmp(pkg->type, ERRO) == 0){
                printf("ERRO: %s: %s", pkg->code, pkg->data);
                fclose(fptr);
                remove(filePath);
                free(pkg);
                free(filePath);
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
        free(filePath);

        pkg = fillPackage(DATA, NORMAL, "Arquivo recebindo.\n", strlen("Arquivo recebindo.\n")+1);
        sendPackage(connectionSocket, pkg, 0);
        free(pkg);
    }
    
    printf("Download Concluido.\n");

    return 0;
}

int goInFolder(char* folderName){
    char c[3];
    sprintf(c, "%d", GO);

    package* pkg;

    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, folderName, strlen(folderName)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    return 0;
}

int goBackFolder(char* argument){
    char c[3];
    sprintf(c, "%d", BCK);

    package* pkg;

    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    return 0;
}

int createFolder(char* folderName){
    package* pkg;
    char c[3];
    sprintf(c, "%d", CF);

    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, folderName, strlen(folderName)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s\n", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    printf("%s", pkg->data);
    free(pkg);
    return 0;
}

int deleteFolder(char* folderName){
    package* pkg;
    char c[3];
    sprintf(c, "%d", DLF);

    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia operação pro monitor
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, folderName, strlen(folderName)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s\n", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    printf("%s", pkg->data);
    free(pkg);
    return 0;
}

int copyFile(char* arguments){
    char fileName[DATA_SIZE];
    char destinationFolder[DATA_SIZE];

    char* token = strtok(arguments, " ");
    strcpy(fileName, token);

    token = strtok(NULL, "");
    strcpy(destinationFolder, token);

    package* pkg;
    char c[3];
    sprintf(c, "%d", CP);

    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia operação pro monitor
        printf("Conexao com servidor perdida.\n");
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, fileName, strlen(fileName)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia nome do arquivo
        printf("Conexao com servidor perdida.\n");
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, destinationFolder, strlen(destinationFolder)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia caminho da pasta
        printf("Conexao com servidor perdida.\n");
        return -1;
    }
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){         // Recebe resultado da operação
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s\n", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    printf("%s", pkg->data);
    free(pkg);
    return 0;
}

int copyFolder(char* arguments){
    char sourceFolder[DATA_SIZE];
    char destinationFolder[DATA_SIZE];

    char* token = strtok(arguments, " ");
    strcpy(sourceFolder, token);

    token = strtok(NULL, "");
    strcpy(destinationFolder, token);

    package* pkg;
    char c[3];
    sprintf(c, "%d", CPF);

    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia operação pro monitor
        printf("Conexao com servidor perdida.\n");
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, sourceFolder, strlen(sourceFolder)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia nome da pasta que vai ser copiado pro monitor
        printf("Conexao com servidor perdida.\n");
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, destinationFolder, strlen(destinationFolder)+1);
    if(sendPackage(connectionSocket, pkg, 0) <= 0){             // Envia caminho da pasta onde vai ficar a cópia
        printf("Conexao com servidor perdida.\n");
        return -1;
    }
    free(pkg);

    if(receivePackage(connectionSocket, &pkg, 0) <= 0){         // Recebe resultado da operação
        printf("Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s\n", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    printf("%s", pkg->data);
    free(pkg);
    return 0;
}

int main() {
    WSADATA wsaData;
    connectionSocket = INVALID_SOCKET;
    struct addrinfo hints = {0}, *result = NULL;
    
    SetConsoleCtrlHandler(consoleHandler, TRUE);    // Manipulador de controle (para fechar o socket quando fechar a janela)

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
    connectionSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (connectionSocket == INVALID_SOCKET) {
        fprintf(stderr, "socket falhou: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // 5. Conecta ao servidor
    if (connect(connectionSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "connect falhou: %d\n", WSAGetLastError());
        closesocket(connectionSocket);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);
    printf("Conectado ao servidor %s:%s\n", MONITOR_IP, CLIENTS_PORT);
    
    communication(connectionSocket);

    //closesocket(connectionSocket);
    //WSACleanup();
    return 0;
}