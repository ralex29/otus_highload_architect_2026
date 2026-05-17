#!/bin/sh
set -e
REDIS_HOST="${REDIS_HOST:-redis}"
REDIS_PORT="${REDIS_PORT:-6379}"

redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" \
    FUNCTION LOAD REPLACE "$(cat /scripts/dialog_functions.lua)"
echo "[OK] dialog_send, dialog_list loaded"

redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" \
    FUNCTION LOAD REPLACE "$(cat /scripts/counter_functions.lua)"
echo "[OK] counter_reset_dialog, counter_reconcile_user loaded"
