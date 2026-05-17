# Stage 1: Builder
FROM ghcr.io/userver-framework/ubuntu-24.04-userver:latest AS builder

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /app

COPY counter-service/ counter-service/
COPY proto/ proto/

ENV CCACHE_DIR=/app/.ccache
ENV CCACHE_COMPRESS=1
ENV CCACHE_COMPRESSLEVEL=6

RUN mkdir -p counter-service/build-release && cd counter-service/build-release && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          ..

RUN cd counter-service/build-release && make -j$(nproc)
RUN cd counter-service/build-release && make install DESTDIR=/install

# Stage 2: Runtime
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    libssl3 \
    libhiredis1.1.0 \
    libjemalloc2 \
    libcares2 \
    libicu74 \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r appuser && useradd -r -g appuser appuser
RUN mkdir -p /app/configs && chown -R appuser:appuser /app

COPY --from=builder /install/usr/local /usr/local
COPY --from=builder /app/counter-service/configs /app/configs

# userver runtime shared libraries
COPY --from=builder /usr/lib/libuserver-*.so* /usr/lib/
COPY --from=builder /usr/lib/libamqpcpp.so* /usr/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libboost_*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcctz.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libre2.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcryptopp.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libev.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libzstd.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libnghttp2.so* /usr/lib/x86_64-linux-gnu/
# gRPC runtime libraries
COPY --from=builder /usr/lib/x86_64-linux-gnu/libgrpc*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libprotobuf*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libabsl*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libgpr.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libupb*.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libaddress_sorting.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libicu*.so* /usr/lib/x86_64-linux-gnu/

RUN ldconfig

WORKDIR /app
USER appuser

EXPOSE 8082 8091

HEALTHCHECK --interval=30s --timeout=10s --start-period=20s --retries=3 \
    CMD curl -f http://localhost:8082/ping || exit 1

CMD ["/usr/local/bin/counter_service", \
     "--config", "/app/configs/static_config.yaml", \
     "--config_vars", "/app/configs/config_vars.docker.yaml"]
