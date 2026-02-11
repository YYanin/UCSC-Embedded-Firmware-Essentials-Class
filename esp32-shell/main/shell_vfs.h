/**
 * @file shell_vfs.h
 * @brief Virtual Filesystem Abstraction Layer
 * 
 * This header defines a portable VFS interface that works on both
 * ESP32 (SPIFFS/LittleFS) and Linux (POSIX filesystem). This allows
 * the shell commands to use the same API regardless of platform.
 * 
 * On ESP32, the VFS uses ESP-IDF's VFS component which already provides
 * POSIX-like file operations on top of SPIFFS/LittleFS.
 * 
 * On Linux, the VFS wraps standard POSIX calls.
 */

#ifndef SHELL_VFS_H
#define SHELL_VFS_H

#include <stddef.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Maximum path length supported by the VFS */
#define VFS_MAX_PATH        256

/* Maximum filename length */
#define VFS_MAX_FILENAME    128

/* Default mount point for ESP32 SPIFFS filesystem */
#define VFS_SPIFFS_MOUNT    "/spiffs"

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief VFS file handle
 * 
 * Opaque pointer to a file. On both platforms this wraps a FILE*.
 * Using a typedef allows future changes if needed.
 */
typedef void* vfs_file_t;

/**
 * @brief VFS directory handle
 * 
 * Opaque pointer to an open directory.
 */
typedef void* vfs_dir_t;

/**
 * @brief Directory entry structure
 * 
 * Returned by vfs_readdir() with information about each file/directory.
 */
typedef struct {
    char name[VFS_MAX_FILENAME];  /* Filename (not full path) */
    int is_dir;                   /* 1 if directory, 0 if file */
    size_t size;                  /* File size in bytes (0 for directories) */
    unsigned long mtime;          /* Modification time (seconds since epoch) */
} vfs_dirent_t;

/**
 * @brief File/directory stat information
 * 
 * Returned by vfs_stat() with detailed file information.
 */
typedef struct {
    int exists;                   /* 1 if file exists, 0 if not */
    int is_dir;                   /* 1 if directory, 0 if file */
    size_t size;                  /* File size in bytes */
    unsigned long mtime;          /* Modification time */
    unsigned long atime;          /* Access time */
    unsigned long ctime;          /* Creation time */
} vfs_stat_t;

/* ============================================================================
 * VFS Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the VFS layer
 * 
 * On ESP32: Mounts SPIFFS at /spiffs
 * On Linux: No-op (filesystem always available)
 * 
 * @return 0 on success, -1 on error
 */
int vfs_init(void);

/**
 * @brief Clean up VFS resources
 * 
 * On ESP32: Unmounts SPIFFS
 * On Linux: No-op
 */
void vfs_cleanup(void);

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Open a file
 * 
 * @param path Path to the file
 * @param mode Mode string ("r", "w", "a", "r+", "w+", "a+", "rb", etc.)
 * @return File handle on success, NULL on error
 */
vfs_file_t vfs_open(const char *path, const char *mode);

/**
 * @brief Close a file
 * 
 * @param file File handle from vfs_open()
 * @return 0 on success, -1 on error
 */
int vfs_close(vfs_file_t file);

/**
 * @brief Read from a file
 * 
 * @param buf Buffer to read into
 * @param size Maximum bytes to read
 * @param file File handle
 * @return Number of bytes read, 0 on EOF, -1 on error
 */
size_t vfs_read(void *buf, size_t size, vfs_file_t file);

/**
 * @brief Write to a file
 * 
 * @param buf Buffer to write from
 * @param size Number of bytes to write
 * @param file File handle
 * @return Number of bytes written, -1 on error
 */
size_t vfs_write(const void *buf, size_t size, vfs_file_t file);

/**
 * @brief Seek to a position in a file
 * 
 * @param file File handle
 * @param offset Offset from whence
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return 0 on success, -1 on error
 */
int vfs_seek(vfs_file_t file, long offset, int whence);

/**
 * @brief Get current position in file
 * 
 * @param file File handle
 * @return Current position, or -1 on error
 */
long vfs_tell(vfs_file_t file);

/**
 * @brief Flush file buffers to storage
 * 
 * @param file File handle
 * @return 0 on success, -1 on error
 */
int vfs_flush(vfs_file_t file);

/**
 * @brief Check if at end of file
 * 
 * @param file File handle
 * @return 1 if at EOF, 0 if not
 */
int vfs_eof(vfs_file_t file);

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Create a directory
 * 
 * @param path Path to create
 * @return 0 on success, -1 on error
 */
int vfs_mkdir(const char *path);

/**
 * @brief Remove an empty directory
 * 
 * @param path Path to remove
 * @return 0 on success, -1 on error
 */
int vfs_rmdir(const char *path);

/**
 * @brief Open a directory for reading
 * 
 * @param path Path to directory
 * @return Directory handle on success, NULL on error
 */
vfs_dir_t vfs_opendir(const char *path);

/**
 * @brief Read next entry from directory
 * 
 * @param dir Directory handle from vfs_opendir()
 * @param entry Pointer to dirent structure to fill
 * @return 0 on success, -1 on end of directory or error
 */
int vfs_readdir(vfs_dir_t dir, vfs_dirent_t *entry);

/**
 * @brief Close a directory
 * 
 * @param dir Directory handle
 * @return 0 on success, -1 on error
 */
int vfs_closedir(vfs_dir_t dir);

/* ============================================================================
 * File Management
 * ============================================================================ */

/**
 * @brief Delete a file
 * 
 * @param path Path to file
 * @return 0 on success, -1 on error
 */
int vfs_remove(const char *path);

/**
 * @brief Rename/move a file
 * 
 * @param oldpath Current path
 * @param newpath New path
 * @return 0 on success, -1 on error
 */
int vfs_rename(const char *oldpath, const char *newpath);

/**
 * @brief Get file/directory information
 * 
 * @param path Path to file/directory
 * @param st Pointer to stat structure to fill
 * @return 0 on success, -1 on error
 */
int vfs_stat(const char *path, vfs_stat_t *st);

/**
 * @brief Check if file/directory exists
 * 
 * @param path Path to check
 * @return 1 if exists, 0 if not
 */
int vfs_exists(const char *path);

/* ============================================================================
 * Path Operations
 * ============================================================================ */

/**
 * @brief Get current working directory
 * 
 * @param buf Buffer to store path
 * @param size Size of buffer
 * @return 0 on success, -1 on error
 */
int vfs_getcwd(char *buf, size_t size);

/**
 * @brief Change current working directory
 * 
 * @param path New working directory
 * @return 0 on success, -1 on error
 */
int vfs_chdir(const char *path);

/**
 * @brief Normalize a path (resolve . and .., make absolute)
 * 
 * @param path Input path
 * @param resolved Buffer for resolved path (at least VFS_MAX_PATH bytes)
 * @return 0 on success, -1 on error
 */
int vfs_realpath(const char *path, char *resolved);

/**
 * @brief Get the filename component of a path
 * 
 * @param path Full path
 * @return Pointer to filename part (within the input string)
 */
const char* vfs_basename(const char *path);

/**
 * @brief Get the directory component of a path
 * 
 * @param path Full path
 * @param dir Buffer for directory part
 * @param size Size of buffer
 * @return 0 on success, -1 on error
 */
int vfs_dirname(const char *path, char *dir, size_t size);

#endif /* SHELL_VFS_H */
