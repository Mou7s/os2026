#include <ctype.h>  // 提供字符类型检查函数，如 isdigit
#include <dirent.h> // 提供目录遍历相关 API：DIR*, opendir, readdir, closedir
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // 提供系统调用，如 getpid(), getppid()

#define MAX_PROCS 4096
#define MAX_CHILDREN 256

typedef struct Process {
  pid_t pid;                              // 进程 ID
  pid_t ppid;                             // 父进程 ID
  char name[256];                         // 进程名字
  int child_count;                        // 子进程数量
  struct Process *children[MAX_CHILDREN]; // 指向子进程的指针数组
} Process;
static Process procs[MAX_PROCS];
static int proc_count = 0;

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
  if (!f)
    return -1;

  // 从文件中读入一行
  if (!fgets(buf, (int)n, f)) {
    fclose(f);
    return -1;
  }

  // strcspn(buf, "\n") 返回 buf 中第一个 '\n' 的下标，将其替换为字符串结束符
  // '\0'
  buf[strcspn(buf, "\n")] = 0;
  fclose(f);
  return 0;
}

static int cmp_pid(const void *a, const void *b) {
  const Process *p1 = *(const Process **)a;
  const Process *p2 = *(const Process **)b;
  return (p1->pid > p2->pid) - (p1->pid < p2->pid);
}
static void print_tree(Process *p, int depth, int show_pids, int numeric_sort) {
  if (!p)
    return;

  // 1. 打印缩进
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }

  // 2. 打印进程名（根据 -p 决定是否附带 pid）
  if (show_pids) {
    printf("%s(%d)\n", p->name, p->pid);
  } else {
    printf("%s\n", p->name);
  }

  // 3. 如果指定了 -n，对子进程按 PID 升序排序
  if (numeric_sort && p->child_count > 1) {
    qsort(p->children, p->child_count, sizeof(Process *), cmp_pid);
  }

  // 4. 递归打印每一个孩子
  for (int i = 0; i < p->child_count; i++) {
    print_tree(p->children[i], depth + 1, show_pids, numeric_sort);
  }
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
  if (!f)
    return -1;

  if (!fgets(line, sizeof(line), f)) {
    fclose(f);
    return -1;
  }
  fclose(f);

  int id, ppid;
  char comm[256], state;
  // 解析 stat 字符串中的前 4 个字段
  if (sscanf(line, "%d (%255[^)]) %c %d", &id, comm, &state, &ppid) != 4)
    return -1;
  *ppid_out = (pid_t)ppid;
  return 0;
}

static Process *find_proc(pid_t pid) {
  for (int i = 0; i < proc_count; i++) {
    if (procs[i].pid == pid) {
      return &procs[i];
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  // 1. 重置 getopt 状态（支持单进程多测试用例）
  optind = 1;
#ifdef __APPLE__
  optreset = 1;
#endif

  // 2. 兼容 TestKit 系统测试（TestKit 在 argv[0] 之后额外传入了 "./pstree"）
  if (argc > 1 && strcmp(argv[1], "./pstree") == 0) {
    argc--;
    argv++;
  }

  // 3. 定义长选项表
  static struct option longopts[] = {{"show-pids", no_argument, NULL, 'p'},
                                     {"numeric-sort", no_argument, NULL, 'n'},
                                     {"version", no_argument, NULL, 'V'},
                                     {NULL, 0, NULL, 0}};

  // 4. 选项状态标记
  int show_pids = 0;
  int numeric_sort = 0;
  int opt;
  while ((opt = getopt_long(argc, argv, "pnV", longopts, NULL)) != -1) {
    switch (opt) {
    case 'V':
      printf("pstree 1.0 (OS2026)\n");
      return 0;
    case 'p':
      show_pids = 1;
      break;
    case 'n':
      numeric_sort = 1;
      break;
    default:
      fprintf(stderr, "Usage: pstree [-p|--show-pids] [-n|--numeric-sort] "
                      "[-V|--version]\n");
      return 1;
    }
  }

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
    if (!isdigit((unsigned char)de->d_name[0]))
      continue;
    pid_t pid = (pid_t)atoi(de->d_name);
    pid_t ppid;
    if (get_ppid_from_stat(pid, &ppid) != 0)
      continue;

    // ✅ 改成这样：读出名字，存进 procs 数组
    char comm[256] = "?";
    read_comm(pid, comm, sizeof comm);

    if (proc_count < MAX_PROCS) {
      procs[proc_count].pid = pid;
      procs[proc_count].ppid = ppid;
      strncpy(procs[proc_count].name, comm, sizeof(procs[proc_count].name) - 1);
      procs[proc_count].child_count = 0;
      proc_count++;
    }
  }

  // 5. 关闭目录流，释放系统资源
  closedir(d);

  // 6. 建立父子树形结构
  for (int i = 0; i < proc_count; i++) {
    Process *parent = find_proc(procs[i].ppid);
    // 如果找到了父进程，并且父进程不是自己（避免死循环）
    if (parent && parent != &procs[i]) {
      if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = &procs[i];
      }
    }
  }

  // 7. 打印整棵树：找到所有根节点并开始 DFS
  for (int i = 0; i < proc_count; i++) {
    if (procs[i].ppid == 0 || find_proc(procs[i].ppid) == NULL) {
      print_tree(&procs[i], 0, show_pids, numeric_sort);
    }
  }

  return 0;
}
