#include "labyrinth.h"
#include "../testkit/testkit.h"
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printUsage() {
  printf("Usage:\n");
  printf("  labyrinth --map map.txt --player id\n");
  printf("  labyrinth -m map.txt -p id\n");
  printf("  labyrinth --map map.txt --player id --move direction\n");
  printf("  labyrinth --version\n");
}

int main(int argc, char *argv[]) {
  // 重置 getopt 全局状态，支持在单进程内多次调用 main (测试套件运行机制)
  optind = 1;
#ifdef __APPLE__
  optreset = 1;
#endif

  // 兼容 testkit 系统测试：testkit 在 argv[0] 之后额外传入了 t.argv[0]
  // ("./labyrinth")
  if (argc > 1 && strcmp(argv[1], "./labyrinth") == 0) {
    argc--;
    argv++;
  }

  if (argc < 2) {
    printUsage();
    return 1;
  }

  // 检查是否包含 --version
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      if (argc == 2) {
        printf("%s\n", VERSION_INFO);
        return 0;
      } else {
        printUsage();
        return 1; // --version 和其他参数混用，非法！
      }
    }
  }
  // 定义长短选项
  struct option longopts[] = {
      {"map", required_argument, NULL, 'm'},
      {"player", required_argument, NULL, 'p'},
      {"move", required_argument, NULL, 'o'},
      {"version", no_argument, NULL, 'v'},
      {NULL, 0, NULL, 0},
  };
  // 解析命令行参数
  int opt;
  char *map_file = NULL;
  char *player_str = NULL;
  char *move_direction = NULL;

  while ((opt = getopt_long(argc, argv, "m:p:o:v", longopts, NULL)) != -1) {
    switch (opt) {
    case 'm':
      // optarg 是指针，直接指向命令行参数字符串，无需额外数组复制
      map_file = optarg;
      break;
    case 'p':
      player_str = optarg;
      break;
    case 'o':
      move_direction = optarg;
      break;
    case 'v':
      printf("%s\n", VERSION_INFO);
      return 0;
    default:
      printUsage();
      return 1;
    }
  }

  // 1. 检查是否有未识别的多余参数（如非法命令）
  if (optind < argc) {
    printUsage();
    return 1;
  }

  // 2. 检查必需参数：--map 和 --player 必须同时提供
  if (map_file == NULL || player_str == NULL) {
    printUsage();
    return 1;
  }

  // 3. 校验玩家 ID：必须是单个字符且在 '0'~'9' 之间
  if (strlen(player_str) != 1 || !isValidPlayer(player_str[0])) {
    return 1;
  }
  char playerId = player_str[0];

  // 4. 加载地图（若文件不存在、格式不一致等直接返回 1）
  Labyrinth lab;
  if (!loadMap(&lab, map_file)) {
    return 1;
  }

  // 5. 校验地图连通性（若所有空地不连通则返回 1）
  if (!isConnected(&lab)) {
    return 1;
  }

  // 6. 根据是否传入 --move 分支处理
  if (move_direction != NULL) {
    // 移动模式：移动玩家并写回文件
    if (!movePlayer(&lab, playerId, move_direction)) {
      return 1;
    }
    if (!saveMap(&lab, map_file)) {
      return 1;
    }
    return 0;
  } else {
    // 纯打印模式：原样输出地图内容
    for (int i = 0; i < lab.rows; i++) {
      printf("%s\n", lab.map[i]);
    }
    return 0;
  }
}

bool isValidPlayer(char playerId) {
  return (playerId >= '0' && playerId <= '9');
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    return false;
  }
  labyrinth->rows = 0;
  char buffer[MAX_COLS + 2];

  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    // 如果已经达到了最大行数限制，还有新行要读，说明迷宫过大
    if (labyrinth->rows >= MAX_ROWS) {
      fclose(fp);
      return false;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
      len--;
    }
    // 处理 Windows 换行符（CRLF 格式中的 \r）
    if (len > 0 && buffer[len - 1] == '\r') {
      buffer[len - 1] = '\0';
      len--;
    }

    int col = 0;

    if (labyrinth->rows == 0) {
      if (len == 0 || len > MAX_COLS) {
        fclose(fp);
        return false;
      }
      labyrinth->cols = len;
    } else if (len != labyrinth->cols) {
      fclose(fp);
      return false;
    }

    while (col < len) {
      char c = buffer[col];

      // 门卫检查：如果既不是墙，也不是空地，也不是合法玩家
      if (c != '#' && c != '.' && !isValidPlayer(c)) {
        fclose(fp);
        // 非法地图，拒绝加载！
        return false;
      }

      // 合法字符原样存入
      labyrinth->map[labyrinth->rows][col] = c;
      col++;
    }
    labyrinth->map[labyrinth->rows][col] = '\0';
    labyrinth->rows++;
  }
  fclose(fp);
  return labyrinth->rows > 0;
}

