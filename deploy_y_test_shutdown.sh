#!/bin/bash
set -e

echo "=========================================================="
echo "  Deploy Automático - Araña OSINT Sur-Sur (V2026.3)       "
echo "=========================================================="

# Validación de ejecución como root
if [ "$EUID" -ne 0 ]; then
  echo "ERROR: Este script debe ejecutarse como root."
  echo "Uso recomendado desde PowerShell: wsl -d fedoraremix -u root bash deploy_y_test_shutdown.sh"
  exit 1
fi

DIR_PROYECTO="/mnt/c/Users/milab/.gemini/antigravity/scratch/arana-osint-sur-sur"

echo "[1] Deteniendo servicio..."
systemctl stop sursur-daemon || true

echo "[2] Compilando binario C++..."
cd $DIR_PROYECTO/build
make -j$(nproc)

echo "[3] Desplegando binario y configuración..."
cp -f sur-sur-daemon /usr/sbin/sur-sur-daemon
chmod +x /usr/sbin/sur-sur-daemon

mkdir -p /etc/sursur
cp -f $DIR_PROYECTO/sursur_config.json /etc/sursur/sursur_config.json

echo "[4] Estructurando directorios base de telemetría y BD..."
mkdir -p /var/lib/sursur/media
mkdir -p /var/lib/sursur/db
mkdir -p /var/lib/sursur/ui

echo "[5] Purgando telemetría y feed (Hard Reset)..."
# Se elimina el XML para forzar a FeedGenerator a reconstruir la cabecera limpia
rm -f /var/lib/sursur/ui/feed.xml
echo "[]" > /var/lib/sursur/ui/ollama_log.json
echo "{\"status\": \"iniciando\", \"timestamp\": $(date +%s)}" > /var/lib/sursur/ui/status.json

echo "[6] Desplegando portal de monitoreo (Centro de Mando)..."
cp -f $DIR_PROYECTO/ui/index.html /var/lib/sursur/ui/index.html

# Ajuste de permisos si Nginx está involucrado (Opcional según entorno)
chown -R nginx:nginx /var/lib/sursur/ui 2>/dev/null || true

echo "[7] Reiniciando servicio via systemd..."
systemctl daemon-reload
systemctl start sursur-daemon

echo "[8] Verificando estado del demonio..."
sleep 2
systemctl status sursur-daemon --no-pager | head -n 15

echo "=========================================================="
echo "Despliegue completado. Sistema operando bajo protocolos de Fase 5."
echo "Para monitorear descartes en tiempo real, ejecuta:"
echo "journalctl -u sursur-daemon -f | grep -E 'Spider: Descartada|Spider: Inyectando'"
echo "=========================================================="