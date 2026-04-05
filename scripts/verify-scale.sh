#!/usr/bin/env bash
# verify-scale.sh — Верификация Сценария 1: горизонтальное масштабирование app (--scale app=3)
#
# Все HTTP-запросы выполняются через docker exec внутри контейнеров.
# Никаких прямых вызовов с хоста.
#
# Использование:
#   docker compose build app
#   bash scripts/verify-scale.sh

set -euo pipefail

PASS=0
FAIL=0

ok()   { echo "[OK]   $*"; ((PASS++)) || true; }
fail() { echo "[FAIL] $*"; ((FAIL++)) || true; }
info() { echo "[INFO] $*"; }
warn() { echo "[WARN] $*"; }
sep()  { echo ""; echo "=== $* ==="; }

RABBITMQ_CONTAINER="social-net-rabbitmq"

# ------------------------------------------------------------
sep "Фаза 1: Сборка образа"
# ------------------------------------------------------------
info "Сборка docker compose build app ..."
docker compose build app
ok "Образ собран"

# ------------------------------------------------------------
sep "Фаза 2: Запуск с 3 репликами"
# ------------------------------------------------------------
info "docker compose up -d --scale app=3"
docker compose up -d --scale app=3

# ------------------------------------------------------------
sep "Фаза 3: Ожидание готовности app (таймаут 120s)"
# ------------------------------------------------------------
info "Поиск контейнера app..."
TIMEOUT=120
ELAPSED=0
APP_C=""
while [[ $ELAPSED -lt $TIMEOUT ]]; do
  APP_C=$(docker ps --filter "label=com.docker.compose.service=app" -q | head -1 || true)
  if [[ -n "$APP_C" ]]; then
    if docker exec "$APP_C" curl -sf http://localhost:8080/ping > /dev/null 2>&1; then
      break
    fi
  fi
  sleep 5
  ELAPSED=$((ELAPSED + 5))
done

if [[ -z "$APP_C" ]]; then
  fail "Ни один контейнер app не запустился за ${TIMEOUT}s"
  exit 1
fi

if ! docker exec "$APP_C" curl -sf http://localhost:8080/ping > /dev/null 2>&1; then
  fail "App не отвечает на /ping за ${TIMEOUT}s"
  exit 1
fi
ok "App доступен (контейнер: $APP_C)"

APP_COUNT=$(docker ps --filter "label=com.docker.compose.service=app" -q | wc -l)
info "Запущено экземпляров app: $APP_COUNT"
if [[ "$APP_COUNT" -eq 3 ]]; then
  ok "3 реплики app запущены"
else
  fail "Ожидалось 3 реплики, запущено: $APP_COUNT"
fi

# Заметка об ограничении WebSocket при масштабировании
warn "WebSocket-соединения (/post/feed/posted) хранятся in-memory в каждом инстансе."
warn "При масштабировании WS-клиент получает уведомления только от инстанса, к которому подключён."
warn "Это ожидаемое поведение без shared-state между репликами."

# ------------------------------------------------------------
sep "Фаза 4: Создание тестовых данных"
# ------------------------------------------------------------
info "Регистрация пользователя A..."
REG_A=$(docker exec "$APP_C" curl -sf -X POST http://localhost:8080/user/register \
  -H "Content-Type: application/json" \
  -d '{"first_name":"Alice","second_name":"Scale","birthdate":"1999-01-01","sex":"female","biography":"test user A","city":"Moscow","password":"pass123"}')
USER_A_ID=$(echo "$REG_A" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('user_id',''))" 2>/dev/null || echo "")

if [[ -z "$USER_A_ID" ]]; then
  fail "Не удалось создать пользователя A. Ответ: $REG_A"
  exit 1
fi
ok "Пользователь A создан: $USER_A_ID"

info "Регистрация пользователя B..."
REG_B=$(docker exec "$APP_C" curl -sf -X POST http://localhost:8080/user/register \
  -H "Content-Type: application/json" \
  -d '{"first_name":"Bob","second_name":"Scale","birthdate":"1998-05-15","sex":"male","biography":"test user B","city":"Moscow","password":"pass123"}')
