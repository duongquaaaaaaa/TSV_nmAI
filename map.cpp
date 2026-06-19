#include "map.h"
#include "AZRandom.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Map Building (Maze Generation)
// ─────────────────────────────────────────────────────────────────────────────
void GameMap::Build(b2World &world, MapMode mode) {
  currentMode = mode;

  auto addWall = [&](float x, float y, float width, float height) {
    b2BodyDef def;
    def.type = b2_staticBody;
    def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
    b2PolygonShape shape;
    shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE);
    b2FixtureDef fixture;
    fixture.shape = &shape;
    fixture.density = 0.0f;
    b2Body *body = world.CreateBody(&def);
    body->CreateFixture(&fixture);
    walls.push_back(body);
  };

  float cellW = CELL_W, cellH = CELL_H, wallThickness = 6.0f;
  float offsetX = OffsetX();
  float offsetY = OffsetY();

  // ── OPEN mode: border walls only, no interior walls ──────────────────
  if (mode == MapMode::OPEN) {
    // Clear all grid walls (A* sees fully open grid)
    for (int r = 0; r <= ROWS; r++)
      for (int c = 0; c < COLS; c++)
        hWalls[r][c] = false;
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c <= COLS; c++)
        vWalls[r][c] = false;
    // Set border walls (edges of the arena)
    for (int c = 0; c < COLS; c++) {
      hWalls[0][c] = true;
      hWalls[ROWS][c] = true;
    }
    for (int r = 0; r < ROWS; r++) {
      vWalls[r][0] = true;
      vWalls[r][COLS] = true;
    }

    // Build physical border walls
    for (int c = 0; c < COLS; c++) {
      addWall(offsetX + c * cellW + cellW / 2.0f, offsetY,
              cellW + wallThickness, wallThickness);
      addWall(offsetX + c * cellW + cellW / 2.0f, offsetY + ROWS * cellH,
              cellW + wallThickness, wallThickness);
    }
    for (int r = 0; r < ROWS; r++) {
      addWall(offsetX, offsetY + r * cellH + cellH / 2.0f, wallThickness,
              cellH + wallThickness);
      addWall(offsetX + COLS * cellW, offsetY + r * cellH + cellH / 2.0f,
              wallThickness, cellH + wallThickness);
    }
    return;
  }

  // ── Maze generation (SPARSE and NORMAL) ──────────────────────────────
  for (int r = 0; r <= ROWS; r++)
    for (int c = 0; c < COLS; c++)
      hWalls[r][c] = true;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c <= COLS; c++)
      vWalls[r][c] = true;

  bool visited[ROWS][COLS] = {false};
  std::vector<std::pair<int, int>> stack;

  int startR = AZ::Rand() % ROWS;
  int startC = AZ::Rand() % COLS;
  visited[startR][startC] = true;
  stack.push_back({startR, startC});

  // Recursive Backtracker
  while (!stack.empty()) {
    int r = stack.back().first;
    int c = stack.back().second;

    std::vector<int> neighbors;
    if (r > 0 && !visited[r - 1][c])
      neighbors.push_back(0);
    if (c < COLS - 1 && !visited[r][c + 1])
      neighbors.push_back(1);
    if (r < ROWS - 1 && !visited[r + 1][c])
      neighbors.push_back(2);
    if (c > 0 && !visited[r][c - 1])
      neighbors.push_back(3);

    if (!neighbors.empty()) {
      int dir = neighbors[AZ::Rand() % neighbors.size()];
      int nr = r, nc = c;
      if (dir == 0) {
        nr = r - 1;
        hWalls[r][c] = false;
      } else if (dir == 1) {
        nc = c + 1;
        vWalls[r][c + 1] = false;
      } else if (dir == 2) {
        nr = r + 1;
        hWalls[r + 1][c] = false;
      } else if (dir == 3) {
        nc = c - 1;
        vWalls[r][c] = false;
      }
      visited[nr][nc] = true;
      stack.push_back({nr, nc});
    } else {
      stack.pop_back();
    }
  }

  // Punch extra holes
  int extraHoles =
      (mode == MapMode::SPARSE) ? (20 + AZ::Rand() % 6) : (6 + AZ::Rand() % 4);
  int failSafe = 500;
  while (extraHoles > 0 && --failSafe > 0) {
    if (AZ::Rand() % 2 == 0) {
      int r = 1 + AZ::Rand() % (ROWS - 1);
      int c = AZ::Rand() % COLS;
      if (hWalls[r][c]) {
        hWalls[r][c] = false;
        extraHoles--;
      }
    } else {
      int r = AZ::Rand() % ROWS;
      int c = 1 + AZ::Rand() % (COLS - 1);
      if (vWalls[r][c]) {
        vWalls[r][c] = false;
        extraHoles--;
      }
    }
  }

  // Build physical walls from grid
  for (int r = 0; r <= ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (hWalls[r][c])
        addWall(offsetX + c * cellW + cellW / 2.0f, offsetY + r * cellH,
                cellW + wallThickness, wallThickness);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c <= COLS; c++)
      if (vWalls[r][c])
        addWall(offsetX + c * cellW, offsetY + r * cellH + cellH / 2.0f,
                wallThickness, cellH + wallThickness);
}

