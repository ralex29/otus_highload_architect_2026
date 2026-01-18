# Docker Deployment Guide

Это руководство описывает как собрать и запустить social-net-service в Docker-контейнерах.

## Архитектура

Проект использует multi-stage Dockerfile с официальными образами userver:
- **Stage 1 (builder)**: Сборка C++ приложения
  - Базовый образ: `ghcr.io/userver-framework/ubuntu-22.04-userver-pg-dev:v2.14`
  - Содержит все необходимые build-зависимости для userver и PostgreSQL
  - Использует CMake и ccache для оптимизации сборки
- **Stage 2 (runtime)**: Минимальный образ для production
  - Базовый образ: `ghcr.io/userver-framework/ubuntu-22.04-userver-pg:v2.14`
  - Содержит только runtime зависимости userver и PostgreSQL
  - Значительно меньше размер финального образа

## Требования

- Docker 20.10+
- Docker Compose 2.0+

## Быстрый старт

### 1. Сборка и запуск всех сервисов

```bash
docker-compose up -d
```

Эта команда:
- Запустит PostgreSQL контейнер
- Инициализирует базу данных с помощью схем из `postgresql/schemas/`
- Загрузит начальные данные из `postgresql/data/`
- Соберёт и запустит приложение

### 2. Проверка состояния сервисов

```bash
docker-compose ps
```

### 3. Проверка логов

```bash
# Логи всех сервисов
docker-compose logs -f

# Логи только приложения
docker-compose logs -f app

# Логи только БД
docker-compose logs -f postgres
```

### 4. Тестирование приложения

```bash
# Health check
curl http://localhost:8080/ping
```

## Управление

### Остановка сервисов

```bash
docker-compose stop
```

### Перезапуск сервисов

```bash
docker-compose restart
```

### Полная очистка (включая volumes)

```bash
docker-compose down -v
```

### Пересборка образа приложения

```bash
docker-compose build --no-cache app
docker-compose up -d app
```

## Конфигурация

### Переменные окружения

Приложение использует следующие переменные окружения:

- `DB_CONNECTION` - строка подключения к PostgreSQL (задается в docker-compose.yml)

### Файлы конфигурации

- `configs/static_config.yaml` - основная конфигурация сервиса
- `configs/config_vars.docker.yaml` - переменные для Docker окружения
- `configs/config_vars.yaml` - переменные для локальной разработки

### База данных

PostgreSQL доступна на:
- Хост: `localhost`
- Порт: `15433` (снаружи контейнера)
- Пользователь: `testsuite`
- Пароль: `testsuite`
- База данных: `social_net_service_db_1`

Подключение из контейнера приложения:
```
postgresql://testsuite:testsuite@postgres:5432/social_net_service_db_1
```

## Версии userver

Проект использует официальные образы userver версии v2.14:
- Builder: `ghcr.io/userver-framework/ubuntu-22.04-userver-pg-dev:v2.14`
- Runtime: `ghcr.io/userver-framework/ubuntu-22.04-userver-pg:v2.14`

Для обновления версии userver измените версию в Dockerfile. Доступные версии можно посмотреть на:
https://github.com/userver-framework/userver/pkgs/container/ubuntu-22.04-userver-pg

## Разработка

### Только сборка образа

```bash
docker build -t social-net-service:latest .
```

### Сборка с кешированием ccache

Ccache уже настроен в Dockerfile. Для повторных сборок кеш будет использоваться автоматически:

```bash
docker build -t social-net-service:latest .
```

### Запуск отдельного контейнера

```bash
docker run -d \
  --name social-net-service \
  -p 8080:8080 \
  -e DB_CONNECTION="postgresql://testsuite:testsuite@postgres:5432/social_net_service_db_1" \
  social-net-service:latest
```

## Troubleshooting

### Приложение не может подключиться к БД

1. Проверьте, что PostgreSQL контейнер запущен:
   ```bash
   docker-compose ps postgres
   ```

2. Проверьте логи PostgreSQL:
   ```bash
   docker-compose logs postgres
   ```

3. Убедитесь, что healthcheck проходит:
   ```bash
   docker inspect social-net-postgres | grep -A 5 Health
   ```

### Ошибка при инициализации БД

Проверьте логи инициализации:
```bash
docker-compose logs postgres | grep init-db
```

### Пересоздание базы данных

```bash
docker-compose down -v
docker-compose up -d postgres
# Дождитесь инициализации БД
docker-compose up -d app
```

## Продакшн

Для продакшн-окружения рекомендуется:

1. Использовать внешнюю управляемую PostgreSQL
2. Настроить persistent volumes для данных
3. Добавить мониторинг и логирование
4. Настроить limits и requests для ресурсов
5. Использовать secrets для паролей вместо переменных окружения
6. Настроить reverse proxy (nginx/traefik)
7. Включить TLS/SSL

### Пример для продакшн с внешней БД

Создайте файл `docker-compose.prod.yml`:

```yaml
version: '3.8'

services:
  app:
    image: social-net-service:latest
    restart: always
    environment:
      DB_CONNECTION: postgresql://user:password@external-db-host:5432/dbname
    ports:
      - "8080:8080"
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '1'
          memory: 1G
```

Запуск:
```bash
docker-compose -f docker-compose.prod.yml up -d
```
