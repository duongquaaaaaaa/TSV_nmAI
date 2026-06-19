#pragma once
#include "bullet.h"
#include "constants.h"
#include "item.h"

/**
 * @class Tank
 * @brief Xe tăng do người chơi hoặc AI điều khiển. Logic thuần, không phụ thuộc
 * đồ họa. Nhận TankActions thay vì đọc phím trực tiếp → dùng được cho cả human
 * play và RL.
 */
class Tank {
public:
    b2Body* body;               ///< Thân vật lý Box2D
    int playerIndex;            ///< Số thứ tự người chơi (0-3)
    float shootCooldownTimer;   ///< Đếm lùi giữa các lần bắn
    bool isDestroyed;           ///< Cờ xe tăng đã chết
    int hp = 1;                 ///< Máu của xe tăng (RL dùng hp=1)
    int lastHitBy = -1;         ///< Index người chơi đã bắn viên đạn cuối cùng trúng xe này (-1 nếu chưa ai bắn)

  ItemType currentWeapon; ///< Vũ khí đặc biệt đang trang bị
  int ammo;               ///< Đạn còn lại của vũ khí đặc biệt

  bool hasShield;            ///< Trạng thái khiên
  float shieldTimer;         ///< Thời gian tồn tại khiên
  float shieldCooldownTimer; ///< Thời gian chờ kích hoạt lại khiên

    Tank(b2World& world, int _playerIndex);
    void Update(b2World& world, std::vector<Bullet*>& bullets, std::vector<Item*>& items, const TankActions& actions, float dt, bool shieldsEnabled = true, float bulletLifespan = 7.0f, int maxBullets = 3);

private:
    void HandleMovement(const TankActions& actions);
    void FireWeapon(b2World& world, std::vector<Bullet*>& bullets, const TankActions& actions, float bulletLifespan = 7.0f, int maxBullets = 3);
    void CheckCollisions(std::vector<Bullet*>& bullets, std::vector<Item*>& items);
};