void GameMap::BuildFromRects(b2World& world, const std::vector<WallRect>& rects) {
  auto addWall = [&](float x, float y, float width, float height) {
    b2BodyDef def;
    def.type = b2_staticBody;
    def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
    b2PolygonShape shape;
    shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE);
    b2FixtureDef fixture;
    fixture.shape = &shape;
    fixture.density = 0.0f;
    b2Body *body = world.CreateBody(&def);
    body->CreateFixture(&fixture);
    walls.push_back(body);
  };

  for (const auto& rect : rects) {
    addWall(rect.x, rect.y, rect.width, rect.height);
  }
}

void GameMap::Clear(b2World &world) {
  for (b2Body *wall : walls)
    world.DestroyBody(wall);
  walls.clear();
}

b2Vec2 GameMap::GetRandomCellCenter() const {
  int row = AZ::Rand() % ROWS;
  int col = AZ::Rand() % COLS;
  return CellToWorld(row, col);
}

// ─────────────────────────────────────────────────────────────────────────────
//  A* Pathfinding Helpers
// ─────────────────────────────────────────────────────────────────────────────
std::pair<int, int> GameMap::WorldToCell(b2Vec2 worldPos) const {
  // Box2D → screen pixels
  float sx = worldPos.x * SCALE;
  float sy = SCREEN_HEIGHT - worldPos.y * SCALE; // Y flip
  // Screen pixels → grid cell
  int col = (int)((sx - OffsetX()) / CELL_W);
  int row = (int)((sy - OffsetY()) / CELL_H);
  col = std::clamp(col, 0, COLS - 1);
  row = std::clamp(row, 0, ROWS - 1);
  return {row, col};
}

b2Vec2 GameMap::CellToWorld(int row, int col) const {
  float sx = OffsetX() + col * CELL_W + CELL_W / 2.0f;
  float sy = OffsetY() + row * CELL_H + CELL_H / 2.0f;
  return b2Vec2(sx / SCALE, (SCREEN_HEIGHT - sy) / SCALE);
}

