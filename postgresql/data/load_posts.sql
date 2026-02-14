CREATE TEMP TABLE temp_posts (
    line_text TEXT
);

COPY temp_posts(line_text) FROM '/docker-entrypoint-initdb.d/data/posts.txt';

WITH numbered_users AS (
    SELECT user_id, ROW_NUMBER() OVER (ORDER BY user_id) - 1 AS user_num
    FROM social_net_schema.users
),
user_count AS (
    SELECT COUNT(*) AS cnt FROM social_net_schema.users
),
numbered_posts AS (
    SELECT line_text, ROW_NUMBER() OVER () - 1 AS post_num
    FROM temp_posts
)
INSERT INTO social_net_schema.posts (author_user_id, text)
SELECT u.user_id, p.line_text
FROM numbered_posts p
JOIN user_count c ON true
JOIN numbered_users u ON u.user_num = p.post_num % c.cnt;

DROP TABLE temp_posts;
