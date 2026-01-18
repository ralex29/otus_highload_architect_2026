#!/bin/bash
set -e

echo "Initializing database schema..."
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<-EOSQL
    -- Initialize database schema
EOSQL

# Execute schema files
for file in /docker-entrypoint-initdb.d/schemas/*.sql; do
    if [ -f "$file" ]; then
        echo "Executing schema file: $file"
        psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f "$file"
    fi
done

# Execute data files
for file in /docker-entrypoint-initdb.d/data/*.sql; do
    if [ -f "$file" ]; then
        echo "Executing data file: $file"
        psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f "$file"
    fi
done

echo "Database initialization completed successfully!"