Position findPlayer(Labyrinth *labyrinth, char playerId) {
  Position pos = {-1, -1};

  for (int i = 0; i < labyrinth->rows; i++) {
    for (int j = 0; j < labyrinth->cols; j++) {
      if (labyrinth->map[i][j] == playerId) {
        pos.row = i;
        pos.col = j;
        return pos;
      }
    }
  }
  return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
  Position pos = {-1, -1};
  for (int i = 0; i < labyrinth->rows; i++) {
    for (int j = 0; j < labyrinth->cols; j++) {
      if (labyrinth->map[i][j] == '.') {
        pos.row = i;
        pos.col = j;
        return pos;
      }
    }
  }
  return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
  if (row >= 0 && row < labyrinth->rows && col >= 0 && col < labyrinth->cols) {
    return labyrinth->map[row][col] == '.';
  }
  return false;
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
  // 1. 先校验玩家 ID 合法性
  if (!isValidPlayer(playerId)) {
    return false;
  }
  // 2. 确定玩家当前位置（找不到则出生在第一个空地）
  Position cur = findPlayer(labyrinth, playerId);
  if (cur.row == -1) {
    cur = findFirstEmptySpace(labyrinth);
    if (cur.row == -1) {
      return false;
    }
  }
  // 3. 计算目标位置 next
  Position next = cur;
  if (strcmp(direction, "up") == 0) {
    next.row--;
  } else if (strcmp(direction, "down") == 0) {
    next.row++;
  } else if (strcmp(direction, "left") == 0) {
    next.col--;
  } else if (strcmp(direction, "right") == 0) {
    next.col++;
  } else {
    return false;
  }

  // 4. 检查目标位置是否合法且是空地
  if (!isEmptySpace(labyrinth, next.row, next.col)) {
    return false;
  }

  // 5. 更新地图
  if (cur.row != -1 && cur.col != -1) {
    labyrinth->map[cur.row][cur.col] = '.';
  }
  labyrinth->map[next.row][next.col] = playerId;

  return true;
}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    return false;
  }
  int i = 0;
  int j = 0;

  while (i < labyrinth->rows) {
    while (j < labyrinth->cols) {
      fputc(labyrinth->map[i][j], fp);
      j++;
    }
    fputc('\n', fp);
    j = 0;
    i++;
  }
  fclose(fp);
  return true;
}

// 连通性检测辅助函数
void dfs(Labyrinth *labyrinth, int row, int col,
         bool visited[MAX_ROWS][MAX_COLS]) {
  // 1. 越界检查
  if (row < 0 || row >= labyrinth->rows || col < 0 || col >= labyrinth->cols) {
    return;
  }
  // 2. 访问墙壁或已访问过的点
  if (labyrinth->map[row][col] == '#' || visited[row][col]) {
    return;
  }
  // 3. 标记已访问
  visited[row][col] = true;
  // 4. 递归访问四个方向
  dfs(labyrinth, row + 1, col, visited);
  dfs(labyrinth, row - 1, col, visited);
  dfs(labyrinth, row, col + 1, visited);
  dfs(labyrinth, row, col - 1, visited);
}

// 连通性检测主函数
bool isConnected(Labyrinth *labyrinth) {
  // 1. 初始化
  int emptyCount = 0;
  int start_row = -1;
  int start_col = -1;
  bool visited[MAX_ROWS][MAX_COLS];
  // 2. 统计空地数量并找到第一个空地
  for (int i = 0; i < labyrinth->rows; i++) {
    for (int j = 0; j < labyrinth->cols; j++) {
      if (labyrinth->map[i][j] == '.' || isValidPlayer(labyrinth->map[i][j])) {
        emptyCount++;
        if (start_row == -1) {
          start_row = i;
          start_col = j;
        }
      }
    }
  }
  // 3. 如果没有空地，则返回 true
  if (emptyCount == 0) {
    return true;
  }
  // 4. 初始化 visited 数组
  for (int i = 0; i < labyrinth->rows; i++) {
    for (int j = 0; j < labyrinth->cols; j++) {
      visited[i][j] = false;
    }
  }
  // 5. 从第一个空地开始 DFS
  dfs(labyrinth, start_row, start_col, visited);
  // 6. 检查是否有未访问的空地
  for (int i = 0; i < labyrinth->rows; i++) {
    for (int j = 0; j < labyrinth->cols; j++) {
      if ((labyrinth->map[i][j] == '.' ||
           isValidPlayer(labyrinth->map[i][j])) &&
          !visited[i][j]) {
        return false;
      }
    }
  }
  // 7. 如果所有空地都已访问，则返回 true
  return true;
}
