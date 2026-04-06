FROM fedora:39 AS builder

RUN dnf install -y \
    gcc-c++ cmake make pkg-config wget tar xz git \
    libcurl-devel sqlite-devel pugixml-devel \
    spdlog-devel fmt-devel inih-devel \
    && dnf clean all

WORKDIR /build
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. \
    && make -j$(nproc)

# ====== STAGE 2: Runtime (single appliance) ======
FROM fedora:39 AS runtime

# Install runtime libs + nginx
RUN dnf install -y \
    libcurl sqlite-libs pugixml \
    spdlog fmt inih inih-cpp \
    nginx \
    && dnf clean all \
    && mkdir -p /etc/sursur /var/lib/sursur/{db,media,ui} \
    && mkdir -p /run/nginx

# Copy compiled binary
COPY --from=builder /build/build/sur-sur-daemon /usr/sbin/sur-sur-daemon

# Copy config and UI
COPY sursur_config.json /etc/sursur/sursur_config.json
COPY ui/index.html /var/lib/sursur/ui/index.html

# Copy nginx config
COPY nginx/nginx.conf /etc/nginx/nginx.conf

RUN chmod +x /usr/sbin/sur-sur-daemon

# Entrypoint script launches both nginx and daemon
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /var/lib/sursur

# Expose nginx dashboard
EXPOSE 1973

ENTRYPOINT ["/entrypoint.sh"]
