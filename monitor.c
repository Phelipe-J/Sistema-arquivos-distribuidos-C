#include "fileStructure.h"

#define CLIENTS_PORT "6669"
#define SERVERS_PORT "6670"

#define PATH "S:\\Sistemas_Distribuidos\\monitor\\"     //caminho da pasta do monitor

#define BACKLOG 10
#define MAX_SERVER_AMOUNT 1

SOCKET serversSockets[MAX_SERVER_AMOUNT];
int serverAmount = 0;

int listFiles(SOCKET, folder*);
int deleteFile(SOCKET, folder*);
int uploadFile(SOCKET, folder*);
int downloadFile(SOCKET, folder*);

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
            case 1:
                listFiles(clientSocket, currentFolder);
                break;

            case 2:
                deleteFile(clientSocket, currentFolder);
                memSave(root, path);
                break;

            case 3:
                uploadFile(clientSocket, currentFolder);
                memSave(root, path);
                break;

            case 4:
                downloadFile(clientSocket, currentFolder);
                break;
                    
            case 5:

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

file* addFile(file* root, char* name){
    if(name == NULL) return root;

    file* newFile = malloc(sizeof(file));
    char* newFileName = malloc(strlen(name)+1);
    strcpy(newFileName, name);

    newFile->name = newFileName;
    newFile->left = NULL;
    newFile->right = NULL;

    if(root == NULL){
        return newFile;
    }

    file* temp = root;
    file* lastFile;

    while(temp != NULL){
        if(strcmp(newFileName, temp->name) < 0){
            lastFile = temp;
            temp = temp->left;
            continue;
        }
        else if(strcmp(newFileName, temp->name) > 0){
            lastFile = temp;
            temp = temp->right;
            continue;
        }
        else if(strcmp(newFileName, temp->name) == 0){
            printf("arquivo ja existe na arvore.\n");
            return root;
        }
    }

    if(strcmp(newFileName, lastFile->name) < 0){
        lastFile->left = newFile;
        return root;
    }
    else if(strcmp(newFileName, lastFile->name) > 0){
        lastFile->right = newFile;
        return root;
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
            free(remove->name);
            free(remove);
            return newRoot;
        }
        else if(remove->right == NULL){
            folder* newRoot = remove->left;
            free(remove->name);
            free(remove);
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

        // conteudo de temp --> remove
        free(remove->name);
        remove->name = temp->name;
        // Parent não é necessários, todos têm o mesmo parent por estarem na mesma subfolder
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

void saveFiles(file* root, FILE* fptr){
    if(root == NULL) return;

    fprintf(fptr, "%s:", root->name);

    saveFiles(root->left, fptr);
    saveFiles(root->right, fptr);
}

void saveFolders(folder* root, FILE* fptr){
    if(root == NULL) return;

    fprintf(fptr, "%s<", root->name);
    saveFiles(root->files,fptr);
    fprintf(fptr, "\n");

    saveFolders(root->subFolders, fptr);
    fprintf(fptr, ">\n", root->name);

    saveFolders(root->left, fptr);
    
    saveFolders(root->right, fptr);
}

int memSave(folder* root, char* path){      // Alterar para salvar em um arquivo temporário antes, e então substituir o original
    FILE* fptr = fopen(path, "wb");         // Isso é  pra caso o monitor seja interrompido durante o salvamento, não perca tudo.

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

int deleteFile(SOCKET clientSocket, folder* root){
    package* pkg;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    printf("remover arquivo.\n");

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

    char fileName[DATA_SIZE];
    strcpy(fileName, pkg->data);

    if(findFile(root->files, fileName) == NULL){
        printf("Arquivo nao encontrado");
        memset(buffer, 0, BUFFER_SIZE);

        sendError(clientSocket, NORMAL, "Arquivo nao encontrado.\n");

        free(pkg);
        return -1;
    }

    package* code = fillPackage(DATA, NORMAL, "2\0", 2);
    if(sendPackage(serversSockets[0], code, 0) <= 0){           // Envia operação pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        free(code);
        return -1;
    }
    free(code);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){           // envia nome do arquivo pro server
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    memset(buffer, 0, BUFFER_SIZE);       

    if(receivePackage(serversSockets[0], &pkg, 0) <= 0){            // recebe resultado de deleção
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->type, DATA) != 0){
        printf("ERRO: %s: %s", pkg->code, pkg->data);
        sendError(clientSocket, pkg->code, pkg->data);
        free(pkg);
        return -1;
    }

    if(strcmp(pkg->data, "1\0") == 0){
        root->files = removeFile(root->files, fileName);
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

    pkg = fillPackage(DATA, NORMAL, "3\0", 2);
    if(sendPackage(serversSockets[0], pkg, 0) <= 0){               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }
    free(pkg);

    if(receivePackage(clientSocket, &pkg, 0) <= 0){                // Recebe informações do arquivo
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

    if(sendPackage(serversSockets[0], pkg, 0) <= 0){                // envia informações do arquivo pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(pkg);
        return -1;
    }                        

    // Tokeniza e separa as informações
    char* token = strtok(pkg->data, ";");
    packageAmount = atoi(token);

    token = strtok(NULL, ";");
    char* fileName = malloc(sizeof(token));
    strcpy(fileName, token);
    token = NULL;

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
    
    free(fileName);
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

        return -1;
    }

    package* code;
    code = fillPackage(DATA, NORMAL, "4\0", 2);
    if(sendPackage(serversSockets[0], code, 0) <= 0){               // envia operação pro servidor
        printf("Conexao com servidor perdida.\n");
        sendError(clientSocket, LOST_CONNECTION, "Conexao com servidor perdida.\n");
        free(code);
        return -1;
    }
    free(code);

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