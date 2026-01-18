# Stage 1: Builder
FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg-dev:v2.14 AS builder

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Set working directory
WORKDIR /app

# Copy project files
COPY . .

# Configure ccache
ENV CCACHE_DIR=/app/.ccache
ENV CCACHE_COMPRESS=1
ENV CCACHE_COMPRESSLEVEL=6

# Create build directory and configure CMake
RUN mkdir -p build-release && cd build-release && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          ..

# Build the project
RUN cd build-release && make -j$(nproc)

# Install the project
RUN cd build-release && make install DESTDIR=/install

# Stage 2: Runtime
FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg:v2.14 AS runtime

# Install curl for healthcheck if not present
RUN apt-get update && apt-get install -y --no-install-recommends curl && \
    rm -rf /var/lib/apt/lists/*

# Create application user
RUN groupadd -r appuser && useradd -r -g appuser appuser

# Create necessary directories
RUN mkdir -p /app/configs && chown -R appuser:appuser /app

# Copy installed files from builder
COPY --from=builder /install/usr/local /usr/local
COPY --from=builder /app/configs /app/configs

# Set working directory
WORKDIR /app

# Switch to non-root user
USER appuser

# Expose application port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/ping || exit 1

# Run the application
# Use docker config if available, fallback to default
CMD ["/usr/local/bin/social_net_service", "--config", "/app/configs/static_config.yaml", "--config_vars", "/app/configs/config_vars.docker.yaml"]
