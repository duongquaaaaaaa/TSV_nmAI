#pragma once
#include "map.h"
#include <string>

// ── Enemy behaviour during training ──────────────────────────────────────────
enum class EnemyType {
  STATIONARY,
  RANDOM,
  RULE_V1,
  RULE_V2,
  RULE_V3,
  SELF_PLAY
};

// ── Curriculum phase ID
// ───────────────────────────────────────────────────────
enum class Phase { PHASE1 = 1, PHASE2, PHASE3, PHASE4, PHASE5 };

struct PhaseConfig {
  Phase phase;
  MapMode mapMode;
  EnemyType enemyType;
  int maxSteps;
  int kSeeds;
  int maxGenerations;
  float promotionThreshold;
  bool itemsEnabled;
  bool portalsEnabled;
  bool shieldsEnabled;
  float bulletLifespan;
  int maxBullets;
  int streakRequired;
  float leagueRate;
  std::string name;
};

// ═════════════════════════════════════════════════════════════════════════════
// CURRICULUM DESIGN NOTES (TÍNH TOÁN NGƯỠNG THRESHOLD)
// ═════════════════════════════════════════════════════════════════════════════
//
// === REWARD BUDGET SAU FIX VĐ #6 ===
//   End bonus Win  (kill nhanh):  +400 ~ +750  (trung bình ~550)
//   End bonus Loss:              -400
//   End bonus Draw:              -250
//   Step reward per episode:     [-450 .. +600] (clamped)
//   Complexity penalty:          ~-0.4 pts (216 conns * 0.002)
//
// === FITNESS KỲ VỌNG CHO CÁC KỊCH BẢN ===
//   Agent thắng tất cả seeds (hoàn hảo):  ~500-600 pts trung bình
//   Agent thắng 60% seeds:                 ~200-300 pts trung bình
//   Agent thắng 40% seeds:                 ~50-100 pts trung bình
//   Agent chỉ hòa/thua:                    ~-200 đến -400 pts
//
// Threshold phải ĐẶT Ở MỨC "ĐỦ TỐT" chứ không phải "HOÀN HẢO".
// Nếu threshold quá cao, agent sẽ không bao giờ pass → training stuck.
// ═════════════════════════════════════════════════════════════════════════════

inline PhaseConfig GetPhaseConfig(Phase phase) {
  switch (phase) {
  case Phase::PHASE1:
    // MỤC TIÊU: Học di chuyển + bắn mục tiêu cố định trên map trống.
    // Đây là bước đơn giản nhất, agent chỉ cần tìm và tiêu diệt bao cát.
    return {
        Phase::PHASE1,
        MapMode::SPARSE,       // Map trống, ít tường → dễ tìm đường
        EnemyType::STATIONARY, // Đối thủ đứng yên → chỉ cần học aim
        1200,                  // 20 giây/ván (1200 steps * 1/60)
        5,                     // 5 seeds — map SPARSE ít variance
        300,                   // 500→300 gen: bias mutation giúp hội tụ nhanh hơn
        250.0f,                // ~50% winrate. Thấp để agent pass Phase 1 sớm
                               // và bắt đầu học kỹ năng phức tạp hơn.
        false, false, false,   // Tắt items/portals/shields
        2.5f,                  // Đạn tồn tại 2.5s
        3,                     // Max 3 viên đạn
        5,                     // Streak 5 gen (10→5: Phase 1 nên pass nhanh)
        0.0f,                  // Không trộn đối thủ
        "Phase1_Basic"
    };

  case Phase::PHASE2:
    // MỤC TIÊU: Học lách mê cung + truy đuổi mục tiêu di động.
    // Bước nhảy: SPARSE→NORMAL map + STATIONARY→Wanderer enemy.
    return {
        Phase::PHASE2,
        MapMode::NORMAL,       // Mê cung đầy đủ → phải dùng A* navigation
        EnemyType::RULE_V1,    // Wanderer: chỉ đi lang thang, KHÔNG bắn
        1500,                  // 25 giây/ván
        8,                     // 8 seeds — NORMAL map variance cao hơn
        400,                   // Nhiều gen hơn vì nhảy 2 bậc độ khó
        250.0f,                // ~50% winrate trên 8 seeds. Wanderer không bắn
                               // nên agent chỉ cần tìm đường + aim → 250 hợp lý.
        false, false, false,
        2.5f,
        3,
        7,                     // Streak 7 gen
        0.20f,                 // 20% trộn STATIONARY để ôn Phase 1
        "Phase2_Wanderer"
    };

  case Phase::PHASE3:
    // MỤC TIÊU: Học chiến đấu thật sự — enemy bắn trả!
    // Bước nhảy: Enemy bắn nghiệp dư (V2). Agent phải dodge + aim + shoot.
    return {
        Phase::PHASE3,
        MapMode::NORMAL,
        EnemyType::RULE_V2,    // Fighter: A* hunt + bắn khi thấy (ngắm lệch)
        1500,
        10,                    // 10 seeds — enemy bắn tạo variance cao
        400,                   // Cần nhiều gen vì kỹ năng mới (dodge + shoot)
        200.0f,                // ~40% winrate. V2 bắn trả có thể giết agent,
                               // nên fitness trung bình thấp hơn nhiều.
                               // 200 = thắng 4/10 seeds hoặc 3W + vài hòa chiến đấu.
        false, false, false,
        3.5f,                  // Đạn sống lâu hơn → đạn bounce nguy hiểm hơn
        3,
        7,                     // Streak 7
        0.25f,                 // 25% trộn V1 để ôn navigation
        "Phase3_Fighter"
    };

  case Phase::PHASE4:
    // MỤC TIÊU: Đánh bại Sniper Boss — enemy ngắm chuẩn, dừng xe bắn.
    // Pure sparse reward — agent phải tự phát triển chiến thuật.
    return {
        Phase::PHASE4,
        MapMode::NORMAL,
        EnemyType::RULE_V3,    // Sniper Boss: A* + stop-and-shoot + aim chuẩn
        1500,
        12,                    // 12 seeds — V3 rất khó, cần nhiều seed giảm variance
        400,
        200.0f,                // V3 cực mạnh. 200 = thắng ~35% seeds (4-5/12).
                               // Đủ chứng minh agent có thể đánh bại sniper.
        false, false, false,
        7.0f,                  // Đạn sống 7s — tăng risk từ bounce
        3,
        10,                    // Streak 10 gen — sparse reward variance cao
        0.40f,                 // 40% trộn V2 để ôn bài chiến đấu
        "Phase4_SniperBoss"
    };

  case Phase::PHASE5:
    // MỤC TIÊU: Self-play tournament — đánh chính mình.
    // Agent phải generalize, không chỉ exploit weakness của rule bot.
    return {
        Phase::PHASE5,
        MapMode::NORMAL,
        EnemyType::SELF_PLAY,  // Đối thủ: bản sao best genome cũ
        1500,
        10,                    // 10 seeds
        500,                   // Nhiều gen — self-play cần thời gian co-evolve
        150.0f,                // Self-play 50/50 → avg fitness ~0 (thắng = thua).
                               // 150 = nhỉnh hơn trung bình = agent đang tốt hơn
                               // phiên bản cũ của chính nó.
        false, false, false,
        7.0f,
        3,
        10,                    // Streak 10
        0.40f,                 // 40% trộn V3 để giữ kỷ luật Sniper
        "Phase5_Tournament"
    };

  default:
    return GetPhaseConfig(Phase::PHASE1);
  }
}
