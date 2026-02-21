#!/bin/bash
set -e

# Wait for worker nodes to be ready and register them with the coordinator
PGUSER="${POSTGRES_USER:-testsuite}"
PGDB="${POSTGRES_DB:-social_net_dialogs}"

wait_for_worker() {
    local host="$1"
    local port="${2:-5432}"
    local retries=30
    echo "Waiting for worker $host:$port..."
    for i in $(seq 1 $retries); do
        if pg_isready -h "$host" -p "$port" -U "$PGUSER" -d "$PGDB" > /dev/null 2>&1; then
            echo "Worker $host:$port is ready."
            return 0
        fi
        sleep 2
    done
    echo "Worker $host:$port did not become ready in time." >&2
    return 1
}

wait_for_worker citus-worker-0 5432
wait_for_worker citus-worker-1 5432

psql -U "$PGUSER" -d "$PGDB" -c "SELECT citus_set_coordinator_host('citus-coordinator', 5432);"
psql -U "$PGUSER" -d "$PGDB" -c "SELECT citus_add_node('citus-worker-0', 5432);"
psql -U "$PGUSER" -d "$PGDB" -c "SELECT citus_add_node('citus-worker-1', 5432);"

echo "Citus workers registered. Distributing table and creating indexes..."

psql -U "$PGUSER" -d "$PGDB" -c "SELECT create_distributed_table('dialog_schema.messages', 'virtual_bucket');"
psql -U "$PGUSER" -d "$PGDB" -c "
CREATE INDEX idx_messages_dialog ON dialog_schema.messages(
    LEAST(from_user_id, to_user_id),
    GREATEST(from_user_id, to_user_id),
    created_at
);"

echo "Citus setup complete."
