// vfs.c
#include "vfs.h"
#include "lib/string.h"
#include "lib/printf.h" // Используем ваш printf
#include "mm.h" // Используем ваш менеджер памяти

// Таблица монтирования
static mount_point_t mount_table[MAX_MOUNTS];
static vfs_node_t* vfs_root = NULL;

// Статический буфер для временных строк
static char temp_path_buffer[MAX_PATH];

kerr_t vfs_init(){
    // Очищаем таблицу монтирования
    for(int i = 0; i < MAX_MOUNTS; i++) {
        mount_table[i].in_use = 0;
        mount_table[i].fs = NULL;
        mount_table[i].path[0] = '\0'; // Убедиться, что строка пуста
    }
    vfs_root = NULL;
    return E_OK;
}

kerr_t vfs_mount(filesystem_t* fs, const char* path){
    if(!fs || !path) return E_INVALID;

    // Находим свободную точку монтирования
    int slot = -1;
    for(int i = 0; i < MAX_MOUNTS; i++){
        if(!mount_table[i].in_use){
            slot = i;
            break;
        }
    }

    if(slot == -1) return E_NOMEM;

    // Вызываем функцию монтирования ФС
    if(!fs->mount) return E_INVALID;
    kerr_t err = fs->mount(fs, NULL);
    if(err != E_OK) return err;

    // Добавляем в таблицу
    strcpy(mount_table[slot].path, path);
    mount_table[slot].fs = fs;
    mount_table[slot].in_use = 1;

    // Если монтируем в корень, устанавливаем vfs_root
    if(strcmp(path, "/") == 0){
        vfs_root = fs->root;
    }

    return E_OK;
}

kerr_t vfs_unmount(const char* path){
    if(!path) return E_INVALID;
    for(int i = 0; i < MAX_MOUNTS; i++){
        if(mount_table[i].in_use && strcmp(path, mount_table[i].path) == 0){
            filesystem_t* fs = mount_table[i].fs;

            if(!fs || !fs->unmount) return E_NOTFOUND;
            kerr_t err = fs->unmount(fs);
            if(err != E_OK) return err;

            mount_table[i].in_use = 0;
            mount_table[i].fs = NULL;
            mount_table[i].path[0] = '\0';

            return E_OK;
        }
    }
    return E_NOTFOUND;
}

static filesystem_t* vfs_get_fs_for_path(const char* path){
    if(!path) return NULL;
    size_t best_match_len = 0;
    filesystem_t* best_fs = NULL;

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].in_use) {
            size_t mount_len = strlen(mount_table[i].path);
            // Проверяем, является ли mount_path префиксом path
            if (strncmp(path, mount_table[i].path, mount_len) == 0) {
                // Для избежания ложных совпадений (e.g. "/fo" matches "/foo")
                if (mount_len > best_match_len &&
                    (path[mount_len] == '/' || path[mount_len] == '\0')) {
                    best_match_len = mount_len;
                    best_fs = mount_table[i].fs;
                }
            }
        }
    }
    return best_fs;
}

vfs_node_t* vfs_resolve_path(const char* path){
    if(!vfs_root || !path) return NULL;

    if(strcmp(path, "/") == 0) return vfs_root;

    // Начинаем с корня и спускаемся по дереву
    vfs_node_t* current = vfs_root;

    // Копируем путь для парсинга
    size_t path_len = strlen(path);
    if(path_len >= MAX_PATH) return NULL; // Путь слишком длинный
    strcpy(temp_path_buffer, path);

    char* component = temp_path_buffer;
    if(*component == '/') component++; // Пропускаем начальный слэш

    char* next_slash;
    while(*component){
        next_slash = component;
        while(*next_slash && *next_slash != '/'){
            next_slash++;
        }

        char saved = *next_slash;
        *next_slash = '\0';

        // Ищем потомка с таким именем
        if (current->ops && current->ops->readdir) {
            int found = 0;
            uint32_t index = 0;
            vfs_node_t* child = NULL;

            while (current->ops->readdir(current, index, &child) == E_OK) {
                if (child && strcmp(child->name, component) == 0) {
                    current = child;
                    found = 1;
                    break;
                }
                index++;
            }

            if(!found) return NULL;
        }else{
            return NULL;
        }

        *next_slash = saved;
        if (*next_slash == '/') {
            component = next_slash + 1;
        } else {
            break;
        }
    }

    return current;
}

const char* vfs_basename(const char* path){
    if(!path) return NULL;
    const char* last_slash = path;
    for(const char* p = path; *p; p++){
        if(*p == '/'){
            last_slash = p + 1;
        }
    }
    return last_slash;
}

char* vfs_dirname(const char* path){
    if(!path) return NULL;
    size_t len = strlen(path);
    if(len >= MAX_PATH) len = MAX_PATH - 1;

    size_t last_slash = 0;
    for(size_t i = 0; i < len; i++){
        if(path[i] == '/'){
            last_slash = i;
        }
    }

    if(last_slash == 0 && len > 0 && path[0] != '/'){
        strcpy(temp_path_buffer, ".");
    } else if (last_slash == 0 && len > 0 && path[0] == '/') {
        strcpy(temp_path_buffer, "/");
    } else {
        strncpy(temp_path_buffer, path, last_slash);
        temp_path_buffer[last_slash] = '\0';
    }

    return temp_path_buffer;
}

