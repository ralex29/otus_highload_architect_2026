DROP SCHEMA IF EXISTS social_net_schema CASCADE;

CREATE SCHEMA IF NOT EXISTS social_net_schema;

CREATE TABLE IF NOT EXISTS social_net_schema.users (
    user_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    first_name TEXT NOT NULL,
    second_name TEXT NOT NULL,
    birthdate DATE NOT NULL,
    sex TEXT NOT NULL,
    biography TEXT NOT NULL,
    city TEXT NOT NULL,
    password_hash TEXT NOT NULL,
    salt  TEXT NOT NULL
);

CREATE INDEX idx_names ON social_net_schema.users
    (first_name text_pattern_ops, second_name text_pattern_ops);

CREATE TABLE IF NOT EXISTS social_net_schema.friendships (
    user_id UUID NOT NULL REFERENCES social_net_schema.users(user_id),
    friend_id UUID NOT NULL REFERENCES social_net_schema.users(user_id),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, friend_id)
);

CREATE INDEX idx_friendships_user ON social_net_schema.friendships(user_id);
CREATE INDEX idx_friendships_friend ON social_net_schema.friendships(friend_id);

CREATE TABLE IF NOT EXISTS social_net_schema.posts (
    post_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    author_user_id UUID NOT NULL REFERENCES social_net_schema.users(user_id),
    text TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_posts_author ON social_net_schema.posts(author_user_id);
CREATE INDEX idx_posts_created ON social_net_schema.posts(created_at DESC);

DROP SCHEMA IF EXISTS auth_schema CASCADE;

CREATE SCHEMA IF NOT EXISTS auth_schema;

CREATE TABLE IF NOT EXISTS auth_schema.tokens (
    token TEXT PRIMARY KEY NOT NULL,
    user_id UUID NOT NULL,
    scopes TEXT[] NOT NULL,
    updated TIMESTAMPTZ NOT NULL DEFAULT NOW()
);