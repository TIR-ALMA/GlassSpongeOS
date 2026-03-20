// vfs.h
#ifndef VFS_H
#define VFS_H

#include "types.h" // Используем типы из вашей ОС

#define MAX_FILENAME 64
#define MAX_PATH 256
#define MAX_MOUNTS 8

// Типы файлов
typedef enum {
    FILE_TYPE_REGULAR = 0,
    FILE_TYPE_DIRECTORY = 1
} file_type_t;

// Коды ошибок
typedef enum {
    E_OK = 0,
    E_INVALID = -1,
    E_NOMEM = -2,
    E_NOTFOUND = -3,
    E_NOTDIR = -4,
    E_EXISTS = -5
} kerr_t;

// Forward declarations
struct vfs_node;
struct filesystem;

// Структура операций файла
typedef struct {
    kerr_t (*open)(struct vfs_node* node);
    kerr_t (*close)(struct vfs_node* node);
    kerr_t (*read)(struct vfs_node* node, void* buffer, size_t size, size_t* bytes_read);
    kerr_t (*write)(struct vfs_node* node, const void* buffer, size_t size, size_t* bytes_written);
    kerr_t (*create)(struct vfs_node* parent, const char* name, file_type_t type, struct vfs_node** result);
    kerr_t (*delete)(struct vfs_node* node);
    kerr_t (*readdir)(struct vfs_node* node, uint32_t index, struct vfs_node** result);
} vfs_operations_t;

// Узел VFS (представляет файл или директорию)
typedef struct vfs_node {
    char name[MAX_FILENAME];
    file_type_t type;
    size_t size;
    uint32_t flags;

    struct vfs_node* parent;
    struct filesystem* fs;      // Файловая система, владеющая этим узлом
    void* fs_data;              // Специфичные для ФС данные

    const vfs_operations_t* ops; // Операции для этого узла
} vfs_node_t;

// Структура файловой системы
typedef struct filesystem {
    char name[32];              // например, "ramfs", "ext4"
    void* fs_private;           // Специфичные для ФС данные
    vfs_node_t* root;           // Корень этой ФС

    // Операции уровня ФС
    kerr_t (*mount)(struct filesystem* fs, const char* device);
    kerr_t (*unmount)(struct filesystem* fs);
} filesystem_t;

// Точка монтирования
typedef struct {
    char path[MAX_PATH];        // Где смонтировано (например, "/", "/mnt")
    filesystem_t* fs;           // Файловая система, смонтированная здесь
    uint8_t in_use;
} mount_point_t;

// Интерфейс VFS
kerr_t vfs_init(void);
kerr_t vfs_mount(filesystem_t* fs, const char* path);
kerr_t vfs_unmount(const char* path);

// Файловые операции
vfs_node_t* vfs_open(const char* path);
kerr_t vfs_close(vfs_node_t* node);
kerr_t vfs_read(vfs_node_t* node, void* buffer, size_t size, size_t* bytes_read);
kerr_t vfs_write(vfs_node_t* node, const void* buffer, size_t size, size_t* bytes_written);
kerr_t vfs_create_file(const char* path);
kerr_t vfs_create_directory(const char* path);
kerr_t vfs_delete(const char* path);

// Операции с директориями
kerr_t vfs_list(const char* path);
void vfs_print_tree(vfs_node_t* node, int depth);

// Вспомогательные функции
vfs_node_t* vfs_resolve_path(const char* path);
const char* vfs_basename(const char* path);
char* vfs_dirname(const char* path);

// Операция копирования
kerr_t vfs_copy_file(const char* dest, const char* source);

#endif

