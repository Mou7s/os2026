#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>   // 提供目录遍历相关 API：DIR*, opendir, readdir, closedir
#include <ctype.h>    // 提供字符类型检查函数，如 isdigit
#include <string.h>
#include <unistd.h>   // 提供系统调用，如 getpid(), getppid()

/**
 * read_comm: 读取指定 PID 的进程名称 (Command Name)
 * 
 * 原理：
 * Linux 在 procfs 中为每个进程维护了一个短文件名：/proc/[pid]/comm
 * 该文件只包含该进程的可执行文件名（最长 16 字节），末尾带有换行符 '\n'。
 * 
 * 返回值：0 表示成功读取，-1 表示打开或读取失败
 */
static int read_comm(pid_t pid, char *buf, size_t n) {
    char path[64];
    // 拼接目标路径：/proc/<pid>/comm
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    // 从文件中读入一行
    if (!fgets(buf, (int)n, f)) { 
        fclose(f); 
        return -1; 
    }

    // strcspn(buf, "\n") 返回 buf 中第一个 '\n' 的下标，将其替换为字符串结束符 '\0'
    buf[strcspn(buf, "\n")] = 0;
    fclose(f);
    return 0;
}

/**
 * get_ppid_from_stat: 从 /proc/[pid]/stat 文件中提取父进程 ID (PPID)
 * 
 * 原理：
 * /proc/[pid]/stat 包含了该进程的大量内核状态信息，所有字段用空格分隔。
 * 字段格式如下：
 *   pid (comm) state ppid pgrp session tty_nr ...
 * 例如：
 *   1 (systemd) S 0 1 1 0 -1 4194560 ...
 * 
 * 注意细节：
 * 进程名本身可能包含空格或括号，但内核会用一对小括号将进程名包裹起来 "(comm)"。
 * 因此格式化解析时使用 "%d (%255[^)]) %c %d"：
 *   - %d: 读取第 1 个字段 pid
 *   - (%255[^)]): 读取括号内的所有字符（直到遇到右括号 ')' 为止）作为 comm
 *   - %c: 读取第 3 个字段进程状态 state (如 'S' 睡眠, 'R' 运行)
 *   - %d: 读取第 4 个字段父进程 ID ppid
 * 
 * 返回值：0 表示解析成功并存入 ppid_out，-1 表示失败
 */
static int get_ppid_from_stat(pid_t pid, pid_t *ppid_out) {
    char path[64], line[4096];
    // 拼接目标路径：/proc/<pid>/stat
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(line, sizeof(line), f)) { 
        fclose(f); 
        return -1; 
    }
    fclose(f);

    int id, ppid;
    char comm[256], state;
    // 解析 stat 字符串中的前 4 个字段
    if (sscanf(line, "%d (%255[^)]) %c %d", &id, comm, &state, &ppid) != 4) return -1;
    *ppid_out = (pid_t)ppid;
    return 0;
}

int main(void) {
    // 1. 获取当前程序自身的 PID 和父进程的 PPID
    pid_t self = getpid();
    pid_t parent = getppid();

    // 2. 读取自身与父进程的名称
    char self_comm[256] = "?", parent_comm[256] = "?";
    read_comm(self, self_comm, sizeof self_comm);
    read_comm(parent, parent_comm, sizeof parent_comm);

    // 打印父进程节点（例如终端 shell：bash(1234)）
    printf("%s(%d)\n", parent_comm, parent);

    // 3. 打开 /proc 虚拟文件系统目录
    // Linux 下 /proc 中包含了系统所有正在运行的进程，以及 cpuinfo、meminfo 等
    DIR *d = opendir("/proc");
    if (!d) { 
        perror("opendir /proc"); 
        return 1; 
    }

    // 4. 循环遍历 /proc 目录下的所有文件/子目录
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        // 门卫检查：判断目录名的首字符是否为数字
        // 只有以纯数字命名的文件夹（如 "1", "1024"）才代表一个进程
        // 其余诸如 "cpuinfo", "sys", "." 等全部过滤跳过
        if (!isdigit((unsigned char)de->d_name[0])) continue;

        // 将目录名字符串转为整数 PID
        pid_t pid = (pid_t)atoi(de->d_name);

        // 获取该进程的父进程 PPID
        pid_t ppid;
        if (get_ppid_from_stat(pid, &ppid) != 0) continue;

        // 官方示例此处仅筛选了父进程与当前程序的父进程相同的“兄弟进程”
        // （后续我们的完整 pstree 需要建立全系统的完整进程树）
        if (ppid != parent) continue;

        char comm[256] = "?";
        read_comm(pid, comm, sizeof comm);

        // 打印出子进程条目，并在当前进程后标记 "<== me"
        printf("  |- %s(%d)%s\n", comm, pid, (pid == self) ? "  <== me" : "");
    }

    // 5. 关闭目录流，释放系统资源
    closedir(d);
    return 0;
}
