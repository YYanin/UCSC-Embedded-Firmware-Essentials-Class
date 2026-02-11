/**
 * @file vfs_esp32.c
 * @brief Virtual Filesystem Implementation for ESP32
 * 
 * This file implements the VFS abstraction layer for ESP32 using
 * ESP-IDF's VFS component. ESP-IDF already provides a POSIX-like
 * layer on top of SPIFFS/LittleFS, so most operations are simple wrappers.
 * 
 * The filesystem is mounted at /spiffs on startup.
 * 
 * This file is only compiled when building for ESP32 (ESP_PLATFORM defined).
 */

#ifdef ESP_PLATFORM  /* Only compile on ESP32 */

#include "shell_vfs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_log.h"
#include "esp_spiffs.h"

/* Tag for ESP logging */
static const char *TAG = "vfs";

/* Internal path buffer size - larger to avoid truncation warnings */
#define PATH_BUF_SIZE  512

/* Current working directory - stored locally since ESP32 doesn't have true cwd */
static char g_cwd[VFS_MAX_PATH] = VFS_SPIFFS_MOUNT;

/* Flag to track if VFS is initialized */
static int g_vfs_initialized = 0;

/**
 * @brief Build full path from relative or absolute path
 * 
 * @param path Input path (relative or absolute)
 * @param buf Output buffer (should be PATH_BUF_SIZE bytes)
 * @param size Size of output buffer
 * @return 0 on success, -1 on error (path too long)
 */
static int build_full_path(const char *path, char *buf, size_t size) {
    if (path == NULL || buf == NULL || size == 0) {
        return -1;
    }
    
    int len;
    if (path[0] == '/') {
        /* Already absolute */
        len = snprintf(buf, size, "%s", path);
    } else {
        /* Relative to cwd */
        len = snprintf(buf, size, "%s/%s", g_cwd, path);
    }
    
    /* Check for truncation */
    if (len < 0 || (size_t)len >= size) {
        buf[0] = '\0';
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * VFS Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the VFS layer for ESP32
 * 
 * Mounts SPIFFS filesystem at /spiffs. If SPIFFS is not formatted,
 * it will be formatted automatically.
 * 
 * @return 0 on success, -1 on error
 */
int vfs_init(void) {
    if (g_vfs_initialized) {
        ESP_LOGW(TAG, "VFS already initialized");
        return 0;
    }
    
    ESP_LOGI(TAG, "Initializing SPIFFS VFS");
    
    /* Configure SPIFFS */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = VFS_SPIFFS_MOUNT,     /* Mount point */
        .partition_label = NULL,            /* Use default partition */
        .max_files = 5,                     /* Max open files */
        .format_if_mount_failed = true      /* Auto-format if needed */
    };
    
    /* Mount SPIFFS */
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS partition not found");
        } else {
            ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
        }
        return -1;
    }
    
    /* Log SPIFFS info */
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: Total: %d bytes, Used: %d bytes", total, used);
    }
    
    /* Set initial working directory to mount point */
    strncpy(g_cwd, VFS_SPIFFS_MOUNT, VFS_MAX_PATH - 1);
    g_cwd[VFS_MAX_PATH - 1] = '\0';
    
    g_vfs_initialized = 1;
    ESP_LOGI(TAG, "VFS initialized successfully");
    
    return 0;
}

/**
 * @brief Clean up VFS resources
 * 
 * Unmounts SPIFFS filesystem.
 */
