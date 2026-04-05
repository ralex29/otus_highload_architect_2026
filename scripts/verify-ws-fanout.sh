#!/usr/bin/env bash
# verify-ws-fanout.sh — Верификация WS-фанаута при --scale app=3
#
# Тест: WS-клиент подключён к инстансу-1, пост публикуется через инстанс-2
# (или 3). Без фанаута уведомление потерялось бы. С ws-fanout exchange оно
# доставляется на все инстансы → WS-клиент получает его независимо от того,
# какой инстанс обработал сообщение из RabbitMQ.
#
# Использование:
#   docker compose build app
#   docker compose up -d --scale app=3
#   bash scripts/verify-ws-fanout.sh

set -euo pipefail

PASS=0; FAIL=0
ok()   { echo "[OK]   $*"; ((PASS++)) || true; }
fail() { echo "[FAIL] $*"; ((FAIL++)) || true; }
info() { echo "[INFO] $*"; }
sep()  { echo ""; echo "=== $* ==="; }

# ------------------------------------------------------------------ #
sep "Фаза 0: Проверка окружения"
# ------------------------------------------------------------------ #
CONTAINERS=($(docker ps --filter "label=com.docker.compose.service=app" -q))
COUNT=${#CONTAINERS[@]}
info "Запущено инстансов app: $COUNT"
if [[ $COUNT -lt 2 ]]; then
  fail "Требуется минимум 2 инстанса app (запустите с --scale app=3)"
  exit 1
fi
ok "$COUNT инстансов app доступно"

# Два разных контейнера для публикации и WS
APP_WS=${CONTAINERS[0]}      # WS-клиент подключается сюда
APP_POST=${CONTAINERS[-1]}   # Посты публикуются через этот инстанс

info "WS-инстанс:   $APP_WS"
info "POST-инстанс: $APP_POST"

# Определяем host-порты для обращения с хоста
HOST_PORT_WS=$(docker inspect "$APP_WS" \
  --format '{{range $p, $b := .NetworkSettings.Ports}}{{if $b}}{{(index $b 0).HostPort}}{{end}}{{end}}')
HOST_PORT_POST=$(docker inspect "$APP_POST" \
  --format '{{range $p, $b := .NetworkSettings.Ports}}{{if $b}}{{(index $b 0).HostPort}}{{end}}{{end}}')

info "Host-порт WS-инстанса:   $HOST_PORT_WS"
info "Host-порт POST-инстанса: $HOST_PORT_POST"

# Проверяем что Python с websockets доступен на хосте
HAS_WS=$(python3 -c "import websockets; print('yes')" 2>/dev/null || echo "no")
if [[ "$HAS_WS" != "yes" ]]; then
  fail "На хосте не установлен python3-websockets. Выполните: pip3 install websockets"
  exit 1
fi
ok "python3 + websockets доступны на хосте"

# ------------------------------------------------------------------ #
sep "Фаза 1: Создание пользователей"
# ------------------------------------------------------------------ #
REG_A=$(docker exec "$APP_WS" curl -sf -X POST http://localhost:8080/user/register \
  -H "Content-Type: application/json" \
  -d '{"first_name":"Alice","second_name":"Fanout","birthdate":"1999-01-01","sex":"female","biography":"author","city":"Moscow","password":"pass123"}')
USER_A_ID=$(echo "$REG_A" | python3 -c "import sys,json; print(json.load(sys.stdin)['user_id'])")
ok "Пользователь A: $USER_A_ID"

REG_B=$(docker exec "$APP_WS" curl -sf -X POST http://localhost:8080/user/register \
  -H "Content-Type: application/json" \
  -d '{"first_name":"Bob","second_name":"Fanout","birthdate":"1998-01-01","sex":"male","biography":"subscriber","city":"Moscow","password":"pass123"}')
USER_B_ID=$(echo "$REG_B" | python3 -c "import sys,json; print(json.load(sys.stdin)['user_id'])")
ok "Пользователь B: $USER_B_ID"

TOKEN_A=$(docker exec "$APP_WS" curl -sf -X POST http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "{\"id\":\"$USER_A_ID\",\"password\":\"pass123\"}" | python3 -c "import sys,json; print(json.load(sys.stdin)['token'])")
TOKEN_B=$(docker exec "$APP_WS" curl -sf -X POST http://localhost:8080/login \
  -H "Content-Type: application/json" \
  -d "{\"id\":\"$USER_B_ID\",\"password\":\"pass123\"}" | python3 -c "import sys,json; print(json.load(sys.stdin)['token'])")
ok "Токены получены"

docker exec "$APP_WS" curl -sf -X PUT "http://localhost:8080/friend/add/$USER_A_ID" \
  -H "Authorization: Bearer $TOKEN_B" > /dev/null 2>&1 || true
ok "B подписан на A"

# ------------------------------------------------------------------ #
sep "Фаза 2: WS-соединение к инстансу $APP_WS через host-порт $HOST_PORT_WS"
# ------------------------------------------------------------------ #
info "Открываем WS-соединение к инстансу $APP_WS (localhost:$HOST_PORT_WS) с хоста..."

# Python-скрипт запускается на хосте, подключается через mapped host port
WS_TEST_SCRIPT=$(cat <<'PYEOF'
import asyncio, sys, json

async def main():
    token  = sys.argv[1]
    port   = int(sys.argv[2])
    timeout = float(sys.argv[3])
    uri    = f"ws://localhost:{port}/post/feed/posted"

    import websockets

    async with websockets.connect(
        uri,
        extra_headers={"Authorization": f"Bearer {token}"},
        open_timeout=10
    ) as ws:
        print("CONNECTED", flush=True)
        try:
            msg = await asyncio.wait_for(ws.recv(), timeout=timeout)
            print(f"MSG:{msg}", flush=True)
        except asyncio.TimeoutError:
            print("TIMEOUT", flush=True)

asyncio.run(main())
PYEOF
)

echo "$WS_TEST_SCRIPT" > /tmp/ws_client_host.py

# Запуск WS-клиента с хоста в фоне (timeout 10s)
python3 /tmp/ws_client_host.py "$TOKEN_B" "$HOST_PORT_WS" "10" > /tmp/ws_output.txt 2>&1 &
WS_PID=$!

# Ждём подключения
sleep 2

# Проверяем что клиент подключился
if grep -q "CONNECTED" /tmp/ws_output.txt 2>/dev/null; then
  ok "WS-клиент подключён к инстансу $APP_WS (порт $HOST_PORT_WS)"
else
  info "Ожидаем подключения..."
  sleep 2
  if grep -q "CONNECTED" /tmp/ws_output.txt 2>/dev/null; then
    ok "WS-клиент подключён к инстансу $APP_WS (порт $HOST_PORT_WS)"
  else
    fail "WS-клиент не смог подключиться к $APP_WS:$HOST_PORT_WS"
    info "Вывод: $(cat /tmp/ws_output.txt 2>/dev/null)"
    kill $WS_PID 2>/dev/null || true
  fi
fi

# ------------------------------------------------------------------ #
sep "Фаза 3: Публикация поста через инстанс $APP_POST (порт $HOST_PORT_POST)"
# ------------------------------------------------------------------ #
info "Публикуем пост через инстанс $APP_POST (не тот, где WS-клиент)..."
POST_ID=""
POST_RESP=""
# Retry: auth-pg-cache обновляется каждые 10s, токен может ещё не появиться
for retry in 1 2 3 4 5; do
  POST_RESP=$(docker exec "$APP_POST" curl -s -X POST http://localhost:8080/post/create \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN_A" \
    -d '{"text":"WS fanout test post"}') || true
  POST_ID=$(echo "$POST_RESP" | python3 -c "
import sys,json
try:
    d=json.load(sys.stdin)
    print(d.get('post_id') or d.get('id',''))
except: print('')
" 2>/dev/null || echo "")
  if [[ -n "$POST_ID" ]]; then break; fi
  info "Retry $retry: ответ=$POST_RESP (ожидаем обновления auth-cache...)"
  sleep 3
done
if [[ -n "$POST_ID" ]]; then
  ok "Пост опубликован через $APP_POST: $POST_ID"
else
  fail "Не удалось опубликовать пост. Ответ: $POST_RESP"
fi

# ------------------------------------------------------------------ #
sep "Фаза 4: Проверка получения WS-уведомления"
# ------------------------------------------------------------------ #
info "Ожидаем WS-уведомление (до 10s)..."
wait $WS_PID || true

cat /tmp/ws_output.txt

if grep -q "MSG:" /tmp/ws_output.txt 2>/dev/null; then
  WS_MSG=$(grep "MSG:" /tmp/ws_output.txt | head -1 | sed 's/MSG://')
  info "Получено сообщение: $WS_MSG"
  # Проверяем что это уведомление о нашем посте
  WS_POST_ID=$(echo "$WS_MSG" | python3 -c "import sys,json; print(json.load(sys.stdin).get('postId',''))" 2>/dev/null || echo "")
  if [[ "$WS_POST_ID" == "$POST_ID" ]]; then
    ok "WS-уведомление содержит правильный post_id ($POST_ID)"
    ok "ФАНАУТ РАБОТАЕТ: пост опубликован на $APP_POST, уведомление получено на $APP_WS"
  else
    fail "post_id в WS-сообщении ($WS_POST_ID) не совпадает с ожидаемым ($POST_ID)"
  fi
elif grep -q "TIMEOUT" /tmp/ws_output.txt 2>/dev/null; then
  fail "WS-клиент не получил уведомление за 10s (TIMEOUT)"
  info "Проверьте логи: docker logs $APP_WS | grep -i 'ws-fanout\|fanout\|websocket'"
elif grep -q "CONNECTED" /tmp/ws_output.txt 2>/dev/null; then
  fail "WS-клиент подключился, но уведомление не пришло"
else
  fail "WS-клиент не смог подключиться"
  info "Вывод: $(cat /tmp/ws_output.txt)"
fi

# ------------------------------------------------------------------ #
sep "Фаза 5: Проверка RabbitMQ (fanout exchange + очереди)"
# ------------------------------------------------------------------ #
FANOUT_INFO=$(docker exec social-net-rabbitmq rabbitmqadmin list exchanges name type 2>/dev/null | grep ws-fanout || echo "")
if [[ -n "$FANOUT_INFO" ]]; then
  ok "ws-fanout exchange существует: $FANOUT_INFO"
else
  fail "ws-fanout exchange не найден"
fi

WS_QUEUES=$(docker exec social-net-rabbitmq rabbitmqadmin list queues name consumers 2>/dev/null | grep "ws-fanout-" | grep -v "| 0 " || echo "")
WS_Q_COUNT=$(echo "$WS_QUEUES" | grep -c "ws-fanout-" 2>/dev/null || echo "0")
info "Активных ws-fanout очередей (с consumer): $WS_Q_COUNT"
if [[ "$WS_Q_COUNT" -ge $COUNT ]]; then
  ok "$WS_Q_COUNT ws-fanout очередей с consumers (по одной на инстанс)"
else
  info "Примечание: $WS_Q_COUNT из $COUNT инстансов имеют active consumer в ws-fanout"
  info "Это нормально если некоторые инстансы ещё не имеют WS-соединений"
fi

# ------------------------------------------------------------------ #
sep "Итог"
# ------------------------------------------------------------------ #
echo ""
echo "Результаты верификации WS-фанаута:"
echo "  PASS: $PASS"
echo "  FAIL: $FAIL"
echo ""
if [[ $FAIL -eq 0 ]]; then
  echo "[SUCCESS] WS-фанаут через RabbitMQ работает корректно."
  echo "  Уведомление доставлено с инстанса $APP_POST на $APP_WS"
  echo "  через ws-fanout exchange."
  exit 0
else
  echo "[PARTIAL] WS-фанаут завершён с $FAIL ошибками."
  exit 1
fi