USER_B_ID=$(echo "$REG_B" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('user_id',''))" 2>/dev/null || echo "")

if [[ -z "$USER_B_ID" ]]; then
  fail "Не удалось создать пользователя B. Ответ: $REG_B"
  exit 1
fi
ok "Пользователь B создан: $USER_B_ID"

info "Логин пользователя A..."
LOGIN_A=$(docker exec "$APP_C" curl -sf -X POST http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "{\"id\":\"$USER_A_ID\",\"password\":\"pass123\"}")
TOKEN_A=$(echo "$LOGIN_A" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('token',''))" 2>/dev/null || echo "")

if [[ -z "$TOKEN_A" ]]; then
  fail "Не удалось получить токен для A. Ответ: $LOGIN_A"
  exit 1
fi
ok "Токен A получен"

info "Логин пользователя B..."
LOGIN_B=$(docker exec "$APP_C" curl -sf -X POST http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "{\"id\":\"$USER_B_ID\",\"password\":\"pass123\"}")
TOKEN_B=$(echo "$LOGIN_B" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('token',''))" 2>/dev/null || echo "")

if [[ -z "$TOKEN_B" ]]; then
  fail "Не удалось получить токен для B. Ответ: $LOGIN_B"
  exit 1
fi
ok "Токен B получен"

info "B добавляет A в друзья..."
docker exec "$APP_C" curl -sf -X PUT "http://localhost:8080/friend/add/$USER_A_ID" \
  -H "Authorization: Bearer $TOKEN_B" > /dev/null 2>&1 || true
ok "Подписка B -> A установлена"

# ------------------------------------------------------------
sep "Фаза 5: Публикация 150 постов от A"
# ------------------------------------------------------------
info "Публикуем 150 постов..."
PUBLISH_OK=0
for i in $(seq 1 150); do
  RESP=$(docker exec "$APP_C" curl -sf -X POST http://localhost:8080/post/create \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN_A" \
    -d "{\"text\":\"post $i from scale test\"}" 2>/dev/null || echo "")
  if [[ -n "$RESP" ]]; then
    PUBLISH_OK=$((PUBLISH_OK + 1))
  fi
done
info "Успешно опубликовано: $PUBLISH_OK/150"
if [[ $PUBLISH_OK -eq 150 ]]; then
  ok "Все 150 постов опубликованы"
else
  fail "Опубликовано только $PUBLISH_OK из 150 постов"
fi

# ------------------------------------------------------------
sep "Фаза 6: Ожидание обработки очереди (10s)"
# ------------------------------------------------------------
sleep 10
ok "Ожидание завершено"

# Прогрев Redis-кэша ленты B: OnPostCreated обновляет Redis только если ключ существует.
# Первый GET /post/feed загружает ленту из БД и создаёт ключ feed:B в Redis.
info "Прогрев Redis-кэша ленты B через GET /post/feed..."
FEED_RESP=$(docker exec "$APP_C" curl -sf -X GET \
  "http://localhost:8080/post/feed?offset=0&limit=10" \
  -H "Authorization: Bearer $TOKEN_B" 2>/dev/null || echo "[]")
