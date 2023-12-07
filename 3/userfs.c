#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
    int delete;
};

/** List of all files. */
static struct file *file_list = NULL;

struct file * find_file(const char * filename){
    struct file * ptr = file_list;
    while(ptr != NULL){
        if(!ptr->delete &&  strcmp(filename, ptr->name) == 0){
            ptr->refs++;
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

struct file * create_file(const char * filename){
    struct file * res = malloc(sizeof (struct file));
    res->block_list = res->last_block = NULL;
    res->refs = 1;
    res->name = strdup(filename);
    res->prev = NULL;
    res->delete = 0;
    res->next = file_list;
    if(file_list != NULL) file_list->prev = res;
    file_list = res;
    return res;
}


struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
    size_t pos;
    int is_free;
    int flag;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;


int is_valid_file(int fd){
    if (fd >= 0  && fd < file_descriptor_capacity) {
        if(file_descriptors[fd]->file == NULL){
            return 1;
        }
    }
    return fd < 0 || fd >= file_descriptor_capacity || file_descriptors[fd]->is_free;
}

int find_free_fd() {
    if(file_descriptor_count < file_descriptor_capacity)
        for(int i = 0; i < file_descriptor_capacity; i++)
            if(file_descriptors[i]->is_free) {
                ++file_descriptor_count;
                file_descriptors[i]->is_free=0;
                return i;
            }
    if(file_descriptors == NULL)
        file_descriptors = malloc(sizeof (struct filedesc *) * (++file_descriptor_capacity));
    else
        file_descriptors = realloc(file_descriptors, sizeof (struct filedesc *) * (++file_descriptor_capacity));
    file_descriptors[file_descriptor_count] = malloc(sizeof (struct filedesc));
    file_descriptors[file_descriptor_count]->file = NULL;
    file_descriptors[file_descriptor_count]->is_free = 0;
    file_descriptors[file_descriptor_count]->flag = 0;
    return file_descriptor_count++;
}

void close_file(struct file *f){
    if(--f->refs == 0 && f->delete){
        struct block *p = f->block_list;
        struct block *nx;
        while(p != NULL){
            nx = p->next;
            p->occupied = 0;
            free(p->memory);
            free(p);
            p = nx;
        }
        if(f->name != NULL) free(f->name);
        if(f == file_list) file_list = f->next;
        if(f->prev != NULL) f->prev->next = f->next;
        if(f->next != NULL) f->next->prev = f->prev;
        free(f);
    }
}

void filedesc_make_free(int fd){
    if(file_descriptors[fd]->file != NULL) close_file(file_descriptors[fd]->file);
    file_descriptors[fd]->file=NULL;
    file_descriptors[fd]->is_free = 1;
    file_descriptors[fd]->flag = 0;
    file_descriptor_count--;

}

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_open(const char *filename, int flags)
{
	/* IMPLEMENT THIS FUNCTION */
    int fd = find_free_fd();
    struct filedesc *f = file_descriptors[fd];
    f->flag = flags;
    if(flags & UFS_CREATE) {
        ufs_delete(filename);
        f->file = create_file(filename);
    } else {
        f->file = find_file(filename);
    }
    if(f->file == NULL){
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    f->pos = 0;
    return fd;
}

struct block * new_block(struct file *f){
    struct block * ptr;
    ptr = malloc(sizeof(struct block));
    ptr->occupied = 0;
    ptr->memory = malloc(BLOCK_SIZE);
    memset(ptr->memory, 0, BLOCK_SIZE);
    ptr->next = NULL;
    if(f->block_list == NULL){
        f->block_list = ptr;
    }
    if(f->last_block != NULL){
        f->last_block->next = ptr;
        ptr->prev = f->last_block;
    }
    f->last_block = ptr;
    return ptr;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
    if(is_valid_file(fd)){
        ufs_error_code= UFS_ERR_NO_FILE;
        return -1;
    }
    struct filedesc *f = file_descriptors[fd];
    struct block *ptr = f->file->block_list;

    if(f->flag & UFS_READ_ONLY){
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }

    if(f->pos + size > MAX_FILE_SIZE){
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }
    size_t _size = 0, temp = 0;
    for(size_t i = 0; i < f->pos / BLOCK_SIZE; i++){
        ptr = ptr->next;
    }

    _size = 0;
    while(_size < size){
        if(ptr == NULL){
            ptr = new_block(f->file);
        }
        temp = BLOCK_SIZE - f->pos % BLOCK_SIZE;
        if(temp > size - _size) temp = size-_size;
        memcpy(ptr->memory+f->pos%BLOCK_SIZE, buf+_size, temp);
        _size += temp;

        if (ptr->occupied < (int)(temp + f->pos % BLOCK_SIZE)) {
            ptr->occupied = (int)(temp + f->pos % BLOCK_SIZE);
        }
        ptr = ptr->next;
        f->pos += temp;
    }
    return (ssize_t) _size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	if(is_valid_file(fd)){
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    struct filedesc *f =file_descriptors[fd];
    struct block *ptr = f->file->block_list;

    if(f->flag & UFS_WRITE_ONLY){
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }

    size_t _size=0, temp=0;
    for(size_t i = 0; i < f->pos / BLOCK_SIZE; i++){
        ptr = ptr->next;
    }
    _size = 0;
    while(_size < size && ptr != NULL){
        temp = ptr->occupied - f->pos % BLOCK_SIZE;
        if(temp > size - _size) temp = size - _size;
        if(temp == 0) break;

        memcpy(buf+_size, ptr->memory + f->pos%BLOCK_SIZE, temp);
        _size += temp;
        ptr = ptr->next;
        f->pos += temp;
    }
    return (ssize_t) _size;
}

int
ufs_close(int fd)
{
	/* IMPLEMENT THIS FUNCTION */
    if(is_valid_file(fd)){
//        printf("ERROR?\n");
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    filedesc_make_free(fd);
    return 0;
}


int
ufs_delete(const char *filename)
{
	/* IMPLEMENT THIS FUNCTION */
	struct file * f = find_file(filename);
    if(f == NULL){
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }
    f->delete = 1;
    close_file(f);
    return 0;
}

void
ufs_destroy(void)
{
    for(int i = 0; i < file_descriptor_capacity; i++)
        free(file_descriptors[i]);
    file_descriptor_capacity = 0;
    free(file_descriptors);
    file_descriptors = NULL;
}


int ufs_resize(int fd, size_t new_size){
    if(is_valid_file(fd)){
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct filedesc *f = file_descriptors[fd];

    if(f->flag & UFS_READ_ONLY){
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }
    if(new_size > MAX_FILE_SIZE){
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }

    struct block *ptr = f->file->block_list;
    struct block *tmp = NULL;
    size_t size = 0;
    while(size < new_size){
        if(ptr == NULL){
            ptr = new_block(f->file);
        }
        if(BLOCK_SIZE > (int)(new_size - size)){
            ptr->occupied = (int)(new_size - size);
            tmp = ptr;
            ptr = ptr->next;
            break;
        }
        size += BLOCK_SIZE;
        ptr->occupied = BLOCK_SIZE;
        tmp = ptr;
        ptr = ptr->next;
    }
    f->file->last_block = tmp;
    tmp->next = NULL;

    while(ptr != NULL){
        tmp = ptr->next;

        free(ptr->memory);
        free(ptr);

        ptr = tmp;

    }
    for(int i = 0; i < file_descriptor_count; i++)
        if(file_descriptors[i]->file != NULL && file_descriptors[i]->file->name != NULL &&  strcmp(file_descriptors[i]->file->name, f->file->name) == 0)
            if(file_descriptors[i]->pos > new_size)
                file_descriptors[i]->pos = new_size;
    return 0;
}