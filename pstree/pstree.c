#include <ctype.h>  // 字符类型判断：isdigit
#include <dirent.h> // 目录遍历接口：DIR*, opendir, readdir, closedir
#include <getopt.h> // 命令行长短选项解析：struct option, getopt_long
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // 系统调用与标准类型：pid_t

/* =========================================================================
 * 架构设计与数据结构定义
 * =========================================================================
 *
 * pstree 程序的执行流程分为四大阶段：
 *   1. 【命令行解析】：解析 -p(显示PID), -n(按PID排序), -V(版本信息) 等选项
 *   2. 【内核状态扫描】：遍历 /proc 虚拟文件系统，读取各进程的名称与父进程PPID
 *   3. 【多叉树构建】：利用指针数组，将全系统散落的进程根据父子关系串联成树
 *   4. 【深度优先遍历】：找到根进程（如 systemd），以 DFS 递归打印层级缩进
 * ========================================================================= */

#define MAX_PROCS 4096    // 系统最大支持记录的进程数上限
#define MAX_CHILDREN 256  // 单个进程支持记录的直接子进程数上限

/**
 * 进程节点数据结构 (多叉树节点)
 *
 * 采用“子节点指针数组”方案建模多叉树：
 *   - pid / ppid / name 记录自身的基础身份属性
 *   - children 数组存储指向所有直接子进程节点的内存地址（指针）
 *   - 这种结构在结合标准库 qsort 进行子节点排序时极其简洁高效
 */
typedef struct Process {
  pid_t pid;                              // 自身进程号 (Process ID)
  pid_t ppid;                             // 父进程号 (Parent Process ID)
  char name[256];                         // 进程名 (Command Name)
  int child_count;                        // 当前已记录的直接子进程数量
  struct Process *children[MAX_CHILDREN]; // 指向各子进程节点的指针数组
} Process;

// 全局静态进程池与计数器
static Process procs[MAX_PROCS];
static int proc_count = 0;

// procfs 根目录路径（默认指向 Linux 内核虚拟文件系统 /proc）
static const char *proc_path = "/proc";

/* =========================================================================
 * 阶段一：procfs 虚拟文件系统解析辅助函数
 * ========================================================================= */

/**
 * read_comm: 读取指定进程的可执行文件名
 *
 * 原理：
 * Linux 内核在 /proc/[pid]/comm 文件中维护了该进程的命令名（最长 16 字节）。
 * 该文件为纯文本，末尾带有换行符 '\n'。
 *
 * 返回值：0 表示成功读取，-1 表示文件不存在或读取失败
 */
static int read_comm(pid_t pid, char *buf, size_t n) {
  char path[128];
  snprintf(path, sizeof(path), "%s/%d/comm", proc_path, pid);

  FILE *f = fopen(path, "r");
  if (!f)
    return -1;

  if (!fgets(buf, (int)n, f)) {
    fclose(f);
    return -1;
  }

  // 剥离末尾换行符：strcspn 找到首个 '\n' 下标并将其覆盖为 '\0'
  buf[strcspn(buf, "\n")] = '\0';
  fclose(f);
  return 0;
}

/**
 * get_ppid_from_stat: 从进程状态文件提取父进程 ID (PPID)
 *
 * 原理：
 * /proc/[pid]/stat 包含进程运行时的详尽内核指标，各字段以空格分隔：
 *   格式：pid (comm) state ppid pgrp session ...
 *   例如：1 (systemd) S 0 1 1 ...
 *
 * 边界防护：
 * 进程名本身可能包含空格或括号（如 "Web Content"），若直接按空格分割会导致字段错位。
 * 因此格式化字符串使用 "%d (%255[^)]) %c %d"：
 *   - %d: 读取第 1 个字段 pid
 *   - (%255[^)]): 读取左右括号之间的全部内容，安全提取完整的 comm
 *   - %c: 读取第 3 个字段进程状态（如 'S' 睡眠，'R' 运行）
 *   - %d: 读取第 4 个字段父进程 ID (ppid)
 *
 * 返回值：0 表示成功提取并赋值，-1 表示读取或格式匹配失败
 */
static int get_ppid_from_stat(pid_t pid, pid_t *ppid_out) {
  char path[128], line[4096];
  snprintf(path, sizeof(path), "%s/%d/stat", proc_path, pid);

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
  if (sscanf(line, "%d (%255[^)]) %c %d", &id, comm, &state, &ppid) != 4)
    return -1;

  *ppid_out = (pid_t)ppid;
  return 0;
}

/* =========================================================================
 * 阶段二：节点查找与多叉树排序辅助函数
 * ========================================================================= */

/**
 * find_proc: 根据 PID 在已收集的进程池中检索进程节点
 *
 * 返回值：指向匹配 Process 节点的指针；未找到返回 NULL
 */
static Process *find_proc(pid_t pid) {
  for (int i = 0; i < proc_count; i++) {
    if (procs[i].pid == pid) {
      return &procs[i];
    }
  }
  return NULL;
}

