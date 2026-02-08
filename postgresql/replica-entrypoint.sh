#!/bin/bash
set -e

MASTER_HOST="${MASTER_HOST:-postgres-master}"
MASTER_PORT="${MASTER_PORT:-5432}"
REPLICATION_USER="${REPLICATION_USER:-replicator}"
REPLICATION_PASSWORD="${REPLICATION_PASSWORD:-replicator_password}"
APPLICATION_NAME="${APPLICATION_NAME:-replica}"

# Ensure PGDATA exists with correct ownership and permissions
mkdir -p "$PGDATA"
chown postgres:postgres "$PGDATA"
chmod 700 "$PGDATA"

export PGPASSWORD="$REPLICATION_PASSWORD"

echo "Waiting for master at ${MASTER_HOST}:${MASTER_PORT}..."
until gosu postgres pg_isready -h "$MASTER_HOST" -p "$MASTER_PORT" -U "$REPLICATION_USER"; do
    echo "Master is not ready yet, retrying in 2s..."
    sleep 2
done
echo "Master is ready."

# If data directory is empty or uninitialized, perform base backup
if [ ! -s "$PGDATA/PG_VERSION" ]; then
    echo "Data directory is empty, running pg_basebackup from master..."
    rm -rf "$PGDATA"/*

    gosu postgres pg_basebackup \
        -h "$MASTER_HOST" \
        -p "$MASTER_PORT" \
        -U "$REPLICATION_USER" \
        -D "$PGDATA" \
        -Fp \
        -Xs \
        -R \
        -P

    echo "Base backup completed."

    # Add application_name to primary_conninfo for synchronous replication
    sed -i "s|primary_conninfo = '|primary_conninfo = 'application_name=${APPLICATION_NAME} |" "$PGDATA/postgresql.auto.conf"
else
    echo "Data directory already initialized, ensuring standby mode..."
    touch "$PGDATA/standby.signal"
    chown postgres:postgres "$PGDATA/standby.signal"
    if ! grep -q "primary_conninfo" "$PGDATA/postgresql.auto.conf" 2>/dev/null; then
        echo "primary_conninfo = 'application_name=${APPLICATION_NAME} host=${MASTER_HOST} port=${MASTER_PORT} user=${REPLICATION_USER} password=${REPLICATION_PASSWORD}'" >> "$PGDATA/postgresql.auto.conf"
    fi
fi

unset PGPASSWORD

echo "Starting PostgreSQL replica..."
exec gosu postgres postgres -c hot_standby=on
