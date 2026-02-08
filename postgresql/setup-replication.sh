#!/bin/bash
set -e

echo "Configuring streaming replication on master..."

# Create replication user
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<-EOSQL
    CREATE ROLE replicator WITH REPLICATION LOGIN PASSWORD 'replicator_password';
EOSQL

# Allow replication connections from any host in the docker network
echo "host replication replicator 0.0.0.0/0 md5" >> "$PGDATA/pg_hba.conf"

# Reload config so pg_hba changes take effect
pg_ctl reload

echo "Replication configuration completed."
