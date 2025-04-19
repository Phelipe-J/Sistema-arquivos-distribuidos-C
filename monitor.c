#include "communication.h"

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\monitor\\"     //caminho da pasta do monitor

#define BACKLOG 10
#define MAX_SERVER_AMOUNT 1

SOCKET serversSockets[MAX_SERVER_AMOUNT];
int serverAmount = 0;

int listFiles(SOCKET, folder*);
int deleteFileCommunication(SOCKET, folder*);
int deleteFile(SOCKET, folder*, char*);
int uploadFile(SOCKET, folder*);
int downloadFile(SOCKET, folder*);
int downloadFolder(SOCKET, folder*);
folder* goInFolder(SOCKET, folder*);
int createFolder(SOCKET, folder*);
int deleteFolder(SOCKET, folder*);
int copyFile(SOCKET, folder*, folder*);
int copyFolder(SOCKET, folder*, folder*);

file* removeFile(file*, char*);
void freeFiles(file**, SOCKET, folder*);
void freeFolders(folder*, SOCKET);
BOOL removeFolder(folder*, char*, SOCKET);
folder* removeFolderCore(folder*, char*, BOOL*, SOCKET);

unsigned __stdcall client_handler(void *arg) {      // Thread cliente
    SOCKET clientSocket = (SOCKET)(size_t)arg;
    char* clientName = "client1";
    char recBuffer[BUFFER_SIZE];
    int bytes = 0;
    char** files;
    char buffer[BUFFER_SIZE];
    int opcode = 0;

    char path[BUFFER_SIZE];
    memset(path, 0, BUFFER_SIZE);
    strcpy(path, PATH);
    strcat(path, "clientInfo.bin");

    FILE* clientInfo = fopen(path, "rb");

    if(clientInfo == NULL){
        clientInfo = fopen(path, "wb");
        fprintf(clientInfo, "%s<\n>\n", clientName);
        fclose(clientInfo);
        clientInfo = fopen(path, "rb");
    }
    
    folder* root = memLoad(clientInfo);

    fclose(clientInfo);

    folder* currentFolder = root;

    while(1){
        package* pkg;
        bytes = receivePackage(clientSocket, &pkg, 0);

        if(strcmp(pkg->type, DATA) != 0){
            sendError(clientSocket, UNEXPECTED_TYPE, "Tipo de pacote inesperado\n");
            free(pkg);
            continue;
        }

        if(bytes > 0){
            opcode = atoi(pkg->data);
            
            fflush(stdout);

            switch (opcode)
            {
            case LS:
                listFiles(clientSocket, currentFolder);
                break;

            case DEL:
                deleteFileCommunication(clientSocket, currentFolder);
                memSave(root, path);
                break;

            case UP:
                uploadFile(clientSocket, currentFolder);
                memSave(root, path);
                break;

            case DW:
                downloadFile(clientSocket, currentFolder);
                break;

            case CF:
                createFolder(clientSocket, currentFolder);
                memSave(root, path);
                break;

            case DLF:
                deleteFolder(clientSocket, currentFolder);
                memSave(root, path);
                break;

            case GO:                     // Entrar em pasta
                currentFolder = goInFolder(clientSocket, currentFolder);
                break;

            case BCK:                     //Voltar pasta
                currentFolder = getParentFolder(currentFolder);
                break;

            case DWF:
                downloadFolder(clientSocket, currentFolder);
                break;

            case CP:
                copyFile(clientSocket, currentFolder, root);
                break;
            
            case CPF:
                copyFolder(clientSocket, currentFolder, root);
                break;
            default:
                printf("Codigo invalido.\n");
                break;
            }
        }
        else if(bytes < 0){
            printf("Erro: %d\n", WSAGetLastError());
            break;
        }
        else{
            printf("Conexão encerrada pelo client.\n");
            break;
        }
        free(pkg);
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

int listFiles(SOCKET clientSocket, folder* root){
    printf("listar arquivos.\n");

    int packageAmount = 0;
    char buffer[DATA_SIZE];
    memset(buffer, 0, DATA_SIZE);

    // Fazer algo pra caso seja maior que o buffer ????
    sprintf(buffer, "Pasta: %s\n\n|Arquivos|:\n", root->name);
    getFiles(root->files, buffer);
    strcat(buffer, "\n|Pastas|:\n");
    getFolders(root->subFolders, buffer);

    package* pkg = fillPackage(DATA, NORMAL, buffer, strlen(buffer)+1);

    sendPackage(clientSocket, pkg, 0);

    free(pkg);

    printf("list executado.\n");

    return 0;
}

int deleteFileCommunication(SOCKET clientSocket, folder* root){
    package* pkg;

    if(receivePackage(clientSocket, &pkg, 0) <= 0){        // Recebe informações do arquivo (nome)
        printf("Conexao perdida com o client.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao perdida com o client.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){       // DEU ERRO
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(serversSockets[0], pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    int result = deleteFile(clientSocket, root, pkg->data);

    free(pkg);

    if(result < 0){
        sendError(clientSocket, UNKNOWN_ERROR, "Falha na remocao.\n");
        return -1;
    }

    pkg = fillPackage(DATA, NORMAL, "Arquivo removido.\n", strlen("Arquivo removido.\n")+1);
    sendPackage(clientSocket, pkg, 0);                         // Envia resultado pro client
    free(pkg);
    
    return 0;
}

int deleteFile(SOCKET clientSocket, folder* root, char* name){
    package* pkg;
    
    printf("remover arquivo.\n");

    if(findFile(root->files, name) == NULL){
        printf("Arquivo nao encontrado");
        sendError(clientSocket, NORMAL, "Arquivo nao encontrado.\n");
        return -1;
    }

    int pathSize = 0;
    char* namePath = getFullFolderPath(root, &pathSize);

    if(pathSize < (strlen(namePath) + strlen(name) + 2)){
        namePath = realloc(namePath, strlen(namePath) + strlen(name) + 2);
    }

    strcat(namePath, "\\");
    strcat(namePath, name);

    unsigned long pathHash = hash(namePath);
    free(namePath);
    char namePathSend[256];
    sprintf(namePathSend, "%lu.dat", pathHash);

    root->files = removeFile(root->files, name);

    package* code;
    char c[3];
    sprintf(c, "%d", DEL);
    code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(serversSockets[0], code, 0) <= 0){                               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(code);
        return -1;
    }
    free(code);

    
    pkg = fillPackage(DATA, NORMAL, namePathSend, strlen(namePathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){           // envia nome do arquivo pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);     

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){            // recebe resultado de deleção
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->data, ERRO) == 0){
        root->files = addFile(root->files, name);       // adiciona o arquivo de volta em caso de erro no servidor
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(clientSocket, pkg->code, pkg->data);
        free(pkg);
        return -1;
    }
    free(pkg);      

    pkg = fillPackage(DATA, NORMAL, "Arquivo removido.\n", strlen("Arquivo removido.\n")+1);
    sendPackage(clientSocket, pkg, 0);                         // Envia resultado pro client

    free(pkg);

    return 0;
}

int uploadFile(SOCKET clientSocket, folder* root){
    package* pkg;
    int packageAmount = 0;
    int i = 0;
    printf("inserir arquivo.\n");             

    char c[3];
    sprintf(c, "%d", UP);
    pkg = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    if(receivePackage(clientSocket, &pkg, 0) <= 0){                // Recebe informações do arquivo (nome)
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        return -1;
    }
    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(serversSockets[0], pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    char fileName[DATA_SIZE];
    strcpy(fileName, pkg->data);

    int pathSize = 0;
    char* namePath = getFullFolderPath(root, &pathSize);

    if(pathSize < (strlen(namePath) + strlen(fileName) + 2)){
        namePath = realloc(namePath, strlen(namePath) + strlen(fileName) + 2);
    }

    strcat(namePath, "\\");
    strcat(namePath, fileName);

    unsigned long pathHash = hash(namePath);
    free(namePath);
    char namePathSend[256];
    sprintf(namePathSend, "%lu.dat", pathHash);

    free(pkg);

    // send hash
    pkg = fillPackage(DATA, NORMAL, namePathSend, strlen(namePathSend)+1);

    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                // envia informações do arquivo pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }                        

    free(pkg);           

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){                               // Recebe se o servidor pode receber o arquivo
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(sendPackage(clientSocket, pkg, 0) <= 0){                // Envia informação SE está tudo certo e pode começar a mandar os pacotes
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){                // se não puder enviar os pacotes, termina função
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }
    free(pkg);

    while(1){
        if(receivePackage(clientSocket, &pkg, 0) <= 0){                   // recebe pacote do client
            printf("Conexao com client perdida.\n");
            sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
            free(pkg);
            return -1;
        }
        if(strcmp(pkg->type, ERRO) == 0){
            printf("ERRO: %s: %s", pkg->code, pkg->data);
            sendError(serversSockets[0], pkg->code, pkg->data);
            free(pkg);
            return -1;
        }

        if(sendPackage(serversSockets[0], pkg, 0) <= 0){               // envia pacote pro servidor
            printf("Conexao com servidor perdida.\n");
            sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
            free(pkg);
            return -1;
        }
        if(strcmp(pkg->type, FEOF) == 0){
            break;
        }
        free(pkg);
    }
    free(pkg);

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){                   // Recebe output do servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(sendPackage(clientSocket, pkg, 0) <= 0){               // envia output pro client
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, DATA) == 0){
        root->files = addFile(root->files, fileName);
    }
    
    free(pkg);

    return 0;
}

int downloadFile(SOCKET clientSocket, folder* root){
    printf("baixar arquivo.\n");
    package* pkg;

    if(receivePackage(clientSocket, &pkg, 0) <= 0){                // Recebe nome do arquivo
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        return -1;
    }
    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(serversSockets[0], pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    if(findFile(root->files, pkg->data) == NULL){                      // Procura no sistema de arquivos
        printf("Arquivo nao encontrado");
        sendError(clientSocket, ERRO, "Arquivo nao encontrado.\n");
        free(pkg);
        return -1;
    }

    package* code;

    char c[3];
    sprintf(c, "%d", DW);
    code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(serversSockets[0], code, 0) <= 0){                               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(code);
        return -1;
    }
    free(code);

    char fileName[DATA_SIZE];
    strcpy(fileName, pkg->data);

    int pathSize = 0;
    char* namePath = getFullFolderPath(root, &pathSize);

    if(pathSize < (strlen(namePath) + strlen(fileName) + 2)){
        namePath = realloc(namePath, strlen(namePath) + strlen(fileName) + 2);
    }

    strcat(namePath, "\\");
    strcat(namePath, fileName);

    unsigned long pathHash = hash(namePath);
    free(namePath);
    char namePathSend[256];
    sprintf(namePathSend, "%lu.dat", pathHash);

    free(pkg);

    pkg = fillPackage(DATA, NORMAL, namePathSend, strlen(namePathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                // envia nome do arquivo pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){                               // Recebe se o arquivo foi aberto
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(sendPackage(clientSocket, pkg, 0) <= 0){                // Envia se o arquivo foi aberto
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, ERRO) == 0){                // se deu erro na abertura do arquivo, encerra a função
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        free(pkg);
        return -1;
    }
    free(pkg);

    while(1){
        if(receivePackage(serversSockets[0], &pkg, 0) <= 0){                   // recebe pacote do servidor
            printf("Conexao com client perdida.\n");
            sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
            free(pkg);
            return -1;
        }
        if(strcmp(pkg->type, ERRO) == 0){
            printf("ERRO: %s: %s", pkg->code, pkg->data);
            sendError(clientSocket, pkg->code, pkg->data);
            free(pkg);
            return -1;
        }

        if(sendPackage(clientSocket, pkg, 0) <= 0){               // envia pacote pro client
            printf("Conexao com client perdida.\n");
            sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
            free(pkg);
            return -1;
        }

        if(strcmp(pkg->type, FEOF) == 0){
            break;
        }
        free(pkg);
    }
    free(pkg);

    printf("Download concluido.\n");
    return 0;
}

void sendAllFiles(SOCKET clientSocket, file* root, folder* parentFolder, int* result){
    if(root == NULL) return;

    sendAllFiles(clientSocket, root->left, parentFolder, result);
    if(*result < 0){
        return;
    }

    package* pkg;

    pkg = fillPackage(DATA, NORMAL, root->name, strlen(root->name)+1);
    sendPackage(clientSocket, pkg, 0);                                     // Envia nome do arquivo pro client
    free(pkg);
    
    char c[3];
    sprintf(c, "%d", DW);
    package* code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(serversSockets[0], code, 0) <= 0){                               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(code);
        *result = -1;
        return;
    }
    free(code);

    char fileName[DATA_SIZE];
    strcpy(fileName, root->name);

    int pathSize = 0;
    char* namePath = getFullFolderPath(parentFolder, &pathSize);

    if(pathSize < (strlen(namePath) + strlen(fileName) + 2)){
        namePath = realloc(namePath, strlen(namePath) + strlen(fileName) + 2);
    }

    strcat(namePath, "\\");
    strcat(namePath, fileName);

    unsigned long pathHash = hash(namePath);
    free(namePath);
    char namePathSend[256];
    sprintf(namePathSend, "%lu.dat", pathHash);

    pkg = fillPackage(DATA, NORMAL, namePathSend, strlen(namePathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                         // Envia nome do arquivo pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        *result = -1;
        return;
    }

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){                               // Recebe se o arquivo foi aberto
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        *result = -1;
        return;
    }

    if(strcmp(pkg->type, ERRO) == 0){
        sendPackage(clientSocket, pkg, 0);
        free(pkg);
        *result = -1;
        return;
    }
    free(pkg);

    while(1){               // Recebe o arquivo
        if(receivePackage(serversSockets[0], &pkg, 0) <= 0){                   // recebe pacote do servidor
            printf("Conexao com client perdida.\n");
            sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
            free(pkg);
            *result = -1;
            return;
        }
        if(strcmp(pkg->type, ERRO) == 0){
            printf("ERRO: %s: %s", pkg->code, pkg->data);
            sendError(clientSocket, pkg->code, pkg->data);
            free(pkg);
            *result = -1;
            return;
        }

        if(sendPackage(clientSocket, pkg, 0) <= 0){               // envia pacote pro client
            printf("Conexao com client perdida.\n");
            sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
            free(pkg);
            *result = -1;
            return;
        }

        if(strcmp(pkg->type, FEOF) == 0){
            free(pkg);
            break;
        }
        free(pkg);
    }

    if(receivePackage(clientSocket, &pkg, 0) <= 0){                // Recebe se client criou o arquivo com sucesso
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        *result = -1;
        return;
    }
    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(serversSockets[0], pkg->code, pkg->data);
        free(pkg);
        *result = -1;
        return;
    }

    free(pkg);

    sendAllFiles(clientSocket, root->right, parentFolder, result);
    if(*result < 0){
        return;
    }
    return;
}

int downloadFolder(SOCKET clientSocket, folder* root){
    printf("baixar pasta.\n");
    package* pkg;

    if(receivePackage(clientSocket, &pkg, 0) <= 0){                // Recebe nome da pasta
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        return -1;
    }
    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(serversSockets[0], pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    folder* download = findFolder(root->subFolders, pkg->data);

    free(pkg);

    // ver se pasta existe
    if(download == NULL){
        printf("Pasta nao encontrada.\n");
        sendError(clientSocket, ERRO, "Pasta nao encontrado.\n");
        free(pkg);
        return -1;
    }

    int fileAmount = countFiles(download->files);
    char fileAmountSend[DATA_SIZE];

    int netInt = htonl(fileAmount);

    memcpy(fileAmountSend, &netInt, sizeof(int));

    pkg = fillPackage(DATA, NORMAL, fileAmountSend, sizeof(int));
    sendPackage(clientSocket, pkg, 0);
    free(pkg);

    if(receivePackage(clientSocket, &pkg, 0) <= 0){                // Recebe resposta do client sobre a criação da pasta
        printf("Conexao com cliente perdida.\n");
        sendError(serversSockets[0], LOST_CONNECTION, "Conexao com client perdida.\n");
        free(pkg);
        return -1;
    }
    if(strcmp(pkg->type, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(serversSockets[0], pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    free(pkg);

    int result = 0;

    sendAllFiles(clientSocket, download->files, download, &result);

    if(result < 0){
        return -1;
    }

    return 0;
}

folder* goInFolder(SOCKET clienteSocket, folder* root){
    package* name;
    receivePackage(clienteSocket, &name, 0);

    folder* tempFolder = findFolder(root->subFolders, name->data);

    if(tempFolder != NULL){
        return tempFolder;
    }

    return root;
}

int createFolder(SOCKET clientSocket, folder* root){
    printf("Criar pasta.\n");

    package* name = NULL;

    if(receivePackage(clientSocket, &name, 0) <= 0){         // Recebe nome da pasta
        printf("Erro ao receber nome da pasta.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Erro ao receber nome da pasta.\n");
        free(name);
        return -1;
    }
    if(name->dataSize <= 1){
        printf("Nome de pasta invalido.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Nome da pasta inválido.\n");
        free(name);
        return -1;
    }

    if(!addFolder(root, name->data)){
        printf("Pasta nao pode ser criada.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Pasta ja existe ou erro esquisito ocorreu.\n");
        free(name);
        return -1;
    }

    free(name);

    package* pkg = fillPackage(DATA, NORMAL, "Pasta criada.\n", strlen("Pasta criada.\n")+1);
    sendPackage(clientSocket, pkg, 0);
    free(pkg);

    return 0;
}

file* removeFile(file* root, char* name){
    if(root == NULL) return NULL;

    BOOLEAN isLeft;

    file* remove = root;
    file* parent = NULL;

    
    while(remove->left != NULL || remove->right != NULL){
        if(strcmp(name, remove->name) < 0){
            parent = remove;
            remove = remove->left;
            isLeft = TRUE;
            continue;
        }
        else if(strcmp(name, remove->name) > 0){
            parent = remove;
            remove = remove->right;
            isLeft = FALSE;
            continue;
        }
        break;
    }

    if(parent == NULL || (remove->left != NULL && remove->right != NULL)){
        //é a raiz
        if(remove->left == NULL){
            file* newRoot = remove->right;
            free(remove->name);
            free(remove);
            return newRoot;
        }
        else if(remove->right == NULL){
            file* newRoot = remove->left;
            free(remove->name);
            free(remove);
            return newRoot;
        }

        file* leftFile = remove->left;
        file* temp = remove->left;
        while(temp->right != NULL){
            temp = temp->right;
        }
        leftFile->right = temp->left;

        if(leftFile == temp){
            remove->left = leftFile->left;
        }

        // conteudo de temp --> remove
        free(remove->name);
        remove->name = temp->name;
        temp->name = NULL;
        free(temp);

        return root;
    }
    else if(remove->left == NULL){       // tem o da direita
        if(isLeft == TRUE){
            parent->left = remove->right;
        }
        else{
            parent->right = remove->right;
        }
        free(remove->name);
        free(remove);
        return root;
    }
    else if(remove->right == NULL){     //tem o da esquerda
        if(isLeft == TRUE){
            parent->left = remove->left;
        }
        else{
            parent->right = remove->left;
        }
        free(remove->name);
        free(remove);
        return root;
    }

    return root;
}

void freeFiles(file** root, SOCKET clientSocket, folder* parentFolder){
    if(*root == NULL) return;

    freeFiles(&((*root)->left), clientSocket, parentFolder);
    freeFiles(&((*root)->right), clientSocket, parentFolder);

    deleteFile(clientSocket, parentFolder, (*root)->name);
}

void freeFolders(folder* root, SOCKET clientSocket){
    if(root == NULL) return;

    freeFolders(root->left, clientSocket);
    freeFolders(root->right, clientSocket);
    freeFiles(&(root->files), clientSocket, root);
    freeFolders(root->subFolders, clientSocket);
    free(root->name);
    free(root);
}

BOOL removeFolder(folder* root, char* name, SOCKET clientSocket){
    BOOL success = FALSE;
    root->subFolders = removeFolderCore(root->subFolders, name, &success, clientSocket);
    return success;
}

folder* removeFolderCore(folder* root, char* name, BOOL* success, SOCKET clientSocket){
    if(root == NULL) return NULL;

    BOOLEAN isLeft;

    folder* remove = root;
    folder* parent = NULL;

    
    while(remove->left != NULL || remove->right != NULL){
        if(strcmp(name, remove->name) < 0){
            parent = remove;
            remove = remove->left;
            isLeft = TRUE;
            continue;
        }
        else if(strcmp(name, remove->name) > 0){
            parent = remove;
            remove = remove->right;
            isLeft = FALSE;
            continue;
        }
        break;
    }

    if(parent == NULL || (remove->left != NULL && remove->right != NULL)){
        //é a raiz
        if(remove->left == NULL){
            folder* newRoot = remove->right;
            freeFolders(remove->subFolders, clientSocket);
            freeFiles(&(remove->files), clientSocket, remove);
            free(remove->name);
            free(remove);
            *success = TRUE;
            
            return newRoot;
        }
        else if(remove->right == NULL){
            folder* newRoot = remove->left;
            freeFolders(remove->subFolders, clientSocket);
            freeFiles(&(remove->files), clientSocket, remove);
            free(remove->name);
            free(remove);
            *success = TRUE;
            return newRoot;
        }

        folder* leftFolder = remove->left;
        folder* temp = remove->left;
        while(temp->right != NULL){
            temp = temp->right;
        }
        leftFolder->right = temp->left;

        if(leftFolder == temp){
            remove->left = leftFolder->left;
        }

        freeFolders(remove->subFolders, clientSocket);
        freeFiles(&(remove->files), clientSocket, remove);
        free(remove->name);
        
        // conteudo de temp --> remove
        remove->name = temp->name;
        remove->files = temp->files;
        remove->subFolders = temp->subFolders;
        // Parent não é necessários, todos têm o mesmo parent por estarem na mesma subfolder
        temp->name = NULL;
        free(temp);
        *success = TRUE;
        return root;
    }
    else if(remove->left == NULL){       // tem o da direita
        if(isLeft == TRUE){
            parent->left = remove->right;
        }
        else{
            parent->right = remove->right;
        }
        freeFolders(remove->subFolders, clientSocket);
        freeFiles(&(remove->files), clientSocket, remove);
        free(remove->name);
        free(remove);
        *success = TRUE;
        return root;
    }
    else if(remove->right == NULL){     //tem o da esquerda
        if(isLeft == TRUE){
            parent->left = remove->left;
        }
        else{
            parent->right = remove->left;
        }
        freeFolders(remove->subFolders, clientSocket);
        freeFiles(&(remove->files), clientSocket, remove);
        free(remove->name);
        free(remove);
        *success = TRUE;
        return root;
    }
    
    return root;
}

int deleteFolder(SOCKET clientSocket, folder* root){
    printf("deletar pasta.\n");

    package* name;

    if(receivePackage(clientSocket, &name, 0) <= 0){         // Recebe nome da pasta
        printf("Erro ao receber nome da pasta.\n");
        free(name);
        return -1;
    }
    if(name->dataSize <= 1){
        printf("Nome de pasta invalido.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Nome da pasta inválido.\n");
        free(name);
        return -1;
    }

    if(!removeFolder(root, name->data, clientSocket)){
        printf("Pasta nao pode ser deletada.\n");
        //sendError(clientSocket, INVALID_ARGUMENT, "Pasta nao encontrada ou erro esquisito ocorreu.\n");
        free(name);
        return -1;
    }

    package* pkg;
    pkg = fillPackage(DATA, NORMAL, "Pasta deletada.\n", strlen("Pasta deletada.\n")+1);

    sendPackage(clientSocket, pkg, 0);
    free(pkg);

    return 0;
}

int copyFile(SOCKET clientSocket, folder* currentFolder, folder* root){
    printf("Copiar arquivo.\n");

    package* fileName;

    if(receivePackage(clientSocket, &fileName, 0) <= 0){         // Recebe nome do arquivo
        printf("Erro ao receber nome da pasta.\n");
        free(fileName);
        return -1;
    }
    if(strcmp(fileName->type, ERRO) == 0){
        printf("ERRO: %s: %s", fileName->code, fileName->data);
        free(fileName);
        return -1;
    }
    if(fileName->dataSize <= 1){
        printf("Nome de pasta invalido.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Nome de arquivo inválido.\n");
        free(fileName);
        return -1;
    }

    package* folderPath;

    if(receivePackage(clientSocket, &folderPath, 0) <= 0){         // Recebe nome da pasta de destino
        printf("Erro ao receber nome da pasta.\n");
        free(folderPath);
        free(fileName);
        return -1;
    }
    if(strcmp(folderPath->type, ERRO) == 0){
        printf("ERRO: %s: %s", folderPath->code, folderPath->data);
        free(folderPath);
        free(fileName);
        return -1;
    }
    if(folderPath->dataSize <= 1){
        printf("Nome de pasta invalido.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Nome da pasta inválido.\n");
        free(folderPath);
        free(fileName);
        return -1;
    }

    if(findFile(currentFolder->files, fileName->data) == NULL){
        printf("Arquivo nao encontrado.\n");
        sendError(clientSocket, NORMAL, "Arquivo nao encontrado.\n");
        return -1;
    }

    folder* destinationFolder = root;
    char* folderName = strtok(folderPath->data, "\\");          // identificação do client (raiz)

    while((folderName = strtok(NULL, "\\")) != NULL){
        destinationFolder = findFolder(destinationFolder->subFolders, folderName);

        if(destinationFolder == NULL){
            printf("Pasta de destino nao encontrada.\n");
            sendError(clientSocket, NORMAL, "Pasta de destino nao encontrada\n");
            free(folderPath);
            free(fileName);
            return -1;
        }

    }

    if(findFile(destinationFolder->files, fileName->data) != 0){
        sendError(clientSocket, NORMAL, "Arquivo ja existe na pasta indicada.\n");
        free(folderPath);
        free(fileName);
        return -1;
    }

    // hash do arquivo a ser copiado
    int pathSize = 0;
    char* filePath = getFullFolderPath(currentFolder, &pathSize);

    if(pathSize < (strlen(filePath) + strlen(fileName->data) + 2)){
        filePath = realloc(filePath, strlen(filePath) + strlen(fileName->data) + 2);
    }

    strcat(filePath, "\\");
    strcat(filePath, fileName->data);

    unsigned long filePathHash = hash(filePath);
    free(filePath);
    char filePathSend[256];
    sprintf(filePathSend, "%lu.dat", filePathHash);

    // hash do arquivo na nova pasta
    pathSize = 0;
    char* destinationFolderPath = getFullFolderPath(destinationFolder, &pathSize);

    if(pathSize < (strlen(destinationFolderPath) + strlen(fileName->data) + 2)){
        destinationFolderPath = realloc(destinationFolderPath, strlen(destinationFolderPath) + strlen(fileName->data) + 2);
    }

    strcat(destinationFolderPath, "\\");
    strcat(destinationFolderPath, fileName->data);

    unsigned long destinationFolderPathHash = hash(destinationFolderPath);
    free(destinationFolderPath);
    char destinationFolderPathSend[256];
    sprintf(destinationFolderPathSend, "%lu.dat", destinationFolderPathHash);

    package* code;
    char c[3];
    sprintf(c, "%d", CP);
    code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(serversSockets[0], code, 0) <= 0){                               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(code);
        return -1;
    }
    free(code);

    package* pkg;

    pkg = fillPackage(DATA, NORMAL, filePathSend, strlen(filePathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                        // envia nome do arquivo pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(folderPath);
        free(fileName);
        free(pkg);
        return -1;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, destinationFolderPathSend, strlen(destinationFolderPathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                        // envia nome da pasta de destino pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(folderPath);
        free(fileName);
        free(pkg);
        return -1;
    }
    free(pkg);

    free(folderPath);

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){            // recebe resultado de copia
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(fileName);
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->data, ERRO) == 0){
        //destinationFolder->files = removeFile(destinationFolder->files, fileName->data);
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(clientSocket, pkg->code, pkg->data);
        free(fileName);
        free(pkg);
        return -1;
    }
    destinationFolder->files = addFile(destinationFolder->files, fileName->data);
    free(fileName);

    sendPackage(clientSocket, pkg, 0);                         // Envia resultado pro client

    free(pkg);

    return 0;
}

void copyAllFiles(SOCKET clientSocket, folder* sourceFolder, folder* destinationFolder, file* sourceFile, int* result){
    if(sourceFolder == NULL) return;
    if(destinationFolder == NULL) return;
    if(sourceFile == NULL) return;

    copyAllFiles(clientSocket, sourceFolder, destinationFolder, sourceFile->left, result);
    if(result < 0){
        return;
    }

    // hash do arquivo a ser copiado
    int pathSize = 0;
    char* filePath = getFullFolderPath(sourceFolder, &pathSize);

    if(pathSize < (strlen(filePath) + strlen(sourceFile->name) + 2)){
        filePath = realloc(filePath, strlen(filePath) + strlen(sourceFile->name) + 2);
    }

    strcat(filePath, "\\");
    strcat(filePath, sourceFile->name);

    unsigned long filePathHash = hash(filePath);
    free(filePath);
    char filePathSend[256];
    sprintf(filePathSend, "%lu.dat", filePathHash);

    // hash do arquivo na nova pasta
    pathSize = 0;
    char* destinationFolderPath = getFullFolderPath(destinationFolder, &pathSize);

    if(pathSize < (strlen(destinationFolderPath) + strlen(sourceFile->name) + 2)){
        destinationFolderPath = realloc(destinationFolderPath, strlen(destinationFolderPath) + strlen(sourceFile->name) + 2);
    }

    strcat(destinationFolderPath, "\\");
    strcat(destinationFolderPath, sourceFile->name);

    unsigned long destinationFolderPathHash = hash(destinationFolderPath);
    free(destinationFolderPath);
    char destinationFolderPathSend[256];
    sprintf(destinationFolderPathSend, "%lu.dat", destinationFolderPathHash);

    package* code;
    char c[3];
    sprintf(c, "%d", CP);
    code = fillPackage(DATA, NORMAL, c, strlen(c)+1);
    if(sendPackage(serversSockets[0], code, 0) <= 0){                               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(code);
        *result = -1;
        return;
    }
    free(code);

    package* pkg;
    pkg = fillPackage(DATA, NORMAL, filePathSend, strlen(filePathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                        // envia nome do arquivo pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        *result = -1;
        return;
    }
    free(pkg);

    pkg = fillPackage(DATA, NORMAL, destinationFolderPathSend, strlen(destinationFolderPathSend)+1);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                        // envia nome da pasta de destino pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        *result = -1;
        return;
    }
    free(pkg);
    
    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){            // recebe resultado de copia
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        *result = -1;
        return;
    }

    if(strcmp(pkg->data, ERRO) == 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(clientSocket, pkg->code, pkg->data);
        free(pkg);
        *result = -1;
        return;
    }
    free(pkg);
    destinationFolder->files = addFile(destinationFolder->files, sourceFile->name);

    copyAllFiles(clientSocket, sourceFolder, destinationFolder, sourceFile->right, result);
    if(result < 0){
        return;
    }
    return;
}

int copyFolder(SOCKET clientSocket, folder* currentFolder, folder* root){
    printf("Copiar pasta.\n");

    package* sourceFolderName;

    if(receivePackage(clientSocket, &sourceFolderName, 0) <= 0){         // Recebe nome da pasta que vai ser copiada
        printf("Erro ao receber nome da pasta.\n");
        free(sourceFolderName);
        return -1;
    }
    if(strcmp(sourceFolderName->type, ERRO) == 0){
        printf("ERRO: %s: %s", sourceFolderName->code, sourceFolderName->data);
        free(sourceFolderName);
        return -1;
    }
    if(sourceFolderName->dataSize <= 1){
        printf("Nome de pasta invalido.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Nome de arquivo inválido.\n");
        free(sourceFolderName);
        return -1;
    }

    package* destinationFolderPath;

    if(receivePackage(clientSocket, &destinationFolderPath, 0) <= 0){         // Recebe nome da pasta de destino
        printf("Erro ao receber nome da pasta.\n");
        free(destinationFolderPath);
        free(sourceFolderName);
        return -1;
    }
    if(strcmp(destinationFolderPath->type, ERRO) == 0){
        printf("ERRO: %s: %s", destinationFolderPath->code, destinationFolderPath->data);
        free(destinationFolderPath);
        free(sourceFolderName);
        return -1;
    }
    if(destinationFolderPath->dataSize <= 1){
        printf("Nome de pasta invalido.\n");
        sendError(clientSocket, INVALID_ARGUMENT, "Nome de arquivo inválido.\n");
        free(destinationFolderPath);
        free(sourceFolderName);
        return -1;
    }

    folder* sourceFolder = findFolder(currentFolder->subFolders, sourceFolderName->data);

    if(sourceFolder == NULL){
        printf("Pasta nao encontrada.\n");
        sendError(clientSocket, NORMAL, "Pasta nao encontrada.\n");
        free(destinationFolderPath);
        free(sourceFolderName);
        return -1;
    }

    folder* destinationFolder = root;
    char* folderName = strtok(destinationFolderPath->data, "\\");          // identificação do client (raiz)

    while((folderName = strtok(NULL, "\\")) != NULL){
        destinationFolder = findFolder(destinationFolder->subFolders, folderName);

        if(destinationFolder == NULL){
            printf("Pasta de destino nao encontrada.\n");
            sendError(clientSocket, NORMAL, "Pasta de destino nao encontrada\n");
            free(destinationFolderPath);
            free(sourceFolderName);
            return -1;
        }
    }

    int result = 0;

    copyAllFiles(clientSocket, sourceFolder, destinationFolder, sourceFolder->files, &result);

    free(destinationFolderPath);
    free(sourceFolderName);

    if(result >= 0){
        package* pkg = fillPackage(DATA, NORMAL, "Pasta copiada.\n", strlen("Pasta copiada.\n")+1);
        sendPackage(clientSocket, pkg, 0);
        free(pkg);

        return 0;
    }

    return -1;
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