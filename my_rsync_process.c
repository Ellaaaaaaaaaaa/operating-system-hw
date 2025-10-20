#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

void sync_directory(const char *src, const char *dest);
void sync_file(const char *src_file, const char *dest_file);
void copy_file(const char *src_file, const char *dest_file);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法: %s <源目录> <目标目录>\n", argv[0]);
        exit(1);
    }

    const char *source_dir = argv[1];
    const char *dest_dir = argv[2];

    struct stat st;
    // 确保源是一个目录
    if (stat(source_dir, &st) < 0 || !S_ISDIR(st.st_mode)) {
        perror("源路径不是一个有效的目录");
        exit(1);
    }

    // 检查目标目录是否存在，如果不存在则创建
    if (stat(dest_dir, &st) < 0) {
        if (mkdir(dest_dir, 0755) < 0) {
            perror("创建目标目录失败");
            exit(1);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "目标路径存在但不是一个目录\n");
        exit(1);
    }

    printf("开始同步: %s -> %s\n", source_dir, dest_dir);
    sync_directory(source_dir, dest_dir);
    
    // 等待所有子进程结束
    while(wait(NULL) > 0);

    printf("同步完成.\n");

    return 0;
}

void sync_directory(const char *src, const char *dest) {
    DIR *dirp;
    struct dirent *dp;
    struct stat st;
    char src_path[1024], dest_path[1024];

    if ((dirp = opendir(src)) == NULL) {
        perror("无法打开源目录");
        return;
    }

    while ((dp = readdir(dirp)) != NULL) {
        // 忽略 "." 和 ".."
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src, dp->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, dp->d_name);

        if (stat(src_path, &st) < 0) {
            perror("无法获取文件状态");
            continue;
        }
        
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork 失败");
            continue;
        } else if (pid == 0) { // 子进程
            if (S_ISDIR(st.st_mode)) {
                // 如果是目录，检查目标目录是否存在，不存在则创建
                struct stat dest_st;
                if (stat(dest_path, &dest_st) < 0) {
                    mkdir(dest_path, st.st_mode);
                }
                sync_directory(src_path, dest_path); // 递归同步
            } else if (S_ISREG(st.st_mode)) {
                // 如果是普通文件，则同步文件
                sync_file(src_path, dest_path);
            }
            exit(0); // 子进程完成任务后必须退出
        }
        // 父进程会继续循环，创建更多子进程
    }

    closedir(dirp);
}

void sync_file(const char *src_file, const char *dest_file) {
    struct stat src_st, dest_st;

    if (stat(src_file, &src_st) < 0) {
        perror("无法获取源文件状态");
        return;
    }

    // 检查目标文件是否存在
    if (stat(dest_file, &dest_st) < 0) {
        if (errno == ENOENT) { // 文件不存在
            printf("创建文件: %s\n", dest_file);
            copy_file(src_file, dest_file);
        } else {
            perror("无法获取目标文件状态");
        }
    } else {
        // 如果源文件的修改时间比目标文件新，则复制
        if (src_st.st_mtime > dest_st.st_mtime) {
            printf("更新文件: %s\n", dest_file);
            copy_file(src_file, dest_file);
        }
    }
}

void copy_file(const char *src_file, const char *dest_file) {
    int src_fd, dest_fd;
    char buffer[4096];
    ssize_t n_read;

    src_fd = open(src_file, O_RDONLY);
    if (src_fd < 0) {
        perror("无法打开源文件");
        return;
    }

    dest_fd = open(dest_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        perror("无法创建目标文件");
        close(src_fd);
        return;
    }

    while ((n_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dest_fd, buffer, n_read) != n_read) {
            perror("写入目标文件时出错");
            break;
        }
    }

    close(src_fd);
    close(dest_fd);
}