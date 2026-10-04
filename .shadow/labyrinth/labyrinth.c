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
  Position cur = findPlayer(labyrinth, playerId);
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

  if (!isValidPlayer(playerId) ||
      !isValidPlayer(labyrinth->map[cur.row][cur.col])) {
    return false;
  }
  if (!isEmptySpace(labyrinth, next.row, next.col)) {
    return false;
  }
  labyrinth->map[next.row][next.col] = playerId;
  labyrinth->map[cur.row][cur.col] = '.';
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

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col,
         bool visited[MAX_ROWS][MAX_COLS]) {
  // TODO: Implement this function
}

bool isConnected(Labyrinth *labyrinth) {
  // TODO: Implement this function
  return false;
}
