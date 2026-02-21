CREATE EXTENSION IF NOT EXISTS citus;
CREATE SCHEMA IF NOT EXISTS dialog_schema;

CREATE TABLE dialog_schema.messages (
    message_id     UUID NOT NULL DEFAULT gen_random_uuid(),
    from_user_id   UUID NOT NULL,
    to_user_id     UUID NOT NULL,
    text           TEXT NOT NULL,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    virtual_bucket INT NOT NULL,
    PRIMARY KEY (virtual_bucket, message_id)
);

-- NOTE: create_distributed_table and index are called in init-citus.sh
-- AFTER worker nodes are registered, so shards land on workers from the start.
