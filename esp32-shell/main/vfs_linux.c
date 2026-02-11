/**
 * @file vfs_linux.c
 * @brief Virtual Filesystem Implementation for Linux
 * 
 * This file implements the VFS abstraction layer for Linux using
 * standard POSIX calls. This allows the shell to be tested on
 * a desktop before deploying to ESP32.
 * 
 * This file is only compiled when NOT building for ESP32.
 */

#ifndef ESP_PLATFORM  /* Only compile on Linux/desktop */

#include "shell_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

/* Current working directory buffer */
static char g_cwd[VFS_MAX_PATH];

/* Flag to track if VFS is initialized */
static int g_vfs_initialized = 0;

/* ============================================================================
 * VFS Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the VFS layer for Linux
 * 
 * On Linux, filesystem is always available. We just get the
 * current working directory.
 * 
 * @return 0 on success, -1 on error
 */
int vfs_init(void) {
    if (g_vfs_initialized) {
        return 0;
    }
    
    /* Get current working directory */
    if (getcwd(g_cwd, VFS_MAX_PATH) == NULL) {
        strncpy(g_cwd, "/", VFS_MAX_PATH);
    }
    
    g_vfs_initialized = 1;
    return 0;
}

/**
 * @brief Clean up VFS resources
 * 
 * On Linux, nothing to clean up.
 */
void vfs_cleanup(void) {
    g_vfs_initialized = 0;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Open a file
 */
vfs_file_t vfs_open(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) {
        return NULL;
    }
    
    return (vfs_file_t)fopen(path, mode);
}

/**
 * @brief Close a file
 */
int vfs_close(vfs_file_t file) {
    if (file == NULL) {
        return -1;
    }
    return fclose((FILE *)file) == 0 ? 0 : -1;
}

/**
 * @brief Read from a file
 */
size_t vfs_read(void *buf, size_t size, vfs_file_t file) {
    if (buf == NULL || file == NULL) {
        return 0;
    }
    return fread(buf, 1, size, (FILE *)file);
}

/**
 * @brief Write to a file
 */
size_t vfs_write(const void *buf, size_t size, vfs_file_t file) {
    if (buf == NULL || file == NULL) {
        return 0;
    }
    return fwrite(buf, 1, size, (FILE *)file);
}

/**
 * @brief Seek to a position in a file
 */
int vfs_seek(vfs_file_t file, long offset, int whence) {
    if (file == NULL) {
        return -1;
    }
    return fseek((FILE *)file, offset, whence) == 0 ? 0 : -1;
}

/**
 * @brief Get current position in file
 */
long vfs_tell(vfs_file_t file) {
    if (file == NULL) {
        return -1;
    }
    return ftell((FILE *)file);
}

/**
 * @brief Flush file buffers to storage
 */
int vfs_flush(vfs_file_t file) {
    if (file == NULL) {
        return -1;
    }
    return fflush((FILE *)file) == 0 ? 0 : -1;
}

/**
 * @brief Check if at end of file
 */
int vfs_eof(vfs_file_t file) {
    if (file == NULL) {
        return 1;
    }
    return feof((FILE *)file) != 0 ? 1 : 0;
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Create a directory
 */
int vfs_mkdir(const char *path) {
    if (path == NULL) {
        return -1;
    }
    return mkdir(path, 0755) == 0 ? 0 : -1;
}

/**
 * @brief Remove an empty directory
 */
int vfs_rmdir(const char *path) {
    if (path == NULL) {
        return -1;
    }
    return rmdir(path) == 0 ? 0 : -1;
}

/**
 * @brief Open a directory for reading
 */
vfs_dir_t vfs_opendir(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    return (vfs_dir_t)opendir(path);
}

/**
 * @brief Read next entry from directory
 */
int vfs_readdir(vfs_dir_t dir, vfs_dirent_t *entry) {
    if (dir == NULL || entry == NULL) {
        return -1;
    }
    
    struct dirent *de = readdir((DIR *)dir);
    if (de == NULL) {
        return -1;
    }
    
    /* Fill in the entry structure */
    strncpy(entry->name, de->d_name, VFS_MAX_FILENAME - 1);
    entry->name[VFS_MAX_FILENAME - 1] = '\0';
    
    entry->is_dir = (de->d_type == DT_DIR) ? 1 : 0;
    entry->size = 0;
    entry->mtime = 0;
    
    /* Get file size using stat - need full path */
    /* Note: This is simplified and may not work for all cases */
    /* A production version would track the directory path */
    
    return 0;
}

/**
 * @brief Close a directory
 */
int vfs_closedir(vfs_dir_t dir) {
    if (dir == NULL) {
        return -1;
    }
    return closedir((DIR *)dir) == 0 ? 0 : -1;
}

/* ============================================================================
 * File Management
 * ============================================================================ */

/**
 * @brief Delete a file
 */
int vfs_remove(const char *path) {
    if (path == NULL) {
        return -1;
    }
    return remove(path) == 0 ? 0 : -1;
}

/**
 * @brief Rename/move a file
 */
int vfs_rename(const char *oldpath, const char *newpath) {
    if (oldpath == NULL || newpath == NULL) {
        return -1;
    }
    return rename(oldpath, newpath) == 0 ? 0 : -1;
}

/**
 * @brief Get file/directory information
 */
int vfs_stat(const char *path, vfs_stat_t *st) {
    struct stat posix_st;
    
    if (path == NULL || st == NULL) {
        return -1;
    }
    
    if (stat(path, &posix_st) != 0) {
        st->exists = 0;
        return -1;
    }
    
    st->exists = 1;
    st->is_dir = S_ISDIR(posix_st.st_mode) ? 1 : 0;
    st->size = posix_st.st_size;
    st->mtime = posix_st.st_mtime;
    st->atime = posix_st.st_atime;
    st->ctime = posix_st.st_ctime;
    
    return 0;
}

/**
 * @brief Check if file/directory exists
 */
int vfs_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

/* ============================================================================
 * Path Operations
 * ============================================================================ */

/**
 * @brief Get current working directory
 */
int vfs_getcwd(char *buf, size_t size) {
    if (buf == NULL || size == 0) {
        return -1;
    }
    
    if (getcwd(buf, size) == NULL) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Change current working directory
 */
int vfs_chdir(const char *path) {
    if (path == NULL) {
        return -1;
    }
    return chdir(path) == 0 ? 0 : -1;
}

/**
 * @brief Normalize a path
 */
int vfs_realpath(const char *path, char *resolved) {
    if (path == NULL || resolved == NULL) {
        return -1;
    }
    
    char *result = realpath(path, resolved);
    return (result != NULL) ? 0 : -1;
}

/**
 * @brief Get the filename component of a path
 */
const char* vfs_basename(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        return last_slash + 1;
    }
    
    return path;
}

/**
 * @brief Get the directory component of a path
 */
int vfs_dirname(const char *path, char *dir, size_t size) {
    if (path == NULL || dir == NULL || size == 0) {
        return -1;
    }
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        strncpy(dir, ".", size - 1);
    } else if (last_slash == path) {
        strncpy(dir, "/", size - 1);
    } else {
        size_t len = last_slash - path;
        if (len >= size) {
            len = size - 1;
        }
        strncpy(dir, path, len);
        dir[len] = '\0';
        return 0;
    }
    
    dir[size - 1] = '\0';
    return 0;
}

#endif /* !ESP_PLATFORM */
