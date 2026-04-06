#!/usr/bin/env bash
set -e

# Start nginx in background
echo "[entrypoint] Starting nginx on port 1973..."
nginx -g "daemon off;" &
NGINX_PID=$!

# Short delay to ensure nginx is ready
sleep 1

# Start the daemon
echo "[entrypoint] Starting sur-sur-daemon..."
exec /usr/sbin/sur-sur-daemon --config=/etc/sursur/sursur_config.json &
DAEMON_PID=$!

# Trap signals and forward to both processes
cleanup() {
    echo "[entrypoint] Shutting down..."
    kill -SIGTERM "$DAEMON_PID" 2>/dev/null || true
    kill -SIGTERM "$NGINX_PID" 2>/dev/null || true
    wait "$DAEMON_PID" 2>/dev/null || true
    wait "$NGINX_PID" 2>/dev/null || true
    exit 0
}

trap cleanup SIGTERM SIGINT SIGQUIT

# Wait for either process to exit
wait
