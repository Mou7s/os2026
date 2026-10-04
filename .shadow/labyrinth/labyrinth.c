#include "labyrinth.h"
#include "../testkit/testkit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // TODO: Implement this function
  return 0;
}

void printUsage() {
  printf("Usage:\n");
  printf("  labyrinth --map map.txt --player id\n");
  printf("  labyrinth -m map.txt -p id\n");
  printf("  labyrinth --map map.txt --player id --move direction\n");
  printf("  labyrinth --version\n");
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
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
      len--;
    }
    int col = 0;

    if (labyrinth->rows == 0) {
      labyrinth->cols = len;

    } else if (len != labyrinth->cols) {
      fclose(fp);
      return false;
    }

    while (col < len) {
      labyrinth->map[labyrinth->rows][col] = buffer[col];
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
  bool visited[MAX_ROWS][MAX_COLS] = {false};
  // 2. 统计空地数量并找到第一个空地
  for (int i = 0; i < labyrinth->rows; i++) {
    for (int j = 0; j < labyrinth->cols; j++) {
      if (labyrinth->map[i][j] == '.') {
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
      if (labyrinth->map[i][j] == '.' && !visited[i][j]) {
        return false;
      }
    }
  }
  // 7. 如果所有空地都已访问，则返回 true
  return true;
}
