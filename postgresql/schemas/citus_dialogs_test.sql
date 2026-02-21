-- Plain PostgreSQL schema for functional tests (no Citus extension required)
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

CREATE INDEX idx_messages_dialog ON dialog_schema.messages(
    LEAST(from_user_id, to_user_id),
    GREATEST(from_user_id, to_user_id),
    created_at
);
