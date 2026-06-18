#pragma once
#include "game.h"
#include "Observation.h"
#include "Curriculum.h"
#include <algorithm>
#include <cmath>
#include <vector>

// ═════════════════════════════════════════════════════════════════════════════
//  TRIẾT LÝ THIẾT KẾ REWARD (ĐỌC TRƯỚC KHI SỬA)
// ═════════════════════════════════════════════════════════════════════════════
//
//  Nguyên tắc 1 — END BONUS PHẢI THỐNG TRỊ:
//    Step reward chỉ là "la bàn" giúp NEAT thoát khỏi vùng tối sớm.
//    End bonus (Win/Loss) phải chiếm > 60% tổng fitness của một episode tốt.
//    Nếu step reward quá lớn, agent học "diễn" tốt thay vì "thắng".
//
//  Nguyên tắc 2 — MỘT TÍN HIỆU RÕ HƠN NHIỀU TÍN HIỆU NHỎ:
//    Mỗi kênh reward phải có mục đích duy nhất, không chồng chéo.
//    Reward chồng chéo tạo ra local optima giả (agent tối ưu nhiều kênh
//    cùng lúc nhưng không làm được việc thật).
//
//  Nguyên tắc 3 — KHÔNG THƯỞNG TRẠNG THÁI, CHỈ THƯỞNG HÀNH VI:
//    "Đứng ở 4m" là trạng thái → không thưởng.
//    "Di chuyển về phía waypoint" là hành vi → thưởng.
//    Sweet spot reward (+3 pts/s chỉ vì ở đúng tầm) vi phạm nguyên tắc này.
//
//  Scale tham chiếu (Phase 1-3, episode 1500 steps = 25 giây):
//    Navigation reward tối đa: ~+80 pts  (nếu đi đúng hướng 25 giây)
//    Aim reward tối đa:        ~+60 pts  (nếu ngắm chuẩn 25 giây khi LOS)
//    Penalty tối đa (idle):    ~-80 pts
//    End bonus Win:            +400~600 pts  ← PHẢI LÀ TÍN HIỆU CHỦ ĐẠO
//    End bonus Loss:           -400 pts
//
// ═════════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: Kiểm tra đạn có đang BAY ĐẾN agent không (local frame obs)
// ─────────────────────────────────────────────────────────────────────────────
// CÁCH GHI NHỚ ĐÚNG:
// dx_n, dy_n = hình chiếu của vector (agent → đạn) lên local frame của agent.
// vx_n, vy_n = hình chiếu của velocity đạn lên local frame của agent.
// Nếu đạn đang bay ĐẾN gần: velocity của nó phải ngược chiều vector (agent→đạn)
//   → tích dx_n*vx_n âm → dot < 0 là NGUY HIỂM (ĐÚNG)
// Nếu đạn bay RA XA: velocity cùng chiều → dot > 0 (AN TOÀN)
static inline bool IsBulletApproaching(float dx_n, float dy_n,
                                        float vx_n, float vy_n) {
  float dSq = dx_n * dx_n + dy_n * dy_n;
  if (dSq < 0.0001f || dSq > 1.0f) return false; // Ngoài tầm radar (8.0m)

  // Đạn cực gần (<0.95m) → nguy hiểm bất kể hướng
  if (dSq < 0.014f) return true;

  // dot < -0.05 → đạn đang bay VỀ PHÍA agent (velocity ngược chiều vector agent→đạn)
  float dot = dx_n * vx_n + dy_n * vy_n;
  return dot < -0.05f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ComputeStepReward
// ─────────────────────────────────────────────────────────────────────────────
inline float ComputeStepReward(const Game &game, int agentIdx, bool canSeeEnemy,
                               b2Vec2 astarWaypoint,
                               float prevDist,    // dist TRƯỚC step (từ train.cpp)
                               float &outCurrDist, // dist HIỆN TẠI (từ train.cpp, chỉ đọc)
                               const TankActions &actions,
                               const std::vector<float> &agentObs,
                               Phase phase) {
  const Tank *agent = nullptr;
  const Tank *enemy = nullptr;
  for (auto *t : game.tanks) {
    if (t->playerIndex == agentIdx) agent = t;
    else                            enemy = t;
  }

  if (!agent || agentObs.size() < 36) {
    // outCurrDist không thay đổi — train.cpp đã tính đúng rồi
    return 0.0f;
  }

  float reward = 0.0f;
  constexpr float DT_SCALE = 1.0f / 60.0f;

  // ── Trích obs cần thiết ──
  float ammoLevel  = agentObs[21];
  float radarFront = agentObs[24];
  float radarLeft  = agentObs[25];
  float radarRight = agentObs[26];

  // [FIX #1] Bullet danger với logic dot product đã sửa
  bool dangerAlert = IsBulletApproaching(agentObs[13], agentObs[14], agentObs[15], agentObs[16])
                  || IsBulletApproaching(agentObs[17], agentObs[18], agentObs[19], agentObs[20]);

  // ── Kinematic state của agent ──
  b2Vec2 aPos     = agent->body->GetPosition();
  float  aAngle   = agent->body->GetAngle();
  b2Vec2 fwd(-std::sin(aAngle), std::cos(aAngle));
  b2Vec2 vel      = agent->body->GetLinearVelocity();
  float  angVel   = std::abs(agent->body->GetAngularVelocity());
  float  speedNow = vel.Length();
  float  fwdSpeed = vel.x * fwd.x + vel.y * fwd.y;
  constexpr float maxSpeed = 3.0f;

  // ── Waypoint direction ──
  b2Vec2 toTarget     = astarWaypoint - aPos;
  float  toTargetLen  = toTarget.Length();
  b2Vec2 toTargetNorm = (toTargetLen > 1e-4f)
                        ? b2Vec2(toTarget.x / toTargetLen, toTarget.y / toTargetLen)
                        : b2Vec2(fwd.x, fwd.y);
  float dotWp = fwd.x * toTargetNorm.x + fwd.y * toTargetNorm.y;

  // ── Wall check ──
  // Tăng threshold lên 0.15f (~1.5m) để AI phản ứng TRƯỚC khi chạm vật lý
  bool noseInWall = (radarFront < 0.15f || radarLeft < 0.15f || radarRight < 0.15f);

  // [FIX #2] Sửa lỗi CHẾT NGƯỜI: train.cpp KHÔNG cập nhật distToEnemy trong vòng lặp!
  // Nó chỉ gán prev = distToEnemy, và phó mặc cho hàm này cập nhật outCurrDist.
  // Nếu hàm này không cập nhật outCurrDist, distToEnemy sẽ vĩnh viễn bằng khoảng cách ban đầu.
  float currDist = prevDist; // Giá trị mặc định nếu không thấy địch
  float dotAim = 0.0f;       
  if (enemy) {
    b2Vec2 ePos    = enemy->body->GetPosition();
    b2Vec2 toEnemy = ePos - aPos;
    currDist       = toEnemy.Length();
    outCurrDist    = currDist; // Cập nhật ngược lại ra ngoài cho train.cpp vòng sau

    float  dE      = currDist;
    if (dE > 1e-4f) {
      b2Vec2 toEnNorm(toEnemy.x / dE, toEnemy.y / dE);
      dotAim = fwd.x * toEnNorm.x + fwd.y * toEnNorm.y;
    }
  }

  // (Đã gộp Phase 4 vào logic Dense Reward của Phase 1, 2, 3 bên dưới để tăng mật độ reward)

  // ═══════════════════════════════════════════════════════════════════════════
  // PHASE 5 — PURE SPARSE
  // Chỉ chống các degenerate behavior. Tuyệt đối không có aim hint.
  // ═══════════════════════════════════════════════════════════════════════════
  if (phase == Phase::PHASE5) {
    if (!canSeeEnemy) {
      if (speedNow < 0.2f)
        reward -= 1.5f * DT_SCALE;
      if (fwdSpeed > 0.3f && dotWp > 0.0f)
        reward += 0.8f * DT_SCALE;
    }
    if (angVel > 3.0f && speedNow < 0.5f)
      reward -= 2.0f * DT_SCALE;
    if (noseInWall) {
      if (actions.forward)  reward -= 4.0f * DT_SCALE;
      else if (actions.backward) reward += 0.5f * DT_SCALE;
      else reward -= 1.5f * DT_SCALE;
    }
    // Né đạn được thưởng ngay cả ở Phase 5 — đây là survival instinct,
    // không phải chiến thuật cố định, không tạo bias.
    if (dangerAlert && speedNow > 1.0f)
      reward += 1.0f * DT_SCALE;

    return reward;
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // PHASE 1, 2, 3 — DENSE REWARD
  // ═══════════════════════════════════════════════════════════════════════════

  // ══ KÊNH 0: CHỐNG DEGENERATE BEHAVIOR (ưu tiên cao nhất, chạy luôn) ══

  // Chống tường
  if (noseInWall) {
    if (actions.forward)       reward -= 5.0f * DT_SCALE;
    else if (actions.backward) reward += 0.8f * DT_SCALE;
    else                       reward -= 1.5f * DT_SCALE;
  }

  // Chống beyblade (xoay tại chỗ)
  if (angVel > 2.5f && speedNow < 0.5f)
    reward -= 4.0f * DT_SCALE;

  // Chống idle khi chưa thấy địch
  if (!canSeeEnemy && speedNow < 0.4f && !noseInWall)
    reward -= 2.0f * DT_SCALE;

  // Chống áp sát (kamikaze) — tăng tỉ lệ thuận với mức độ quá gần
  if (currDist < 1.8f)
    reward -= 5.0f * DT_SCALE * (1.8f - currDist) / 1.8f;

  // ══ KÊNH 1: NAVIGATION (khi chưa thấy địch hoặc địch còn xa) ══
  // [FIX #3] Thưởng HÀNH VI di chuyển (velocity dot waypoint), không thưởng trạng thái.
  // Không còn "sweet spot reward" tĩnh — agent phải chủ động di chuyển để được thưởng.
  if (!canSeeEnemy || currDist > 8.0f) {
    if (!noseInWall) {
      float navReward = 0.0f;
      if (phase == Phase::PHASE1 || phase == Phase::PHASE2) {
        // P1/2: Thưởng khi tiến về waypoint (forward + align với waypoint)
        if (fwdSpeed > 0.0f && dotWp > 0.0f)
          navReward = (fwdSpeed / maxSpeed) * dotWp * 5.0f * DT_SCALE;
      } else {
        // P3: Cho phép strafe — thưởng bất kỳ velocity nào hướng về waypoint
        float moveDot = vel.x * toTargetNorm.x + vel.y * toTargetNorm.y;
        if (moveDot > 0.0f)
          navReward = (moveDot / maxSpeed) * 5.0f * DT_SCALE;
      }
      reward += navReward;
    }

    // P3+: Phạt lùi khi chưa thấy địch (trừ khi có đạn bay tới)
    if (phase >= Phase::PHASE3 && actions.backward && !dangerAlert)
      reward -= 3.0f * DT_SCALE;
  }

  // ══ KÊNH 2: ENGAGEMENT (khi thấy địch ở tầm bắn) ══
  if (canSeeEnemy && currDist <= 8.0f && !noseInWall) {

    // [FIX #4] Aim reward bị giảm xuống để không át đảo end bonus.
    // Max aim reward: 5.0 * DT_SCALE * 60 * 25s ở sweet spot = ~125 pts
    // nhưng agent chỉ có LOS một phần episode → thực tế ~40-60 pts.
    // End bonus Win (+400) vẫn thống trị.
    float aimScale = (currDist < 3.0f) ? 2.0f
                   : (currDist < 8.0f) ? 5.0f  // Giảm từ 10 → 5
                                       : 2.0f;
    reward += dotAim * aimScale * DT_SCALE;

    // [FIX #4] Phạt đứng im khi thấy địch — áp dụng mọi phase để ngăn farm aim tĩnh
    if (speedNow < 0.5f && dotAim < 0.85f)
      reward -= 2.5f * DT_SCALE;

    // P3+: Phạt lùi trốn khi ở sweet spot (trừ khi né đạn)
    if (phase >= Phase::PHASE3 && actions.backward && !dangerAlert && currDist >= 3.0f)
      reward -= 5.0f * DT_SCALE;

    // P3+: Thưởng strafe (di chuyển ngang) trong engagement — dạy agent né đạn V2 trong khi giữ aim
    // Velocity ngang (lateral) trong local frame = vel.x * (-fwd.y) + vel.y * fwd.x
    if (phase >= Phase::PHASE3 && currDist >= 3.0f && canSeeEnemy) {
      float lateralSpeed = std::abs(-vel.x * fwd.y + vel.y * fwd.x); // độc lập với hướng
      if (lateralSpeed > 0.5f)
        reward += lateralSpeed / maxSpeed * 2.0f * DT_SCALE; // max ~2 pts/s nhỏ
    }

    // [XÓA] retreatDelta penalty — Phạt OAN khi ENEMY di chuyển ra xa, không phải agent lùi.
    // currDist tăng có thể do V2 tự lùi sau khi bắn, không chỉ do agent bỏ chạy.
    // Đã có penalty actions.backward ở trên là đủ để ngăn agent chủ động lùi.
  }

  // ══ KÊNH 3: NÉ ĐẠN ══
  // [FIX #1 hệ quả] Giờ dangerAlert đúng → reward né đạn mới có ý nghĩa.
  if (dangerAlert && speedNow > 1.0f)
    reward += 1.5f * DT_SCALE;

  // ══ KÊNH 4: XẠ THỦ ══
  // [FIX #4] Shoot reward bị giảm và restructure để không tạo aim-lock exploit.
  if (actions.shoot) {
    if (noseInWall) {
      // Nếu mũi đang húc tường thì coi như bắn bừa và không nhận được điểm thưởng.
      // Agent phải gỡ kẹt trước khi được thưởng điểm bắn súng.
      reward -= 2.0f * DT_SCALE;
    } else if (phase == Phase::PHASE1 || phase == Phase::PHASE2) {
      if (canSeeEnemy && dotAim > 0.90f && currDist < 10.0f) {
        // [FIX] Giảm từ +15 → +8, và KHÔNG cộng thêm nếu đứng im
        // Mục tiêu: bắn chuẩn khi di chuyển tốt hơn bắn chuẩn khi đứng im
        float shootBonus = 8.0f * DT_SCALE;
        if (speedNow < 0.3f) shootBonus *= 0.4f; // Phạt ngầm khi đứng im bắn
        reward += shootBonus;
      } else {
        reward -= 3.0f * DT_SCALE; // Bắn bừa
      }
    } else if (phase == Phase::PHASE3 || phase == Phase::PHASE4) {
      if (!canSeeEnemy) {
        reward -= 3.0f * DT_SCALE;
      } else if (ammoLevel > 0.01f) {
        if (dotAim > 0.85f && currDist < 10.0f) {
          float shootBonus = 7.0f * DT_SCALE; // Giảm từ 12 → 7
          if (speedNow < 0.3f) shootBonus *= 0.4f;
          reward += shootBonus;
        } else if (dotAim > 0.60f) {
          reward -= 0.5f * DT_SCALE; // Bắn lệch nhẹ: phạt rất nhẹ
        } else {
          reward -= 2.5f * DT_SCALE; // Bắn bừa
        }
      } else {
        reward -= 0.5f * DT_SCALE; // Bắn khi hết đạn (cooldown)
      }
    }
  } else {
    // Phạt chần chừ khi đã ngắm chuẩn ở sweet spot (chỉ P3)
    // [FIX #6] Kiểm tra cooldown súng (obs[35]) để tránh phạt sai khi đang reload.
    // ammoLevel > 0.45f nghĩa là activeBullets <= 2 (vì 1 - 2/5 = 0.6 > 0.45).
    // Nếu maxBullets=3 và cả 3 viên đang bay (ammoLevel=0.4), điều kiện NÀY không được thỏa
    // → không phạt oan khi agent đã bắn tối đa mà đang đợi đạn hết hạn.
    float shootReady = agentObs[35];
    if ((phase == Phase::PHASE3 || phase == Phase::PHASE4) && canSeeEnemy && ammoLevel > 0.45f && shootReady > 0.8f &&
        dotAim > 0.85f && currDist >= 3.0f && currDist < 8.0f) {
      reward -= 4.0f * DT_SCALE; 
    }
  }

  return reward;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ComputeEndBonus
// ─────────────────────────────────────────────────────────────────────────────
// [FIX #5] End bonus được tăng lên để đảm bảo nó THỐNG TRỊ step reward.
// Với step reward tối đa ~100-150 pts, end bonus Win phải >> 150 pts.
// Giữ nguyên P4/5 vì chúng đã là sparse (step reward rất nhỏ).
inline float ComputeEndBonus(const Game &game, int agentIdx,
                             const int scoresBefore[4], const int killsBefore[4], int stepsTaken,
                             int maxSteps, bool agentDidShoot, Phase phase) {
  float bonus = 0.0f;
  bool agentAlive = false;
  const Tank *enemy = nullptr;

  for (auto *t : game.tanks) {
    if (t->playerIndex == agentIdx) agentAlive = true;
    else                            enemy = t;
  }

  bool  enemyAlive = (enemy != nullptr);
  bool  agentWin   = (game.playerScores[agentIdx] > scoresBefore[agentIdx]);
  bool  agentKill  = (game.playerKills[agentIdx] > killsBefore[agentIdx]);
  float timeRatio  = 1.0f - (float)stepsTaken / (float)maxSteps;

  // \u2500\u2500 PHASE 4 & 5: Sparse (gi\u1eef nguy\u00ean) \u2500\u2500
  if (phase == Phase::PHASE4 || phase == Phase::PHASE5) {
    if (agentWin) {
      if (agentKill) {
        bonus += 500.0f + timeRatio * 500.0f; // Max 1000 pts
      } else {
        bonus += 100.0f; // K\u1ebb \u0111\u1ecbch t\u1ef1 s\u00e1t (v\u00e2n s\u1ed1ng s\u00f3t l\u00e0 t\u1ed1t nh\u01b0ng kh\u00f4ng th\u01b0\u1edfng kill)
      }
    } else if (!agentAlive) {
      bonus -= 500.0f;
    } else if (agentAlive && enemyAlive && stepsTaken >= maxSteps - 2) {
      bonus -= 500.0f; // H\u00f2a = thua
    }
    return bonus;
  }

  // \u2500\u2500 PHASE 1, 2, 3: Dense step reward \u0111\u00e3 gi\u1ea3m \u2192 end bonus v\u1eabn th\u1ed1ng tr\u1ecb \u2500\u2500
  if (agentWin) {
    if (!agentKill && phase >= Phase::PHASE3) {
      // T\u1eeb Phase 3, n\u1ebfu k\u1ebb \u0111\u1ecbch t\u1ef1 ch\u1ebft (bullet bounce), kh\u00f4ng th\u01b0\u1edfng \u0111i\u1ec3m Th\u1eafng to.
      // Tr\u00e1nh \u0111\u1ec3 agent "\u0103n may" m\u00e0 pass ng\u01b0\u1ee1ng
      bonus += 100.0f;
    } else {
      // Th\u1eafng nhanh \u2192 bonus cao h\u01a1n (khuy\u1ebfn kh\u00edch di\u1ec7t \u0111\u1ecbch s\u1edbm)
      bonus += 400.0f + timeRatio * 300.0f; // Max 700 pts (t\u0103ng t\u1eeb 600)
      if (agentDidShoot)
        bonus += 50.0f; // Bonus nh\u1ecf: x\u00e1c nh\u1eadn \u0111\u00e2y l\u00e0 kill th\u1eadt, kh\u00f4ng ph\u1ea3i do map
    }
  } else if (agentAlive) {
    if (enemyAlive && stepsTaken >= maxSteps - 2) {
      // Hòa
      if (phase == Phase::PHASE3 && !agentDidShoot) {
        bonus -= 500.0f; // Rùa rụt cổ hoàn toàn
      } else {
        bonus -= 250.0f; // Chiến đấu nhưng chưa giết được — giảm từ -300
        // Lý do giảm: với step reward đã bị giảm, -300 quá harsh,
        // agent sẽ sợ engagement hơn sợ không thắng.
      }
    } else {
      // Enemy chết do nguyên nhân khác (đạn bounce, ngoại cảnh)
      bonus += 80.0f; // Giảm từ 100: sống sót do may mắn không nên được thưởng nhiều
    }
  } else {
    // Chết
    // Thang: Thắng(+400~700) > Hòa chiến đấu(-250) > Chết(-400) > Rùa(-500)
    bonus -= 400.0f;
  }

  return bonus;
}