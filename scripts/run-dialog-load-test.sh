#!/usr/bin/env bash
# run-dialog-load-test.sh — JMeter load test for the Dialog module
#
# Usage:
#   bash scripts/run-dialog-load-test.sh           # containers must be running
#   bash scripts/run-dialog-load-test.sh --start   # start docker-compose first
#
# Results saved to: jmeter/test_dialog/<TIMESTAMP>/
#   results.jtl       — raw JTL data
#   report/index.html — HTML dashboard

set -euo pipefail

PASS=0
FAIL=0

ok()   { echo "[OK]   $*"; ((PASS++)) || true; }
fail() { echo "[FAIL] $*"; ((FAIL++)) || true; }
info() { echo "[INFO] $*"; }
sep()  { echo ""; echo "=== $* ==="; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
JMETER_DIR="$PROJECT_DIR/jmeter"
RESULTS_DIR="$JMETER_DIR/test_dialog/$TIMESTAMP"
DOCKER_NETWORK="social-net-service_social-net-network"
JMETER_IMAGE="justb4/jmeter"

START_COMPOSE=0
for arg in "$@"; do
  [[ "$arg" == "--start" ]] && START_COMPOSE=1
done

# -----------------------------------------------------------------------
sep "Phase 1: Docker Compose startup (optional)"
# -----------------------------------------------------------------------
if [[ $START_COMPOSE -eq 1 ]]; then
  info "Starting docker-compose..."
  docker-compose -f "$PROJECT_DIR/docker-compose.yml" up -d
  ok "docker-compose up -d complete"
else
  info "Skipping startup (pass --start to launch containers)"
fi

# -----------------------------------------------------------------------
sep "Phase 2: Wait for service health (GET /ping, timeout 120s)"
# -----------------------------------------------------------------------
TIMEOUT=120
ELAPSED=0
APP_C=""
while [[ $ELAPSED -lt $TIMEOUT ]]; do
  APP_C=$(docker ps --filter "label=com.docker.compose.service=app" -q 2>/dev/null | head -1 || true)
  if [[ -n "$APP_C" ]]; then
    if docker exec "$APP_C" curl -sf http://localhost:8080/ping > /dev/null 2>&1; then
      break
    fi
  fi
  sleep 5
  ELAPSED=$((ELAPSED + 5))
done

if [[ -z "$APP_C" ]] || ! docker exec "$APP_C" curl -sf http://localhost:8080/ping > /dev/null 2>&1; then
  fail "App did not respond to /ping within ${TIMEOUT}s"
  exit 1
fi
ok "Service is healthy (container: $APP_C)"

# -----------------------------------------------------------------------
sep "Phase 3: Prepare results directory"
# -----------------------------------------------------------------------
mkdir -p "$RESULTS_DIR"
ok "Results directory: $RESULTS_DIR"

# -----------------------------------------------------------------------
sep "Phase 4: Run JMeter load test (~4 min)"
# -----------------------------------------------------------------------
info "Scenarios: Send Load (50 VUs, 60s) → List Load (50 VUs, 60s) → Mixed Load (100 VUs, 120s)"
info "User registration and login are handled inside JMeter (Once Only Controller per VU)"
info "JMeter image: $JMETER_IMAGE"
info "Network: $DOCKER_NETWORK"
echo ""

docker run --rm \
  --network "$DOCKER_NETWORK" \
  -v "$JMETER_DIR":/jmeter \
  -v "$RESULTS_DIR":/results \
  "$JMETER_IMAGE" \
  -n \
  -t /jmeter/dialog_test_plan.jmx \
  -l /results/results.jtl \
  -e -o /results/report \
  -Jhost=app \
  -Jport=8080

JMETER_EXIT=$?
echo ""

if [[ $JMETER_EXIT -eq 0 ]]; then
  ok "JMeter completed successfully"
else
  fail "JMeter exited with code $JMETER_EXIT"
fi

# -----------------------------------------------------------------------
sep "Results"
# -----------------------------------------------------------------------
echo ""
echo "Dialog Load Test results:"
echo "  Raw JTL:      $RESULTS_DIR/results.jtl"
echo "  HTML report:  $RESULTS_DIR/report/index.html"
echo ""
echo "PASS: $PASS  FAIL: $FAIL"
echo ""

if [[ $FAIL -eq 0 ]]; then
  echo "[SUCCESS] Load test complete."
  exit 0
else
  echo "[PARTIAL] Load test completed with errors."
  exit 1
fi