bool GameMap::CanMove(int fromR, int fromC, int toR, int toC) const {
  if (toR < 0 || toR >= ROWS || toC < 0 || toC >= COLS)
    return false;
  int dr = toR - fromR, dc = toC - fromC;
  if (dr == -1 && dc == 0)
    return !hWalls[fromR][fromC]; // UP
  if (dr == 1 && dc == 0)
    return !hWalls[fromR + 1][fromC]; // DOWN
  if (dr == 0 && dc == -1)
    return !vWalls[fromR][fromC]; // LEFT
  if (dr == 0 && dc == 1)
    return !vWalls[fromR][fromC + 1]; // RIGHT
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  A* Pathfinding
// ─────────────────────────────────────────────────────────────────────────────
b2Vec2 GameMap::GetNextWaypoint(b2Vec2 agentPos, b2Vec2 enemyPos) const {
  // OPEN mode: no maze → go straight
  if (currentMode == MapMode::OPEN)
    return enemyPos;

  auto [sr, sc] = WorldToCell(agentPos);
  auto [er, ec] = WorldToCell(enemyPos);

  // Same cell → go directly to enemy
  if (sr == er && sc == ec)
    return enemyPos;

  // A* search on grid
  struct Node {
    int r, c;
    float f; // f = g + h
    float g;
    bool operator>(const Node &o) const { return f > o.f; }
  };

  // Đưa về Stack để đạt tốc độ tối đa (6x8 = 48 ô, cực kỳ an toàn cho Stack)
  float gScore[ROWS][COLS];
  int parentR[ROWS][COLS], parentC[ROWS][COLS];
  bool closed[ROWS][COLS] = {}; // Reset nhanh bằng {}
  Node openBuf[4096]; // Đủ lớn để không bao giờ tràn

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      gScore[r][c] = 1e9f;
      parentR[r][c] = -1;
      parentC[r][c] = -1;
    }
  }

  auto heuristic = [](int r1, int c1, int r2, int c2) -> float {
    return (float)(std::abs(r1 - r2) + std::abs(c1 - c2));
  };

  int openCount = 0;

  auto pushOpen = [&](Node n) {
      if (openCount < 4096) {
          openBuf[openCount++] = n;
          std::push_heap(openBuf, openBuf + openCount, [](const Node& a, const Node& b) { return a.f > b.f; });
      }
  };

  auto popOpen = [&]() -> Node {
      std::pop_heap(openBuf, openBuf + openCount, [](const Node& a, const Node& b) { return a.f > b.f; });
      return openBuf[--openCount];
  };

  gScore[sr][sc] = 0;
  pushOpen({sr, sc, heuristic(sr, sc, er, ec), 0});
  parentR[sr][sc] = -1;
  parentC[sr][sc] = -1;

  constexpr int dr[] = {-1, 0, 1, 0};
  constexpr int dc[] = {0, 1, 0, -1};
  bool found = false;

  while (openCount > 0) {
    Node cur = popOpen();
    if (closed[cur.r][cur.c])
      continue;
    closed[cur.r][cur.c] = true;

    if (cur.r == er && cur.c == ec) {
      found = true;
      break;
    }

    for (int d = 0; d < 4; d++) {
      int nr = cur.r + dr[d], nc = cur.c + dc[d];
      if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
        continue;
      if (closed[nr][nc])
        continue;
      if (!CanMove(cur.r, cur.c, nr, nc))
        continue;

      float ng = cur.g + 1.0f;
      if (ng < gScore[nr][nc]) {
        gScore[nr][nc] = ng;
        parentR[nr][nc] = cur.r;
        parentC[nr][nc] = cur.c;
        pushOpen({nr, nc, ng + heuristic(nr, nc, er, ec), ng});
      }
    }
  }

  if (!found)
    return enemyPos; // No path → fallback to direct

  // 1. Trace back to build the path array (cũng dùng mảng Stack cố định)
  std::pair<int, int> pathBuf[ROWS * COLS];
  int pathCount = 0;
  
  int tr = er, tc = ec;
  while (tr != sr || tc != sc) {
    if (pathCount >= ROWS * COLS)
      break; // Bảo vệ Stack: Không bao giờ ghi quá kích thước mảng
    pathBuf[pathCount++] = {tr, tc};
    int pr = parentR[tr][tc];
    int pc = parentC[tr][tc];
    // Kiểm tra tính hợp lệ của tọa độ cha
    if (pr < 0 || pr >= ROWS || pc < 0 || pc >= COLS)
      break;
    tr = pr;
    tc = pc;
  }
  if (pathCount == 0)
    return enemyPos;

  // 1. KÉO CĂNG ĐƯỜNG THẲNG (Tìm điểm xa nhất trên trục)
  int targetIdx = pathCount - 1;
  int currentR = sr, currentC = sc;
  int dr_dir = pathBuf[targetIdx].first - currentR;
  int dc_dir = pathBuf[targetIdx].second - currentC;

  while (targetIdx > 0) {
    int nextDr = pathBuf[targetIdx - 1].first - pathBuf[targetIdx].first;
    int nextDc = pathBuf[targetIdx - 1].second - pathBuf[targetIdx].second;
    if (nextDr == dr_dir && nextDc == dc_dir) {
      targetIdx--;
    } else {
      break;
    }
  }

  // targetIdx đang trỏ đến điểm XA NHẤT trên cùng 1 đường thẳng
  int farR = pathBuf[targetIdx].first;
  int farC = pathBuf[targetIdx].second;

  // 2. KỸ THUẬT PURE PURSUIT (CÀ RỐT TRÊN GẬY) - Chống giật 100%
  b2Vec2 P1 = CellToWorld(sr, sc);
  b2Vec2 P2 = (targetIdx == 0) ? enemyPos : CellToWorld(farR, farC);
  
  b2Vec2 lineDir = P2 - P1;
  float totalLen = lineDir.Length();
  
  if (totalLen < 1e-4f) {
      return enemyPos; 
  }
  
  lineDir.Normalize();
  
  // Chiếu vị trí hiện tại của xe lên trục đường thẳng P1->P2
  float projLen = (agentPos.x - P1.x) * lineDir.x + (agentPos.y - P1.y) * lineDir.y;
  
  // Tính khoảng cách vuông góc từ xe đến đường thẳng P1->P2
  b2Vec2 projPoint(P1.x + lineDir.x * projLen, P1.y + lineDir.y * projLen);
  float orthoDist = (agentPos - projPoint).Length();
  
  // [ANTI-CORNER CUTTING]: Nếu xe còn cách xa trục đường mới (đang tiến vào ngã rẽ),
  // bắt buộc phải đi tới tâm ngã rẽ (P1) trước để tránh bẻ cua quá sớm gây kẹt tường.
  if (orthoDist > 1.0f) {
      return P1;
  }
  
  // Lookahead: Luôn đặt "củ cà rốt" cách xe 3.0m (1 ô) về phía trước trên trục giữa
  float lookahead = 3.0f; 
  float targetDist = projLen + lookahead;
  
  // Nếu cà rốt vượt quá ngã rẽ, giữ nó ở tâm ngã rẽ để xe chuẩn bị cua
  if (targetDist > totalLen) {
      targetDist = totalLen;
  }
  
  b2Vec2 carrot(P1.x + lineDir.x * targetDist, P1.y + lineDir.y * targetDist);
  return carrot;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Classic Bot Pathfinding (A* with Fat Raycast and String Pulling)
