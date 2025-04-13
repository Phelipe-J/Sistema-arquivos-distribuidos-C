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