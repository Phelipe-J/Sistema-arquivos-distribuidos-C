#include "fileStructure.h"

#define LS 1
#define DEL 2
#define UP 3
#define DW 4
#define CF 5
#define DLF 6
#define GO 7
#define BCK 8
#define UPF 9
#define DWF 10
#define CP 11
#define CPF 12


#define FEOF "feof\0"
#define DATA "data\0"
#define ERRO "erro\0"

#define UNKNOWN_ERROR "-1\0"
#define NORMAL "00\0"
#define LOST_CONNECTION "01\0"
#define UNEXPECTED_TYPE "03\0"
#define INVALID_ARGUMENT "04\0"

package* fillPackage(char* type, char* code, char* data, int dataSize){
    package* pkg = (package*)malloc(sizeof(package));
    strcpy(pkg->type, type);
    strcpy(pkg->code, code);

    pkg->dataSize = dataSize;
    
    memcpy(pkg->data, data, dataSize);

    return pkg;
}

void serialize(package* pkg, char* buffer){
    int offset = 0;

    memcpy(buffer+offset, pkg->type, TYPE_SIZE);
    offset += TYPE_SIZE;

    memcpy(buffer+offset, pkg->code, CODE_SIZE);
    offset += CODE_SIZE;

    int netInt = htonl(pkg->dataSize);
    memcpy(buffer+offset, &netInt, sizeof(int));
    offset += sizeof(int);

    memcpy(buffer+offset, pkg->data, DATA_SIZE);

    int bufferSize = TYPE_SIZE + CODE_SIZE + sizeof(int) + pkg->dataSize;
}

void openPackage(package* pkg, char* buffer){
    int offset = 0;
    
    memcpy(pkg->type, buffer+offset, TYPE_SIZE);
    offset += TYPE_SIZE;

    memcpy(pkg->code, buffer+offset, CODE_SIZE);
    offset += CODE_SIZE;

    int netInt = 0;
    memcpy(&netInt, buffer+offset, sizeof(int));
    pkg->dataSize = ntohl(netInt);
    offset += sizeof(int);

    memcpy(pkg->data, buffer+offset, pkg->dataSize);
}

int receivePackage(SOCKET sock, package** pkg, int flag){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int totalReceived = 0;

    while(totalReceived < BUFFER_SIZE){
        int received = recv(sock, buffer+totalReceived, BUFFER_SIZE-totalReceived, flag);
        if(received <= 0){
            *pkg = (package*)malloc(sizeof(package));
            fillPackage(ERRO, LOST_CONNECTION, "Conexao perdida.\n", strlen("Conexao perdida.\n")+1);
            return -1;
        }
        totalReceived += received;
    }

    *pkg = (package*)malloc(sizeof(package));

    openPackage(*pkg, buffer);

    return 1;
}

int __stdcall sendPackage(SOCKET sock, package* pkg, int flag){
    char buffer[BUFFER_SIZE];

    serialize(pkg, buffer);

    int bufferSize = TYPE_SIZE + CODE_SIZE + sizeof(int) + pkg->dataSize;

    return send(sock, buffer, BUFFER_SIZE, flag);
}

void sendError(SOCKET sock, char* code, char* msg){
    package* errMsg = fillPackage(ERRO, code, msg, strlen(msg)+1);
    sendPackage(sock, errMsg, 0);
    free(errMsg);
}