vfs_node_t* vfs_open(const char* path) {
    return vfs_resolve_path(path);
}

kerr_t vfs_close(vfs_node_t* node) {
    if (!node || !node->ops || !node->ops->close) return E_OK;
    return node->ops->close(node);
}

kerr_t vfs_read(vfs_node_t* node, void* buffer, size_t size, size_t* bytes_read) {
    if (!node || !node->ops || !node->ops->read) return E_INVALID;
    return node->ops->read(node, buffer, size, bytes_read);
}

kerr_t vfs_write(vfs_node_t* node, const void* buffer, size_t size, size_t* bytes_written) {
    if (!node || !node->ops || !node->ops->write) return E_INVALID;
    return node->ops->write(node, buffer, size, bytes_written);
}

kerr_t vfs_create_file(const char* path) {
    if(!path) return E_INVALID;
    char* dir_path = vfs_dirname(path);
    if(!dir_path) return E_INVALID;
    vfs_node_t* parent = vfs_resolve_path(dir_path);

    if (!parent) return E_NOTFOUND;
    if (!parent->ops) return E_NOTFOUND;
    if (!parent->ops->create) return E_NOTDIR;

    const char* filename = vfs_basename(path);
    if(!filename) return E_INVALID;
    vfs_node_t* new_file = NULL;

    return parent->ops->create(parent, filename, FILE_TYPE_REGULAR, &new_file);
}

kerr_t vfs_create_directory(const char* path) {
    if(!path) return E_INVALID;
    char* dir_path = vfs_dirname(path);
    if(!dir_path) return E_INVALID;
    vfs_node_t* parent = vfs_resolve_path(dir_path);

    if (!parent || !parent->ops || !parent->ops->create) return E_INVALID;

    const char* dirname = vfs_basename(path);
    if(!dirname) return E_INVALID;
    vfs_node_t* new_dir = NULL;

    return parent->ops->create(parent, dirname, FILE_TYPE_DIRECTORY, &new_dir);
}

kerr_t vfs_delete(const char* path) {
    if(!path) return E_INVALID;
    vfs_node_t* node = vfs_resolve_path(path);
    if (!node || !node->ops || !node->ops->delete) return E_INVALID;
    return node->ops->delete(node);
}

kerr_t vfs_list(const char* path) {
    if(!path) return E_INVALID;
    vfs_node_t* dir = vfs_resolve_path(path);

    if (!dir) return E_NOTFOUND;
    if (dir->type != FILE_TYPE_DIRECTORY) return E_NOTDIR;
    if (!dir->ops || !dir->ops->readdir) return E_INVALID;

    uint32_t index = 0;
    vfs_node_t* child = NULL;

    while (dir->ops->readdir(dir, index, &child) == E_OK) {
        if (child) {
            // printf для вывода информации о файле/директории
            printf("%s", child->name);
            if (child->type == FILE_TYPE_DIRECTORY) {
                printf("/\n");
            } else {
                printf(" %zu bytes\n", child->size);
            }
        }
        index++;
    }
    return E_OK;
}

void vfs_print_tree(vfs_node_t* node, int depth) {
    if (!node) node = vfs_root;
    if (!node) return;

    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    printf("%s", node->name);
    if (node->type == FILE_TYPE_DIRECTORY) {
        printf("/\n");
    } else {
        printf(" (%zu bytes)\n", node->size);
    }

    if (node->type == FILE_TYPE_DIRECTORY && node->ops && node->ops->readdir) {
        uint32_t index = 0;
        vfs_node_t* child = NULL;

        while (node->ops->readdir(node, index, &child) == E_OK) {
            if (child) {
                vfs_print_tree(child, depth + 1);
            }
            index++;
        }
    }
}

kerr_t vfs_copy_file(const char* dest, const char* source) {
    if(!dest || !source) return E_INVALID;
    vfs_node_t* src_node = vfs_open(source);
    if (!src_node || src_node->type != FILE_TYPE_REGULAR) return E_INVALID;

    // Читаем содержимое исходного файла
    char* buffer = (char*)get_free_page(); // Используем вашу функцию выделения памяти
    if(!buffer) return E_NOMEM;

    size_t bytes_read = 0;
    kerr_t err = vfs_read(src_node, buffer, src_node->size, &bytes_read);

    if(err != E_OK) {
        free_page((paddr_t)buffer); // Освобождаем память при ошибке
        return err;
    }

    // Создаем целевой файл
    err = vfs_create_file(dest);
    if (err != E_OK && err != E_EXISTS) {
        free_page((paddr_t)buffer);
        return err;
    }

    vfs_node_t* dst_node = vfs_open(dest);
    if (!dst_node) {
        free_page((paddr_t)buffer);
        return E_NOTFOUND;
    }

    // Записываем содержимое в целевой файл
    size_t bytes_written = 0;
    err = vfs_write(dst_node, buffer, bytes_read, &bytes_written);

    free_page((paddr_t)buffer); // Всегда освобождаем буфер

    if(err != E_OK) {
        return err;
    }

    // Устанавливаем размер целевого файла
    dst_node->size = bytes_written;

    return E_OK;
}

