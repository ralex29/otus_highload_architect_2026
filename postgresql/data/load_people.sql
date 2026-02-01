-- Load users from people.v2.csv
-- CSV format: "Фамилия Имя,дата_рождения,город"

-- Create temporary table for CSV import
CREATE TEMP TABLE temp_people (
    full_name TEXT,
    birthdate DATE,
    city TEXT
);

-- Load CSV data (path for Docker: /docker-entrypoint-initdb.d/data/)
COPY temp_people(full_name, birthdate, city) FROM '/docker-entrypoint-initdb.d/data/people.v2.csv' WITH (FORMAT csv, ENCODING 'UTF8');

-- Insert into users table, splitting full_name into second_name and first_name
INSERT INTO social_net_schema.users (second_name, first_name, birthdate, city, sex, biography, password_hash, salt)
SELECT
    split_part(full_name, ' ', 1) AS second_name,
    split_part(full_name, ' ', 2) AS first_name,
    birthdate,
    city,
    CASE WHEN random() < 0.5 THEN 'man' ELSE 'woman' END AS sex,
    '' AS biography,
    '' AS password_hash,
    '' AS salt
FROM temp_people;

DROP TABLE temp_people;