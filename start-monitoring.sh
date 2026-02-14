#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Ensure the main application network exists
if ! docker network inspect social-net-service_social-net-network &>/dev/null; then
    echo "Application network not found. Start the main compose first:"
    echo "  docker compose up -d"
    exit 1
fi

cd "$SCRIPT_DIR/monitoring"

case "${1:-up}" in
    up)
        echo "Starting monitoring stack (cAdvisor + Prometheus + Grafana)..."
        docker compose up -d
        echo ""
        echo "Monitoring is running:"
        echo "  Grafana:    http://localhost:3000  (admin / admin)"
        echo "  Prometheus: http://localhost:9090"
        echo "  cAdvisor:   http://localhost:8081"
        ;;
    down)
        echo "Stopping monitoring stack..."
        docker compose down
        ;;
    down-v)
        echo "Stopping monitoring stack and removing volumes..."
        docker compose down -v
        ;;
    logs)
        docker compose logs -f
        ;;
    *)
        echo "Usage: $0 {up|down|down-v|logs}"
        exit 1
        ;;
esac
