#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>
#include <sys/stat.h>
#include "info.h"

#define BUFFER_SIZE 999
#define CODE_SIZE 3
#define TYPE_SIZE 5
#define DATA_SIZE (BUFFER_SIZE - CODE_SIZE - TYPE_SIZE - sizeof(int))

#define FEOF "feof\0"
#define DATA "data\0"
#define ERRO "erro\0"

#define UNKNOWN_ERROR "-1\0"
#define NORMAL "00\0"
#define LOST_CONNECTION "01\0"
#define UNEXPECTED_TYPE "03\0"

typedef struct Package{
    char type[TYPE_SIZE];
    char code[CODE_SIZE];
    int dataSize;
    char data[DATA_SIZE];
}package;

typedef struct File{
    char* name;
    struct File* left;
    struct File* right;
}file;

typedef struct Folder{
    char* name;
    struct File* files;         // Raiz de arquivos
    struct Folder* parentFolder;
    struct Folder* subFolders;  // Raiz de subpastas
    struct Folder* left;
    struct Folder* right;
}folder;

package* fillPackage(char* type, char* code, char* data, int dataSize){
    package* pkg = malloc(sizeof(package));
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
            *pkg = malloc(sizeof(package));
            fillPackage(ERRO, LOST_CONNECTION, "Conexao perdida.\n", strlen("Conexao perdida.\n")+1);
            return -1;
        }
        totalReceived += received;
    }

    *pkg = malloc(sizeof(package));

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
