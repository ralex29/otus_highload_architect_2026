#!/usr/bin/env bash
# load_dialog_functions.sh — загрузить Redis Functions для dialog модуля
# Выполняется через docker exec внутри Redis-контейнера
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LUA_FILE="$SCRIPT_DIR/dialog_functions.lua"

REDIS_C=$(docker ps --filter "label=com.docker.compose.service=redis" -q | head -1)
if [[ -z "$REDIS_C" ]]; then
  echo "[FAIL] Redis container not found (is docker-compose up?)"
  exit 1
fi

docker cp "$LUA_FILE" "$REDIS_C":/tmp/dialog_functions.lua
docker exec "$REDIS_C" sh -c \
  'redis-cli FUNCTION LOAD REPLACE "$(cat /tmp/dialog_functions.lua)"'

echo "[OK] Redis Functions loaded: dialog_send, dialog_list"
docker exec "$REDIS_C" redis-cli FUNCTION LIST
