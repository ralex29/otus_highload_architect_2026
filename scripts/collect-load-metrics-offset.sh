#!/usr/bin/env bash
# Собирает RED-метрики chat-service за конкретное прошедшее окно нагрузки.
# Запрос вычисляется В МОМЕНТ конца окна (параметр Prometheus time=), поэтому
# rate()/max_over_time() усредняются именно по интервалу прогона, а не "от now".
#
# Использование: collect-load-metrics-offset.sh <label> <end_epoch> <window_seconds>
# Пример:        collect-load-metrics-offset.sh high 1779606577 470

set -uo pipefail

LABEL="${1:?label required}"
END="${2:?end epoch required}"
WIN="${3:?window seconds required}"
W="${WIN}s"
PROM="http://localhost:9090/api/v1/query"

q() {
  local Q="$1"
  curl -sG --data-urlencode "query=$Q" --data-urlencode "time=$END" "$PROM" 2>/dev/null | \
    python3 -c "import sys,json
try:
    d=json.load(sys.stdin); r=d.get('data',{}).get('result',[])
    print(f\"{float(r[0]['value'][1]):.2f}\" if r else 'n/a')
except Exception:
    print('n/a')"
}

S="service=\"chat-service\""
RPS_SEND=$(q "sum(rate(grpc_server_by_destination_rps{$S,grpc_method=\"SendMessage\"}[$W]))")
RPS_LIST=$(q "sum(rate(grpc_server_by_destination_rps{$S,grpc_method=\"ListMessages\"}[$W]))")
RPS_TOTAL=$(q "sum(rate(grpc_server_by_destination_rps{$S}[$W]))")
EPS_TOTAL=$(q "sum(rate(grpc_server_by_destination_eps{$S}[$W]))")
ERR_PCT=$(q "100 * sum(rate(grpc_server_by_destination_eps{$S}[$W])) / clamp_min(sum(rate(grpc_server_by_destination_rps{$S}[$W])), 1)")
P95_SEND=$(q "max(max_over_time(grpc_server_by_destination_timings{$S,grpc_method=\"SendMessage\",percentile=\"p95\"}[$W]))")
P99_SEND=$(q "max(max_over_time(grpc_server_by_destination_timings{$S,grpc_method=\"SendMessage\",percentile=\"p99\"}[$W]))")
P95_LIST=$(q "max(max_over_time(grpc_server_by_destination_timings{$S,grpc_method=\"ListMessages\",percentile=\"p95\"}[$W]))")
P99_LIST=$(q "max(max_over_time(grpc_server_by_destination_timings{$S,grpc_method=\"ListMessages\",percentile=\"p99\"}[$W]))")
ACTIVE=$(q "max(max_over_time(grpc_server_by_destination_active{$S}[$W]))")
CPU=$(q "max(rate(container_cpu_usage_seconds_total{name=\"social-net-chat-service\"}[$W])) * 100")
MEM=$(q "max(max_over_time(container_memory_working_set_bytes{name=\"social-net-chat-service\"}[$W])) / 1024 / 1024")

# Подробный блок (в metrics-<label>.txt)
cat <<EOF
ПРОФИЛЬ НАГРУЗКИ: $LABEL (окно: $W, конец: $(date -d "@$END" '+%H:%M:%S'))
  Avg RPS  SendMessage   : $RPS_SEND  req/s
  Avg RPS  ListMessages  : $RPS_LIST  req/s
  Total RPS (все методы) : $RPS_TOTAL req/s
  Error rate             : $ERR_PCT %  (EPS=$EPS_TOTAL)
  p95 / p99 SendMessage  : $P95_SEND / $P99_SEND ms
  p95 / p99 ListMessages : $P95_LIST / $P99_LIST ms
  Max active RPCs        : $ACTIVE
  Container CPU peak      : $CPU %
  Container MEM peak      : $MEM MiB
EOF

# Строка для сводной markdown-таблицы (в metrics-summary.txt) на stderr,
# чтобы не смешивать с подробным блоком
printf '| %-6s | %s | %s | %s | %s | %s | %s | %s | %s |\n' \
  "$LABEL" "$RPS_SEND" "$RPS_LIST" "$P95_SEND" "$P99_SEND" "$P99_LIST" "$ERR_PCT" "$CPU" "$ACTIVE" >&2