/**
 * cmp_pid: 用于 qsort 的比较函数，实现子进程按 PID 升序排列
 *
 * 遵循 qsort 规约：
 *   - 返回值 < 0: a 排在 b 前面
 *   - 返回值 > 0: a 排在 b 后面
 *   - 返回值 == 0: 两者相等
 */
static int cmp_pid(const void *a, const void *b) {
  const Process *p1 = *(const Process **)a;
  const Process *p2 = *(const Process **)b;
  return (p1->pid > p2->pid) - (p1->pid < p2->pid);
}

/* =========================================================================
 * 阶段三：深度优先搜索 (DFS) 递归排版打印
 * ========================================================================= */

/**
 * print_tree: 前序递归深度优先遍历打印进程子树
 *
 * @param p            当前正在打印的根/分支节点
 * @param depth        当前节点在树中的深度（用于计算缩进）
 * @param show_pids    是否显示进程 PID (-p 选项)
 * @param numeric_sort 是否按 PID 数值对子节点升序排序 (-n 选项)
 */
static void print_tree(Process *p, int depth, int show_pids, int numeric_sort) {
  if (!p)
    return;

  // 1. 打印当前层级的缩进（每层缩进 2 个空格）
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }

  // 2. 打印进程节点文本
  if (show_pids) {
    printf("%s(%d)\n", p->name, p->pid);
  } else {
    printf("%s\n", p->name);
  }

  // 3. 若开启 -n 且有多个子节点，在遍历前对 children 数组执行快速排序
  if (numeric_sort && p->child_count > 1) {
    qsort(p->children, p->child_count, sizeof(Process *), cmp_pid);
  }

  // 4. 深度优先递归打印每一个子节点
  for (int i = 0; i < p->child_count; i++) {
    print_tree(p->children[i], depth + 1, show_pids, numeric_sort);
  }
}

/* =========================================================================
 * 阶段四：主程序入口 (流程控制总调度)
 * ========================================================================= */

int main(int argc, char *argv[]) {
  // 1. 重置全局状态（防御单进程内多次运行测试用例时的脏数据污染）
  optind = 1;
#ifdef __APPLE__
  optreset = 1;
#endif
  proc_count = 0;
  proc_path = "/proc";

  // 2. 兼容 TestKit 系统测试框架的参数偏移特性
  if (argc > 1 && strcmp(argv[1], "./pstree") == 0) {
    argc--;
    argv++;
  }

  // 3. 命令行长选项表配置
  static struct option longopts[] = {
      {"show-pids", no_argument, NULL, 'p'},
      {"numeric-sort", no_argument, NULL, 'n'},
      {"version", no_argument, NULL, 'V'},
      {NULL, 0, NULL, 0}
  };

  // 4. 解析命令行参数
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

  // 5. 打开 /proc 虚拟文件系统（在非 Linux 系统上支持本地 mock 目录退守机制）
  DIR *d = opendir(proc_path);
  if (!d) {
    proc_path = "./mock_proc";
    d = opendir(proc_path);
    if (!d) {
      proc_path = "./pstree/mock_proc";
      d = opendir(proc_path);
    }
  }
  if (!d) {
    perror("opendir /proc");
    return 1;
  }

  // 6. 遍历目录，录入系统中所有有效进程
  struct dirent *de;
  while ((de = readdir(d)) != NULL) {
    // 门卫拦截：纯数字命名的子目录才对应真实的运行中进程
    if (!isdigit((unsigned char)de->d_name[0]))
      continue;

    pid_t pid = (pid_t)atoi(de->d_name);
    pid_t ppid;
    if (get_ppid_from_stat(pid, &ppid) != 0)
      continue;

    char comm[256] = "?";
    read_comm(pid, comm, sizeof(comm));

    if (proc_count < MAX_PROCS) {
      procs[proc_count].pid = pid;
      procs[proc_count].ppid = ppid;
      strncpy(procs[proc_count].name, comm, sizeof(procs[proc_count].name) - 1);
      procs[proc_count].name[sizeof(procs[proc_count].name) - 1] = '\0';
      procs[proc_count].child_count = 0;
      proc_count++;
    }
  }
  closedir(d);

  // 7. 扫描进程池，根据 PPID 将子节点指针挂入父节点的 children 数组中
  for (int i = 0; i < proc_count; i++) {
    Process *parent = find_proc(procs[i].ppid);
    if (parent && parent != &procs[i]) {
      if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = &procs[i];
      }
    }
  }

  // 8. 检索所有根节点（无父进程或父进程未收录），依次启动深度优先递归打印
  for (int i = 0; i < proc_count; i++) {
    if (procs[i].ppid == 0 || find_proc(procs[i].ppid) == NULL) {
      print_tree(&procs[i], 0, show_pids, numeric_sort);
    }
  }

  return 0;
}

