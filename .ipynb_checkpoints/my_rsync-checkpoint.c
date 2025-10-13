#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 4096

void copy_file_content(const char *src_path, const char *dest_path);
void copy_file_attributes(const char *src_path, const char *dest_path);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_file> <destination_file>\n", argv[0]);
        exit(1);
    }

    const char *src_path = argv[1];
    const char *dest_path = argv[2];

    struct stat src_stat, dest_stat;

    // 1. 获取源文件的属性
    if (stat(src_path, &src_stat) < 0) {
        perror("Failed to stat source file");
        exit(1);
    }

    int dest_exists = 1;
    // 2. 尝试获取目标文件的属性，如果文件不存在则会失败
    if (stat(dest_path, &dest_stat) < 0) {
        if (errno == ENOENT) {
            // 目标文件不存在
            dest_exists = 0;
        } else {
            perror("Failed to stat destination file");
            exit(1);
        }
    }

    // 3. 比较文件属性，判断是否需要同步
    // 如果目标文件不存在，或者修改时间或大小不一致，则进行同步
    if (!dest_exists || src_stat.st_mtime != dest_stat.st_mtime || src_stat.st_size != dest_stat.st_size||
    src_stat.st_mode != dest_stat.st_mode) {
        printf("Files differ. Starting synchronization...\n");

        // 3.1. 同步文件内容
        copy_file_content(src_path, dest_path);

        // 3.2. 同步文件属性
        copy_file_attributes(src_path, dest_path);

        printf("Synchronization complete.\n");
    } else {
        printf("Files are already in sync.\n");
    }

    return 0;
}

// 函数：复制文件内容
void copy_file_content(const char *src_path, const char *dest_path) {
    int src_fd, dest_fd;
    ssize_t n_read;
    char buffer[BUFFER_SIZE];

    // 打开源文件
    if ((src_fd = open(src_path, O_RDONLY)) < 0) {
        perror("Failed to open source file for reading");
        exit(1);
    }

    // 打开（或创建）目标文件
    // 0644 是一个常见的文件权限设置
    if ((dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("Failed to open destination file for writing");
        close(src_fd);
        exit(1);
    }

    // 循环读写，完成复制
    while ((n_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dest_fd, buffer, n_read) != n_read) {
            perror("Error writing to destination file");
            close(src_fd);
            close(dest_fd);
            exit(1);
        }
    }

    if (n_read < 0) {
        perror("Error reading from source file");
    }

    close(src_fd);
    close(dest_fd);
}

// 函数：复制文件属性
void copy_file_attributes(const char *src_path, const char *dest_path) {
    struct stat src_stat;

    if (stat(src_path, &src_stat) < 0) {
        perror("Failed to get source file stats for attribute copy");
        return;
    }

    // 1. 复制权限
    if (chmod(dest_path, src_stat.st_mode) < 0) {
        perror("Failed to change mode of destination file");
    }

    // 2. 复制所有权 (注意：只有超级用户才能成功执行)
    // 在普通用户权限下，这个调用通常会失败，可以忽略这个错误
    if (chown(dest_path, src_stat.st_uid, src_stat.st_gid) < 0) {
        if (errno != EPERM) { // EPERM 是权限不足的错误，可以忽略
            perror("Failed to change ownership of destination file");
        }
    }

    // 3. 复制时间戳
    struct utimbuf times;
    times.actime = src_stat.st_atime; // 访问时间
    times.modtime = src_stat.st_mtime; // 修改时间
    if (utime(dest_path, &times) < 0) {
        perror("Failed to change times of destination file");
    }
}