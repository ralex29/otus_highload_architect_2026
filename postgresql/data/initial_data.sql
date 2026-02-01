INSERT INTO social_net_schema.users (first_name, second_name, birthdate, sex, biography, city, password_hash, salt)
VALUES ('Joe', 'Cool', '2017-02-01', 'man', 'Хобби, интересы и т.п.', 'Москва', 'passwordhash', 'salt')
RETURNING user_id;

