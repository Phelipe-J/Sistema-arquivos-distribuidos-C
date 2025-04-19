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

file* addFile(file*, char*);
BOOL addFolder(folder*, char*); 
folder* addFolderCore(folder*, char*, folder*, folder**);
file* findFile(file*, char*);
folder* findFolder(folder*, char*);
void saveFiles(file*, FILE*);
void saveFolders(folder*, FILE*);
int memSave(folder*, char*);
char* getLine(FILE*);
folder* memLoad(FILE*);
void getFiles(file*, char*);
void getFolders(folder*, char*);
folder* getParentFolder(folder*);

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

BOOL addFolder(folder* root, char* name){
    folder* temp = NULL;
    root->subFolders = addFolderCore(root->subFolders, name, root, &temp);

    return(temp != NULL);
}

folder* addFolderCore(folder* root, char* name, folder* parent, folder** newFolderAddress){
    if(name == NULL) return root;

    folder* newFolder = malloc(sizeof(folder));
    if(newFolder == NULL){
        printf("falha na alocacao da pasta.\n");
        return root;
    }

    char* newFolderName = malloc(strlen(name)+1);
    if(newFolderName == NULL){
        printf("falha na alocacao do nome da pasta.\n");
        return root;
    }

    strcpy(newFolderName, name);

    newFolder->name = newFolderName;
    newFolder->right = NULL;
    newFolder->left = NULL;
    newFolder->files = NULL;
    newFolder->subFolders = NULL;
    newFolder->parentFolder = parent;

    if(root == NULL){
        *newFolderAddress = newFolder; 
        return newFolder;
    }

    folder* temp = root;
    folder* lastFolder;

    while(temp != NULL){
        if(strcmp(newFolderName, temp->name) < 0){
            lastFolder = temp;
            temp = temp->left;
            continue;
        }
        else if(strcmp(newFolderName, temp->name) > 0){
            lastFolder = temp;
            temp = temp->right;
            continue;
        }
        else if(strcmp(newFolderName, temp->name) == 0){
            printf("arquivo ja existe na arvore.\n");
            return root;
        }
    }

    if(strcmp(newFolderName, lastFolder->name) < 0){
        lastFolder->left = newFolder;
        *newFolderAddress = newFolder; 
        return root;
    }
    else if(strcmp(newFolderName, lastFolder->name) > 0){
        lastFolder->right = newFolder;
        *newFolderAddress = newFolder; 
        return root;
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

int countFiles(file* root){
    if(root == NULL) return 0;

    return 1 + countFiles(root->left) + countFiles(root->right);
}

folder* getParentFolder(folder* root){
    folder* tempFolder = root->parentFolder;

    if(tempFolder != NULL){
        return tempFolder;
    }
    return root;
}

char* getFullFolderPath(folder* root, int* pathSize){
    if(root == NULL) return NULL;

    char* buffer1 = malloc(BUFFER_SIZE);
    char* buffer2 = malloc(BUFFER_SIZE);

    memset(buffer1, 0, BUFFER_SIZE);
    memset(buffer2, 0, BUFFER_SIZE);

    int curretSize = BUFFER_SIZE;

    folder* currentFolder = root;

    strcpy(buffer1, currentFolder->name);

    while(currentFolder->parentFolder != NULL){
        currentFolder = currentFolder->parentFolder;
        strcpy(buffer2, currentFolder->name);
        strcat(buffer2, "\\");
        strcat(buffer2, buffer1);
        strcpy(buffer1, buffer2);

        if(strlen(buffer1) > ((2*curretSize)/3)){
            curretSize *= 2;
            buffer1 = realloc(buffer1, curretSize);
            buffer2 = realloc(buffer2, curretSize);
        }
    }

    free(buffer2);

    *pathSize = curretSize;
    return buffer1;
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

    root->name = folderName;
    root->left = NULL;
    root->right = NULL;
    root->parentFolder = NULL;
    root->subFolders = NULL;
    root->files = NULL;

    fileItems++;
    char* token = strtok(fileItems, ":");
    while(token){
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
        addFolder(currentFolder, subFolderName);
        
        folder* tempfolder = findFolder(currentFolder->subFolders, subFolderName);

        if(tempfolder != NULL) currentFolder = tempfolder;
        
        fileItems++;
        char* token = strtok(fileItems, ":");
        while(token){
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

unsigned long hash(const char* str){
    unsigned long hash = 5381;

    int c;
    while((c = *str++)){
        hash = ((hash << 5) + hash + c);
    }

    return hash;
}