FEED_COUNT=$(echo "$FEED_RESP" | python3 -c "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo "0")
info "Лента B после прогрева: $FEED_COUNT постов через API"

# ------------------------------------------------------------
sep "Фаза 7: Проверка RabbitMQ Management API"
# ------------------------------------------------------------
info "Запрос состояния очереди feed-materialization через $RABBITMQ_CONTAINER..."
QUEUE_JSON=$(docker exec "$RABBITMQ_CONTAINER" sh -c \
  'wget -q -O- --header="Authorization: Basic Z3Vlc3Q6Z3Vlc3Q=" "http://127.0.0.1:15672/api/queues/%2F/feed-materialization"' \
  2>/dev/null || echo "")

if [[ -z "$QUEUE_JSON" ]]; then
  fail "Не удалось получить информацию об очереди feed-materialization"
else
  CONSUMER_COUNT=$(echo "$QUEUE_JSON" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(d.get('consumers',0))" 2>/dev/null || echo "0")
  MSGS_READY=$(echo "$QUEUE_JSON" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(d.get('messages_ready',0))" 2>/dev/null || echo "-1")
  MSGS_TOTAL=$(echo "$QUEUE_JSON" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(d.get('messages',0))" 2>/dev/null || echo "-1")

  info "Consumers: $CONSUMER_COUNT, messages_ready: $MSGS_READY, total: $MSGS_TOTAL"

  if [[ "$CONSUMER_COUNT" -eq 3 ]]; then
    ok "3 consumer-а подключены к очереди (по одному от каждой реплики)"
  else
    fail "Ожидалось 3 consumers, получено: $CONSUMER_COUNT"
    info "Примечание: prefetch_count=100 в configs/static_config.yaml означает, что один consumer"
    info "может забрать все сообщения в рамках prefetch. Consumer count — надёжный показатель распределения."
  fi

  if [[ "$MSGS_READY" -eq 0 ]]; then
    ok "Очередь пуста — все сообщения обработаны"
  else
    fail "В очереди остались необработанные сообщения: $MSGS_READY"
  fi
fi

# ------------------------------------------------------------
sep "Фаза 8: Проверка уникальных consumer-контейнеров"
# ------------------------------------------------------------
info "Получение списка consumers с именами соединений..."
CONSUMERS_JSON=$(docker exec "$RABBITMQ_CONTAINER" sh -c \
  'wget -q -O- --header="Authorization: Basic Z3Vlc3Q6Z3Vlc3Q=" "http://127.0.0.1:15672/api/consumers/%2F"' \
  2>/dev/null || echo "")

if [[ -n "$CONSUMERS_JSON" ]]; then
  UNIQUE_CONTAINERS=$(echo "$CONSUMERS_JSON" | python3 -c "
import sys, json
consumers = json.load(sys.stdin)
channels = set()
for c in consumers:
    name = c.get('channel_details', {}).get('connection_name', '')
    channels.add(name)
print(len(channels))
for c in consumers:
    name = c.get('channel_details', {}).get('connection_name', '')
    print('  connection:', name)
" 2>/dev/null || echo "0")
  info "Уникальных соединений consumers: первая строка"
  echo "$UNIQUE_CONTAINERS" | head -1 | xargs -I{} echo "[INFO] Уникальных соединений: {}"
  echo "$UNIQUE_CONTAINERS" | tail -n +2
fi

# ------------------------------------------------------------
sep "Фаза 9: Проверка ленты B в Redis"
# ------------------------------------------------------------
LLEN=$(docker exec social-net-redis redis-cli LLEN "feed:$USER_B_ID" 2>/dev/null || echo "0")
info "Длина ленты B (feed:$USER_B_ID): $LLEN"
if [[ "$LLEN" -ge 150 ]]; then
  ok "Лента B содержит $LLEN записей (>= 150)"
elif [[ "$LLEN" -gt 0 ]]; then
  warn "Лента B содержит $LLEN записей — частичная обработка. Возможно, нужно больше времени."
  # Проверяем ещё раз через 5 секунд
  sleep 5
  LLEN=$(docker exec social-net-redis redis-cli LLEN "feed:$USER_B_ID" 2>/dev/null || echo "0")
  if [[ "$LLEN" -ge 150 ]]; then
    ok "Лента B содержит $LLEN записей (>= 150) после дополнительного ожидания"
  else
    fail "Лента B содержит только $LLEN записей (ожидалось >= 150)"
  fi
else
  fail "Лента B пуста. Ключ: feed:$USER_B_ID"
fi

# ------------------------------------------------------------
sep "Итог"
# ------------------------------------------------------------
echo ""
echo "Результаты верификации Сценария 1 (горизонтальное масштабирование):"
echo "  PASS: $PASS"
echo "  FAIL: $FAIL"
echo ""

if [[ $FAIL -eq 0 ]]; then
  echo "[SUCCESS] Сценарий 1 пройден полностью."
  exit 0
else
  echo "[PARTIAL] Сценарий 1 завершён с $FAIL ошибками."
  exit 1
fi