// ─────────────────────────────────────────────────────────────────────────────

bool CheckClearance(b2World& world, b2Vec2 p1, b2Vec2 p2) {
    b2Vec2 dir = p2 - p1;
    float len = dir.Length();
    if (len < 1e-4f) return true;
    dir.Normalize();

    b2Vec2 norm(-dir.y, dir.x);
    float offset = 10.5f / SCALE;

    struct ClearanceCast : public b2RayCastCallback {
        bool hit = false;
        float ReportFixture(b2Fixture* fixture, const b2Vec2&, const b2Vec2&, float) override {
            if (fixture->GetBody()->GetType() == b2_staticBody) {
                hit = true;
                return 0.0f;
            }
            return -1.0f;
        }
    } c1, c2, c3;

    world.RayCast(&c1, p1, p2);
    if (c1.hit) return false;

    world.RayCast(&c2, p1 + offset * norm, p2 + offset * norm);
    if (c2.hit) return false;

    world.RayCast(&c3, p1 - offset * norm, p2 - offset * norm);
    if (c3.hit) return false;

    return true;
}

b2Vec2 GameMap::GetNextWaypoint(b2World& world, b2Vec2 start, b2Vec2 target, int& pathDistance, std::vector<b2Vec2>* outPath) const {
    float cellW = CELL_W, cellH = CELL_H;
    float offsetX = OffsetX();
    float offsetY = OffsetY();

    auto toGrid = [&](b2Vec2 physPos, int& row, int& col) {
        float screenX = physPos.x * SCALE;
        float screenY = SCREEN_HEIGHT - physPos.y * SCALE;
        col = (int)floorf((screenX - offsetX) / cellW);
        row = (int)floorf((screenY - offsetY) / cellH);
        if (col < 0) col = 0; if (col >= COLS) col = COLS - 1;
        if (row < 0) row = 0; if (row >= ROWS) row = ROWS - 1;
    };

    auto toPhysics = [&](int row, int col) -> b2Vec2 {
        float x = offsetX + col * cellW + cellW / 2.0f;
        float y = offsetY + row * cellH + cellH / 2.0f;
        return b2Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
    };

    int startRow, startCol, targetRow, targetCol;
    toGrid(start, startRow, startCol);
    toGrid(target, targetRow, targetCol);

    if (startRow == targetRow && startCol == targetCol) {
        pathDistance = 0;
        if (outPath) {
            outPath->clear();
            outPath->push_back(start);
            outPath->push_back(target);
        }
        return target;
    }

    float dist[ROWS][COLS];
    int parentR[ROWS][COLS], parentC[ROWS][COLS];
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            dist[r][c] = 1e9f;
            parentR[r][c] = -1;
            parentC[r][c] = -1;
        }
    }

    struct Node {
        int r, c;
        float f, g;
        bool operator>(const Node& other) const { return f > other.f; }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    dist[targetRow][targetCol] = 0.0f;
    pq.push({targetRow, targetCol, 0.0f, 0.0f});

    while (!pq.empty()) {
        Node top = pq.top(); pq.pop();
        int r = top.r, c = top.c;
        if (top.g > dist[r][c]) continue;
        if (r == startRow && c == startCol) break;

        auto checkNeighbor = [&](int nr, int nc, bool hasWall) {
            if (!hasWall) {
                int wallCount = 0;
                if (hWalls[nr][nc]) wallCount++;
                if (hWalls[nr+1][nc]) wallCount++;
                if (vWalls[nr][nc]) wallCount++;
                if (vWalls[nr][nc+1]) wallCount++;

                float moveCost = 1.0f;
                if (wallCount >= 3) moveCost += 20.0f;
                else if (wallCount == 2) moveCost += 3.0f;
                else if (wallCount == 1) moveCost += 0.5f;

                float new_g = dist[r][c] + moveCost;
                if (new_g < dist[nr][nc]) {
                    dist[nr][nc] = new_g;
                    parentR[nr][nc] = r;
                    parentC[nr][nc] = c;

                    float h = (float)(std::abs(nr - startRow) + std::abs(nc - startCol));
                    h *= 1.001f;

                    pq.push({nr, nc, new_g + h, new_g});
                }
            }
        };

        if (r > 0) checkNeighbor(r - 1, c, hWalls[r][c]);
        if (r < ROWS - 1) checkNeighbor(r + 1, c, hWalls[r+1][c]);
        if (c > 0) checkNeighbor(r, c - 1, vWalls[r][c]);
        if (c < COLS - 1) checkNeighbor(r, c + 1, vWalls[r][c+1]);
    }

    if (parentR[startRow][startCol] == -1) {
        pathDistance = 999;
        if (outPath) {
            outPath->clear();
            outPath->push_back(start);
            outPath->push_back(target);
        }
        return target;
    }

    pathDistance = (int)dist[startRow][startCol];

    std::vector<b2Vec2> pathNodes;
    int currR = startRow, currC = startCol;
    while (currR != targetRow || currC != targetCol) {
        int nR = parentR[currR][currC];
        int nC = parentC[currR][currC];
        if (nR == -1) break;
        currR = nR;
        currC = nC;
        pathNodes.push_back(toPhysics(currR, currC));
    }

    pathNodes.pop_back();
    pathNodes.push_back(target);

    b2Vec2 bestWaypoint = pathNodes[0];
    for (int i = pathNodes.size() - 1; i >= 0; i--) {
        if (CheckClearance(world, start, pathNodes[i])) {
            bestWaypoint = pathNodes[i];
            break;
        }
    }

    if (outPath) {
        outPath->clear();
        outPath->push_back(start);
        outPath->push_back(bestWaypoint);

        bool found = false;
        for (size_t i = 0; i < pathNodes.size(); i++) {
            if (found) {
                outPath->push_back(pathNodes[i]);
            } else if (pathNodes[i].x == bestWaypoint.x && pathNodes[i].y == bestWaypoint.y) {
                found = true;
            }
        }
    }

    return bestWaypoint;
}

