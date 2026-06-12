#!/usr/bin/env bash
# Запрашивает в Prometheus агрегированные RED-метрики chat-service за заданное окно
# и печатает таблицу. Использование: collect-load-metrics.sh <label> <duration>
# Пример: collect-load-metrics.sh medium 5m

LABEL="${1:?label required (low/medium/high)}"
WINDOW="${2:-5m}"

PROM="http://localhost:9090/api/v1/query"

q() {
  local Q="$1"
  curl -sG --data-urlencode "query=$Q" "$PROM" | \
    python3 -c "import sys,json; d=json.load(sys.stdin); r=d['data']['result']; print(f\"{float(r[0]['value'][1]):.2f}\" if r else 'n/a')"
}

# Rate (Send / List)
RPS_SEND=$(q "sum(rate(grpc_server_by_destination_rps{service=\"chat-service\",grpc_method=\"SendMessage\"}[$WINDOW]))")
RPS_LIST=$(q "sum(rate(grpc_server_by_destination_rps{service=\"chat-service\",grpc_method=\"ListMessages\"}[$WINDOW]))")

# Errors
EPS_TOTAL=$(q "sum(rate(grpc_server_by_destination_eps{service=\"chat-service\"}[$WINDOW]))")
RPS_TOTAL=$(q "sum(rate(grpc_server_by_destination_rps{service=\"chat-service\"}[$WINDOW]))")
ERR_PCT=$(q "100 * sum(rate(grpc_server_by_destination_eps{service=\"chat-service\"}[$WINDOW])) / clamp_min(sum(rate(grpc_server_by_destination_rps{service=\"chat-service\"}[$WINDOW])), 1)")

# Duration p99
P99_SEND=$(q "max(max_over_time(grpc_server_by_destination_timings{service=\"chat-service\",grpc_method=\"SendMessage\",percentile=\"p99\"}[$WINDOW]))")
P99_LIST=$(q "max(max_over_time(grpc_server_by_destination_timings{service=\"chat-service\",grpc_method=\"ListMessages\",percentile=\"p99\"}[$WINDOW]))")

# Active RPCs
ACTIVE=$(q "max(max_over_time(grpc_server_by_destination_active{service=\"chat-service\"}[$WINDOW]))")

# Container CPU
CPU=$(q "max(rate(container_cpu_usage_seconds_total{name=\"social-net-chat-service\"}[$WINDOW])) * 100")

cat <<EOF
LOAD PROFILE: $LABEL (window: $WINDOW)
  Avg RPS  SendMessage   : $RPS_SEND  req/s
  Avg RPS  ListMessages  : $RPS_LIST  req/s
  Total RPS (all methods): $RPS_TOTAL req/s
  Error rate            : $ERR_PCT %  (EPS=$EPS_TOTAL)
  p99 SendMessage       : $P99_SEND ms
  p99 ListMessages      : $P99_LIST ms
  Max active RPCs       : $ACTIVE
  Container CPU peak    : $CPU %
EOF
