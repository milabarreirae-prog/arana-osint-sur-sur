#!/bin/bash
# ============================================================================
# Araña OSINT Sur-Sur — Script de Limpieza de Repositorio (v2.0)
# Propósito: Purga de artefactos de compilación, datos volátiles, modelos y scripts.
# Cumplimiento: Estado de 'Fuente Pura' para estándares RPM de Fedora.
# ============================================================================

echo "------> Iniciando limpieza quirúrgica del entorno Araña OSINT..."

# 1. Eliminación de artefactos de construcción y generación de código
echo "[1/5] Eliminando directorios de build, rpmbuild y binarios..."
rm -rf build/
rm -rf rpmbuild/
rm -rf proto_gen/
rm -f sur-sur-daemon

# 2. Purga de modelos de IA y pesos descargados
# Los modelos (.gguf) son pesados y deben gestionarse mediante la configuración
# del sistema en /var/lib/sursur/models, no en el repositorio de código.
echo "[2/5] Removiendo modelos IA (.gguf) y pesos locales..."
rm -rf models/
rm -f *.gguf
rm -f *.zip

# 3. Purga de bases de datos de prueba y archivos de estado
echo "[3/5] Eliminando bases de datos SQLite (.db) y archivos de volcado..."
rm -f *.db
rm -f *.db-journal
rm -f coredump*

# 4. Eliminación de logs, reportes de error y capturas de diagnóstico
echo "[4/5] Limpiando logs de sistema, reportes de error y telemetría..."
rm -f *.txt
rm -f *.log

# 5. Limpieza de scripts auxiliares y herramientas de desarrollo local
# Se remueven scripts .sh (exceptuando este) y herramientas .py de soporte.
echo "[5/5] Removiendo scripts auxiliares y herramientas de soporte (.sh, .py)..."
find . -maxdepth 1 -name "*.sh" ! -name "limpiar_repositorio.sh" -exec rm -f {} +
rm -f *.py

echo "------------------------------------------------------------"
echo "CONSOLIDACIÓN COMPLETADA:"
echo "Estructura preservada: /src, /include, /systemd, /rpm, /conf, /config, /proto."
echo "El repositorio ha sido purgado de modelos, datos y ruido técnico."
