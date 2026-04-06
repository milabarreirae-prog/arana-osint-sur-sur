# **Araña OSINT Sur-Sur 🕸️📡**

**Radar Geopolítico Autónomo y Motor Contrahegemónico (V2026.3.22.FINAL)**

Araña OSINT Sur-Sur es un demonio asíncrono escrito en C++20, diseñado para rastrear, extraer y curar inteligencia geopolítica en tiempo real. Utilizando inferencia de Modelos de Lenguaje Locales (LLM vía Ollama) y un estricto cortafuegos epistémico (NEWSGATE), el sistema filtra la infoxicación hegemónica y genera un feed Rich RSS inmersivo con perspectivas del Sur Global.

## **🏗️ Arquitectura del Sistema**

El sistema está compuesto por módulos altamente desacoplados, orquestados mediante una arquitectura basada en eventos y colas seguras para subprocesos:

1. **Spider (spider.cpp)**: Motor de rastreo asíncrono basado en libcurl y pugixml. Extrae el DOM, purga scripts/estilos y extrae el texto limpio de los artículos.  
2. **Evaluator (evaluator.cpp)**: Interfaz REST de inferencia LLM. Se conecta a Ollama (/api/chat), inyecta el esquema JSON plano y limpia alucinaciones de formato (Markdown). Está optimizado para aceleración por hardware (num\_gpu: 99).  
3. **Graph Connector (graph\_connector.cpp)**: Capa de persistencia SQLite3. Actúa como memoria histórica para evitar el re-procesamiento de URLs ya analizadas, ahorrando tokens y cómputo.  
4. **Feed Generator (feed\_generator.cpp)**: Motor de salida que construye un XML estándar (RSS 2.0). Inyecta bloques \<\!\[CDATA\[...\]\]\> con HTML y CSS en línea para renderizar "Tarjetas de Inteligencia" (Rich RSS).  
5. **Main Worker (main.cpp)**: El corazón del demonio. Encola eventos, maneja el *Hard Drop* (eliminación en memoria de noticias irrelevantes), puebla la telemetría y controla el ciclo de vida del servicio mediante systemd.  
6. **Centro de Mando (feedback\_server.cpp / UI)**: Dashboard servido en tiempo real que lee el estado de la memoria (status.json, ollama\_log.json) para proveer observabilidad.

## **🛡️ El Firewall Epistémico (NEWSGATE)**

El núcleo de evaluación opera bajo el **System Prompt Maestro \[V2026.3.22.FINAL\]**, configurado a través del archivo /etc/sursur/sursur\_config.json.

* **Axioma Cero**: "Más Jamaica y menos Miles Davis". Priorización estricta del "Arsenal de Soberanía".  
* **Filtro Anti-Frivolidad**: Rechazo automático e infranqueable de noticias sobre deportes, hazañas personales, farándula o videos virales.  
* **Vector Decolonial**: Bloqueo de discursos de odio, extractivismo, sionismo, capacitismo y sesgos cisgénero.  
* **Cortafuegos C++ (Hard Drop)**: Si el LLM etiqueta una noticia con relevancia BAJA o falla en su estructuración, el subproceso en main.cpp aniquila el dato en memoria antes de que toque el disco, manteniendo el ecosistema libre de ruido.

## **⚙️ Requisitos del Entorno**

El software está diseñado para ejecutarse en entornos Linux empresariales o en subsistemas WSL2 (Windows Subsystem for Linux).

* **SO**: Fedora Remix (WSL2) o RHEL 8+.  
* **Hardware Crítico**: GPU dedicada con al menos 16 GB VRAM (ej. NVIDIA RTX 3080\) para inferencia LLM local sin cuello de botella.  
* **Compilación**: g++ (soporte C++20), cmake, make.  
* **Dependencias**:  
  * libcurl-devel  
  * sqlite-devel  
  * pugixml-devel  
  * spdlog-devel  
  * nlohmann-json-devel  
* **Servicios Externos**:  
  * ollama (con modelo qwen2.5-coder:7b o superior).  
  * nginx (para servir la interfaz estática del Centro de Mando).

## **🚀 Despliegue y Operación**

### **1\. Despliegue con Docker (Recomendado)**

El sistema está disponible como un único contenedor tipo *appliance* que ejecuta tanto el demonio C++ como Nginx en el mismo proceso. Ollama se ejecuta en el host.

**Requisitos previos:**

- Docker + Docker Compose instalados
- Ollama corriendo en el host con el modelo cargado:
  ```bash
  ollama pull qwen2.5-coder:7b
  ollama serve
  ```

**Arranque:**

```bash
docker compose build
docker compose up -d
```

El Centro de Mando queda accesible en **http://localhost:1973/**.

**Ver logs:**

```bash
docker compose logs -f daemon
```

**Detener:**

```bash
docker compose down           # Mantiene la BD SQLite
docker compose down -v        # Borra datos persistentes (reset completo)
```

**Configuración:** Edita `sursur_config.json` en la raíz del proyecto y reinicia el contenedor. El archivo se monta como bind-mount (no requiere rebuild).

### **2\. Compilación y Despliegue Automatizado (Bare Metal / WSL2)**

El proyecto cuenta con un script de despliegue atómico que detiene el servicio, compila el binario en 4 hilos, migra la configuración y reinicia el demonio.

Debe ejecutarse como usuario root:

bash deploy\_y\_test\_shutdown.sh

### **3\. Estructura de Directorios del Sistema**

Tras el despliegue, la infraestructura se organiza en las siguientes rutas estándar de Linux:

* /usr/sbin/sur-sur-daemon: Binario ejecutable principal.  
* /etc/sursur/sursur\_config.json: Single Source of Truth (SSOT) para configuración y prompts.  
* /var/lib/sursur/db/graph.db: Base de datos histórica SQLite3.  
* /var/lib/sursur/ui/: Raíz documental de Nginx. Contiene el Dashboard (index.html), telemetría (ollama\_log.json) y el archivo de salida final (feed.xml).

### **4\. Observabilidad**

El Centro de Mando está accesible vía HTTP (ej. http://localhost:1973/).

Para auditar la telemetría y el comportamiento del cortafuegos en tiempo real directamente desde la consola:

journalctl \-u sursur-daemon \-f | grep \-E 'Spider: Descartada|Spider: Inyectando'

## **🛠️ Resoluciones Críticas (Registro de Arquitectura)**

Durante la Fase 5 del proyecto, se implementaron soluciones arquitectónicas críticas que definen la estabilidad actual:

1. **Aplanamiento JSON y Limpieza Markdown**: Se eliminó la anidación del esquema de salida de Ollama. Se introdujo una rutina en evaluator.cpp que elimina etiquetas \`\`\`json para evitar excepciones de parseo (type\_error) que mataban el hilo de trabajo.  
2. **Enrutamiento Dinámico de API**: El Evaluator corrige dinámicamente el endpoint hacia /api/chat en tiempo de ejecución, asegurando compatibilidad total y permitiendo el uso de aceleración GPU (num\_gpu: 99).  
3. **Telemetría Pre-Drop**: El main.cpp fue refactorizado para guardar el registro de inferencia del LLM en la memoria *antes* de ejecutar el *Hard Drop*, permitiendo auditar por qué el sistema descarta noticias.  
4. **Sincronización Nginx/C++**: La salida del Rich RSS (feed.xml) fue redirigida desde /media/ hacia la raíz documental de UI (/var/lib/sursur/ui/) resolviendo conflictos HTTP 404\.