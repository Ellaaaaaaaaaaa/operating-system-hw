#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h> // 多线程编程所需的头文件

#define MAX_THREADS 256 // 假设一个目录下的文件数不会超过这个值

// 用于向线程传递参数的结构体
typedef struct {
    char src_path[1024];
    char dest_path[1024];
} sync_args;

// 函数声明
void sync_directory(const char *src, const char *dest);
void sync_file(const char *src_file, const char *dest_file);
void copy_file(const char *src_file, const char *dest_file);
void *sync_entry(void *args);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法: %s <源目录> <目标目录>\n", argv[0]);
        exit(1);
    }

    const char *source_dir = argv[1];
    const char *dest_dir = argv[2];

    struct stat st;
    if (stat(source_dir, &st) < 0 || !S_ISDIR(st.st_mode)) {
        perror("源路径不是一个有效的目录");
        exit(1);
    }

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
    printf("同步完成.\n");

    return 0;
}
void sync_directory(const char *src, const char *dest) {
    DIR *dirp;
    struct dirent *dp;
    pthread_t threads[MAX_THREADS]; // 存储线程ID的数组
    int thread_count = 0;

    if ((dirp = opendir(src)) == NULL) {
        perror("无法打开源目录");
        return;
    }

    while ((dp = readdir(dirp)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }
        
        // 为线程参数动态分配内存
        sync_args *args = malloc(sizeof(sync_args));
        if (!args) {
            perror("无法为线程参数分配内存");
            continue;
        }
        snprintf(args->src_path, sizeof(args->src_path), "%s/%s", src, dp->d_name);
        snprintf(args->dest_path, sizeof(args->dest_path), "%s/%s", dest, dp->d_name);

        // 创建线程
        if (pthread_create(&threads[thread_count], NULL, sync_entry, args) != 0) {
            perror("创建线程失败");
            free(args); // 创建失败，释放内存
        } else {
            thread_count++;
        }

        if (thread_count >= MAX_THREADS) {
            fprintf(stderr, "达到最大线程数限制\n");
            break;
        }
    }
    closedir(dirp);

    // 等待该目录下所有被创建的线程结束
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
}


void *sync_entry(void *arg) {
    sync_args *args = (sync_args *)arg;
    struct stat st;

    if (stat(args->src_path, &st) < 0) {
        perror("无法获取文件状态");
    } else {
        if (S_ISDIR(st.st_mode)) {
            struct stat dest_st;
            if (stat(args->dest_path, &dest_st) < 0 && errno == ENOENT) {
                mkdir(args->dest_path, st.st_mode);
            }
            sync_directory(args->src_path, args->dest_path); // 递归
        } else if (S_ISREG(st.st_mode)) {
            sync_file(args->src_path, args->dest_path);
        }
    }

    free(args); // 任务完成，释放传递参数的内存
    return NULL;
}

void sync_file(const char *src_file, const char *dest_file) {
    struct stat src_st, dest_st;

    if (stat(src_file, &src_st) < 0) {
        perror("无法获取源文件状态");
        return;
    }

    if (stat(dest_file, &dest_st) < 0) {
        if (errno == ENOENT) {
            printf("创建文件: %s\n", dest_file);
            copy_file(src_file, dest_file);
        } else {
            perror("无法获取目标文件状态");
        }
    } else {
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