std::vector<b2Vec2> GameMap::GetFullPath(b2World& world, b2Vec2 start, b2Vec2 target, const std::vector<std::pair<int,int>>& blockedCells) const {
    float cellW = CELL_W, cellH = CELL_H;
    float offsetX = OffsetX();
    float offsetY = OffsetY();

    auto toGrid = [&](b2Vec2 physPos, int& row, int& col) {
        float screenX = physPos.x * SCALE;
        float screenY = SCREEN_HEIGHT - physPos.y * SCALE;
        col = (int)floorf((screenX - offsetX) / cellW);
        row = (int)floorf((screenY - offsetY) / cellH);
        if (col < 0) col = 0; if (col >= COLS) col = COLS - 1;
        if (row < 0) row = 0; if (row >= ROWS) row = ROWS - 1;
    };

    auto toPhysics = [&](int row, int col) -> b2Vec2 {
        float x = offsetX + col * cellW + cellW / 2.0f;
        float y = offsetY + row * cellH + cellH / 2.0f;
        return b2Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE);
    };

    int startRow, startCol, targetRow, targetCol;
    toGrid(start, startRow, startCol);
    toGrid(target, targetRow, targetCol);

    std::vector<b2Vec2> result;
    result.push_back(start);

    if (startRow == targetRow && startCol == targetCol) {
        result.push_back(target);
        return result;
    }

    float dist[ROWS][COLS];
    int parentR[ROWS][COLS], parentC[ROWS][COLS];
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            dist[r][c] = 1e9f;
            parentR[r][c] = -1;
            parentC[r][c] = -1;
        }
    }

    struct Node {
        int r, c;
        float f, g;
        bool operator>(const Node& other) const { return f > other.f; }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    dist[targetRow][targetCol] = 0.0f;
    pq.push({targetRow, targetCol, 0.0f, 0.0f});

    while (!pq.empty()) {
        Node top = pq.top(); pq.pop();
        int r = top.r, c = top.c;
        if (top.g > dist[r][c]) continue;
        if (r == startRow && c == startCol) break;

        auto checkNeighbor = [&](int nr, int nc, bool hasWall) {
            if (!hasWall) {
                int wallCount = 0;
                if (hWalls[nr][nc]) wallCount++;
                if (hWalls[nr+1][nc]) wallCount++;
                if (vWalls[nr][nc]) wallCount++;
                if (vWalls[nr][nc+1]) wallCount++;

                float moveCost = 1.0f;
                if (wallCount >= 3) moveCost += 20.0f;
                else if (wallCount == 2) moveCost += 3.0f;
                else if (wallCount == 1) moveCost += 0.5f;

                for (const auto& bc : blockedCells) {
                    if (nr == bc.first && nc == bc.second) {
                        moveCost += 50.0f;
                        break;
                    }
                }

                float new_g = dist[r][c] + moveCost;
                if (new_g < dist[nr][nc]) {
                    dist[nr][nc] = new_g;
                    parentR[nr][nc] = r;
                    parentC[nr][nc] = c;

                    float h = (float)(std::abs(nr - startRow) + std::abs(nc - startCol));
                    h *= 1.001f;

                    pq.push({nr, nc, new_g + h, new_g});
                }
            }
        };

        if (r > 0) checkNeighbor(r - 1, c, hWalls[r][c]);
        if (r < ROWS - 1) checkNeighbor(r + 1, c, hWalls[r+1][c]);
        if (c > 0) checkNeighbor(r, c - 1, vWalls[r][c]);
        if (c < COLS - 1) checkNeighbor(r, c + 1, vWalls[r][c+1]);
    }

    if (parentR[startRow][startCol] == -1) {
        result.push_back(target);
        return result;
    }

    std::vector<b2Vec2> rawNodes;
    int currR = startRow, currC = startCol;
    while (currR != targetRow || currC != targetCol) {
        int nR = parentR[currR][currC];
        int nC = parentC[currR][currC];
        if (nR == -1) break;
        currR = nR;
        currC = nC;
        rawNodes.push_back(toPhysics(currR, currC));
    }
    rawNodes.push_back(target);

    b2Vec2 current = start;
    size_t currentIdx = 0;

    while (currentIdx < rawNodes.size()) {
        int maxLookAhead = std::min((int)rawNodes.size() - 1, (int)currentIdx + 3);
        int bestIdx = (int)currentIdx;
        for (int i = maxLookAhead; i >= (int)currentIdx; i--) {
            if (CheckClearance(world, current, rawNodes[i])) {
                bestIdx = i;
                break;
            }
        }

        result.push_back(rawNodes[bestIdx]);
        current = rawNodes[bestIdx];
        currentIdx = bestIdx + 1;
    }

    return result;
}