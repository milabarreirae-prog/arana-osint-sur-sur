# Araña OSINT Sur-Sur — Radar Geopolítico Autónomo

> **Motor Contrahegemónico (V2026.3.22.FINAL)**
> **Versión:** 1.0.0 | **Fecha:** 2026-04-28

---

## Contexto del Proyecto

**Araña OSINT Sur-Sur** es un demonio asíncrono en C++20 para rastrear, extraer y curar inteligencia geopolítica en tiempo real. Opera con LLMs locales (Ollama) y un firewall epistémico (NEWSGATE) para filtrar infoxicación hegemónica y generar un feed Rich RSS con perspectivas del Sur Global.

### Stack
- **Lenguaje:** C++20
- **LLM:** Ollama (local, GPU-accelerated)
- **Rastreo:** libcurl + pugixml
- **Persistencia:** SQLite3
- **Salida:** RSS 2.0 con CDATA enriquecido
- **UI:** Dashboard con telemetría (status.json, ollama_log.json)

### Componentes
1. **Spider** — Rastreo asíncrono y extracción DOM
2. **Evaluator** — Inferencia LLM vía REST
3. **Graph Connector** — Persistencia SQLite3 (memoria histórica)
4. **Feed Generator** — RSS 2.0 con tarjetas visuales
5. **Main Worker** — Orquestación y ciclo de vida systemd
6. **Feedback Server** — Dashboard en tiempo real

---

## Doc Dictionary

| Archivo | Propósito |
|---------|-----------|
| `README.md` | Documentación técnica completa |
| `src/` | Código fuente C++20 |

---

## Conexiones con Otras Células

- **cimarronaje-digital** — Auditoría de data oscura judicial chilena
- **aranha-saude** — Análisis de datos y pipelines de información

---

*Más Jamaica y menos Miles Davis.*