void vfs_cleanup(void) {
    if (!g_vfs_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Cleaning up VFS");
    esp_vfs_spiffs_unregister(NULL);
    g_vfs_initialized = 0;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Open a file
 * 
 * Wraps fopen(). Paths are relative to current working directory
 * unless they start with /.
 */
vfs_file_t vfs_open(const char *path, const char *mode) {
    char fullpath[PATH_BUF_SIZE];
    
    if (path == NULL || mode == NULL) {
        return NULL;
    }
    
    /* Build absolute path using helper */
    if (build_full_path(path, fullpath, sizeof(fullpath)) != 0) {
        ESP_LOGD(TAG, "Path too long: %s", path);
        return NULL;
    }
    
    ESP_LOGD(TAG, "Opening file: %s mode=%s", fullpath, mode);
    
    FILE *f = fopen(fullpath, mode);
    if (f == NULL) {
        ESP_LOGD(TAG, "Failed to open %s: %s", fullpath, strerror(errno));
    }
    
    return (vfs_file_t)f;
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
 * 
 * Note: SPIFFS doesn't support true directories, but ESP-IDF VFS
 * simulates them. LittleFS supports real directories.
 */
int vfs_mkdir(const char *path) {
    char fullpath[PATH_BUF_SIZE];
    
    if (path == NULL) {
        return -1;
    }
    
    /* Build absolute path using helper */
    if (build_full_path(path, fullpath, sizeof(fullpath)) != 0) {
        return -1;
    }
    
    /* Create directory with default permissions */
    return mkdir(fullpath, 0755) == 0 ? 0 : -1;
}

/**
 * @brief Remove an empty directory
 */
int vfs_rmdir(const char *path) {
    char fullpath[PATH_BUF_SIZE];
    
    if (path == NULL) {
        return -1;
    }
    
    /* Build absolute path using helper */
    if (build_full_path(path, fullpath, sizeof(fullpath)) != 0) {
        return -1;
    }
    
    return rmdir(fullpath) == 0 ? 0 : -1;
}

/**
 * @brief Open a directory for reading
 */
vfs_dir_t vfs_opendir(const char *path) {
    char fullpath[PATH_BUF_SIZE];
    
    if (path == NULL) {
        return NULL;
    }
    
    /* Build absolute path using helper */
    if (build_full_path(path, fullpath, sizeof(fullpath)) != 0) {
        return NULL;
    }
    
    ESP_LOGD(TAG, "Opening directory: %s", fullpath);
    
    DIR *d = opendir(fullpath);
    if (d == NULL) {
        ESP_LOGD(TAG, "Failed to open directory %s: %s", fullpath, strerror(errno));
    }
    
    return (vfs_dir_t)d;
}

/**
 * @brief Read next entry from directory
 * 
 * @return 0 on success with entry filled, -1 on end or error
 */
int vfs_readdir(vfs_dir_t dir, vfs_dirent_t *entry) {
    if (dir == NULL || entry == NULL) {
        return -1;
    }
    
    struct dirent *de = readdir((DIR *)dir);
    if (de == NULL) {
        return -1;  /* End of directory or error */
    }
    
    /* Fill in the entry structure */
    strncpy(entry->name, de->d_name, VFS_MAX_FILENAME - 1);
    entry->name[VFS_MAX_FILENAME - 1] = '\0';
    
    /* SPIFFS doesn't have real directories, d_type may not be reliable */
    /* We'll use stat to determine if it's a directory */
    entry->is_dir = (de->d_type == DT_DIR) ? 1 : 0;
    
    /* Get file size using stat */
    entry->size = 0;
    entry->mtime = 0;
    
    /* Build full path for stat using helper */
    char fullpath[PATH_BUF_SIZE];
    if (build_full_path(entry->name, fullpath, sizeof(fullpath)) == 0) {
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            entry->size = st.st_size;
            entry->mtime = st.st_mtime;
            entry->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
        }
    }
    
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
    char fullpath[PATH_BUF_SIZE];
    
    if (path == NULL) {
        return -1;
    }
    
    /* Build absolute path using helper */
    if (build_full_path(path, fullpath, sizeof(fullpath)) != 0) {
        return -1;
    }
    
    ESP_LOGD(TAG, "Removing file: %s", fullpath);
    
    return remove(fullpath) == 0 ? 0 : -1;
}

/**
 * @brief Rename/move a file
 */
int vfs_rename(const char *oldpath, const char *newpath) {
    char old_full[PATH_BUF_SIZE];
    char new_full[PATH_BUF_SIZE];
    
    if (oldpath == NULL || newpath == NULL) {
        return -1;
    }
    
    /* Build absolute paths using helper */
    if (build_full_path(oldpath, old_full, sizeof(old_full)) != 0) {
        return -1;
    }
    if (build_full_path(newpath, new_full, sizeof(new_full)) != 0) {
        return -1;
    }
    
    ESP_LOGD(TAG, "Renaming %s -> %s", old_full, new_full);
    
    return rename(old_full, new_full) == 0 ? 0 : -1;
}

/**
 * @brief Get file/directory information
 */
int vfs_stat(const char *path, vfs_stat_t *st) {
    char fullpath[PATH_BUF_SIZE];
    struct stat posix_st;
    
    if (path == NULL || st == NULL) {
        return -1;
    }
    
    /* Build absolute path using helper */
    if (build_full_path(path, fullpath, sizeof(fullpath)) != 0) {
        st->exists = 0;
        return -1;
    }
    
    if (stat(fullpath, &posix_st) != 0) {
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
    vfs_stat_t st;
    return (vfs_stat(path, &st) == 0 && st.exists) ? 1 : 0;
}

/* ============================================================================
 * Path Operations
 * ============================================================================ */

/**
 * @brief Get current working directory
 * 
 * On ESP32, we maintain our own cwd since the system doesn't have one.
 */
int vfs_getcwd(char *buf, size_t size) {
    if (buf == NULL || size == 0) {
        return -1;
    }
    
    strncpy(buf, g_cwd, size - 1);
    buf[size - 1] = '\0';
    
    return 0;
}

/**
 * @brief Change current working directory
 * 
 * Validates that the directory exists before changing.
 * Special handling for SPIFFS mount point which can't be stat'd.
 */
int vfs_chdir(const char *path) {
    char newcwd[PATH_BUF_SIZE];
    struct stat st;
    
    if (path == NULL) {
        return -1;
    }
    
    /* Handle special cases first */
    if (strcmp(path, ".") == 0) {
        /* Stay in current directory */
        return 0;
    } else if (strcmp(path, "..") == 0) {
        /* Go up one directory */
        strncpy(newcwd, g_cwd, sizeof(newcwd) - 1);
        newcwd[sizeof(newcwd) - 1] = '\0';
        char *last_slash = strrchr(newcwd, '/');
        if (last_slash != NULL && last_slash != newcwd) {
            *last_slash = '\0';
        } else if (last_slash == newcwd) {
            /* Already at root or mount point, stay there */
            newcwd[1] = '\0';  /* Keep just the leading slash */
        }
    } else {
        /* Build absolute path using helper */
        if (build_full_path(path, newcwd, sizeof(newcwd)) != 0) {
            return -1;
        }
    }
    
    /* Remove trailing slash if present (except for root) */
    size_t len = strlen(newcwd);
    if (len > 1 && newcwd[len - 1] == '/') {
        newcwd[len - 1] = '\0';
    }
    
    /* Special case: SPIFFS mount point can't be stat'd but is valid */
    if (strcmp(newcwd, VFS_SPIFFS_MOUNT) == 0) {
        /* Accept the mount point directly */
        strncpy(g_cwd, newcwd, VFS_MAX_PATH - 1);
        g_cwd[VFS_MAX_PATH - 1] = '\0';
        ESP_LOGD(TAG, "Changed directory to mount point: %s", g_cwd);
        return 0;
    }
    
    /* For paths starting with the mount point, try to verify with opendir */
    if (strncmp(newcwd, VFS_SPIFFS_MOUNT, strlen(VFS_SPIFFS_MOUNT)) == 0) {
        /* SPIFFS doesn't have real directories - for now just accept paths */
        /* that start with the mount point. A more complete solution would */
        /* check if any files exist with this prefix. */
        strncpy(g_cwd, newcwd, VFS_MAX_PATH - 1);
        g_cwd[VFS_MAX_PATH - 1] = '\0';
        ESP_LOGD(TAG, "Changed directory to: %s", g_cwd);
        return 0;
    }
    
    /* For other paths, verify directory exists */
    if (stat(newcwd, &st) != 0) {
        ESP_LOGD(TAG, "chdir failed: %s does not exist", newcwd);
        return -1;
    }
    
    /* Update cwd */
    strncpy(g_cwd, newcwd, VFS_MAX_PATH - 1);
    g_cwd[VFS_MAX_PATH - 1] = '\0';
    
    ESP_LOGD(TAG, "Changed directory to: %s", g_cwd);
    
    return 0;
}

/**
 * @brief Normalize a path (resolve . and ..)
 * 
 * Simple implementation for ESP32 - doesn't fully resolve symlinks
 * since SPIFFS doesn't have them.
 */
int vfs_realpath(const char *path, char *resolved) {
    if (path == NULL || resolved == NULL) {
        return -1;
    }
    
    /* Build absolute path using helper, write to temporary buffer */
    char temp[PATH_BUF_SIZE];
    if (build_full_path(path, temp, sizeof(temp)) != 0) {
        return -1;
    }
    
    /* Copy to output buffer (caller expects VFS_MAX_PATH size) */
    strncpy(resolved, temp, VFS_MAX_PATH - 1);
    resolved[VFS_MAX_PATH - 1] = '\0';
    
    /* TODO: Implement proper . and .. resolution */
    
    return 0;
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
        /* No directory component, use current directory */
        strncpy(dir, ".", size - 1);
    } else if (last_slash == path) {
        /* Root directory */
        strncpy(dir, "/", size - 1);
    } else {
        /* Copy up to but not including the last slash */
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

#endif /* ESP_PLATFORM */
