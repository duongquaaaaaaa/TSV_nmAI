#!/bin/bash
# Chuyển về thư mục build của dự án
cd "$(dirname "$0")/build"

# Chạy game ở chế độ RL đấu với Bot
./AZgame --rl-vs-bot
