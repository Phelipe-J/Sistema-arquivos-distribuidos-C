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

/* Package:
    types:
    code
    data
    erro
*/

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

    //strcpy(pkg->data, data);

    //memcpy(pkg->type, type, TYPE_SIZE);
    //memcpy(pkg->code, code, CODE_SIZE);


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


    /*
    
    package* pkg = malloc(sizeof(package));

    char* temp = malloc(size);
    strcpy(temp, buffer);

    char* token = strtok(temp, ":");
    //printf("%s\n", token);
    strcpy(pkg->type, token);
    token = strtok(NULL, ":");
    //printf("%s\n", token);
    strcpy(pkg->code, token);
    token = strtok(NULL, ":");
    //printf("%s\n", token);
    strcpy(pkg->data, token);

    free(temp);
    */

    /*                                                      // ESSE AQUI FOI O ÚNICO QUE FUNCIONOU ANTES DA BIG MUDANÇA
    package* pkg = malloc(sizeof(package));
    int i = 0;
    int j = 0;
    while(buffer[i] != ':'){
        pkg->type[j] = buffer[i];
        i++;
        j++;
    }
    pkg->type[i] = '\0';
    i++;
    j = 0;
    while(buffer[i] != ':'){
        pkg->code[j] = buffer[i];
        i++;
        j++;
    }
    pkg->code[j] = '\0';
    i++;
    buffer += i;
    strncpy(pkg->data, buffer, DATA_SIZE);
    */

    /*
    strncpy(pkg->type, buffer, TYPE_SIZE-1);
    //pkg->type[TYPE_SIZE-1] = '\0';
    buffer += TYPE_SIZE-1;
    strncpy(pkg->code, buffer, CODE_SIZE-1);
    //pkg->code[CODE_SIZE-1] = '\0';
    buffer += CODE_SIZE-1;
    strcpy(pkg->data, buffer);
    */

    //printf("asdnsa: %d\n", strlen(pkg->data));
    //printf("asdasdnsa: %d\n", strlen(buffer));





    //return pkg;
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
    
    /*
    if(result <= 0){
        *pkg = malloc(sizeof(package));
        return result;
    }

    printf("Pkg size: %d\n", result);

    *pkg = openPackage(buffer);

    if(strcmp((*pkg)->type, DATA) != 0){
        printf("psahjkdfjikholdfs\n");
    }

    result -= (TYPE_SIZE + CODE_SIZE);
    */

    //return received;
}

int __stdcall sendPackage(SOCKET sock, package* pkg, int flag){
    char buffer[BUFFER_SIZE];

    /*
    int bufferSize = snprintf(buffer, BUFFER_SIZE, "%s:%s:%s", pkg->type, pkg->code, pkg->data);    // <-- O PROBLEMA TÁ AQUI, IMAGEM NÃO É STRING, NÃO
                                                                                                    // DÁ PRA PASSAR ESSA MERDA COMO %S, PROVAVELMENTE TEREU
                                                                                                    // QUE USAR OUTRA FUNÇÃO
    bufferSize++;

    if(strcmp(pkg->type, DATA) != 0){
        printf("dsfhjklfdsjdfs\n");
    }

    printf("buffer: %d\n", bufferSize);

    */

    serialize(pkg, buffer);

    int bufferSize = TYPE_SIZE + CODE_SIZE + sizeof(int) + pkg->dataSize;

    return send(sock, buffer, BUFFER_SIZE, flag);
}

void sendError(SOCKET sock, char* code, char* msg){
    package* errMsg = fillPackage(ERRO, code, msg, strlen(msg)+1);
    sendPackage(sock, errMsg, 0);
    free(errMsg);
}