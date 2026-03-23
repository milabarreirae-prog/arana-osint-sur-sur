// ============================================================================
// Araña OSINT Sur-Sur — Demonio Principal (main.cpp)
// Servidor gRPC que integra todos los módulos del motor OSINT.
// Manejo de señales para apagado graceful del demonio.
// ============================================================================
#include "config_manager.hpp"
#include "monitor.hpp"
#include "evaluator.hpp"
#include "graph_connector.hpp"
#include "media_processor.hpp"
#include "spider.hpp"
#include "feed_generator.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <array>
#include <cctype>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <deque>
#include <queue>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>

// ── Señal de apagado global ────────────────────────────────────────────────
std::atomic<bool> g_apagar{false};

static void manejador_senal(int /*senal*/) {
    g_apagar.store(true, std::memory_order_relaxed);
}

// ── Cola de Inferencia Asíncrona ──────────────────────────────────────────
// ── Telemetría en Memoria (Dashboard en Tiempo Real) ──────────────────────
namespace sursur {
    struct EventoLog {
        std::string ts;
        std::string url;
        std::string autor;
        std::string tema;
        bool aprobado;
        std::string falacias;
        std::string justificacion;
        std::string clasificacion;
        std::string relevancia;
    };
}
static std::deque<sursur::EventoLog> g_ollama_log;
static std::mutex g_mutex_ollama_log;

static std::queue<sursur::ResultadoRastreo> g_cola_inferencia;
static std::mutex g_mutex_cola_inferencia;
static std::condition_variable g_cv_cola_inferencia;

static std::atomic<uint64_t> g_urls_procesadas{0};
static std::atomic<uint64_t> g_urls_aprobadas{0};
static std::atomic<uint64_t> g_urls_rechazadas{0};

// ════════════════════════════════════════════════════════════════════════════
// Punto de entrada principal del demonio
// ════════════════════════════════════════════════════════════════════════════
int main(int argc, char *argv[]) {
  // ── Configurar logging ─────────────────────────────────────────────────
  auto consola = spdlog::stdout_color_mt("consola");
  spdlog::set_default_logger(consola);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info); // Protección ante core-dump
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

  spdlog::info("═══════════════════════════════════════════════════════════");
  spdlog::info("  Araña OSINT Sur-Sur — Demonio v0.1.0 (Concurrent Ready)");
  spdlog::info("  Motor de recolección, curación y síntesis de medios");
  spdlog::info("═══════════════════════════════════════════════════════════");

  // ── Cargar configuración ───────────────────────────────────────────────
  auto &config = sursur::ConfigManager::instancia();

  // Cargar configuración desde ruta por defecto (SSOT: JSON)
  if (!config.cargar_configuracion("/etc/sursur/sursur_config.json")) {
    spdlog::warn("Demonio: No se pudo cargar /etc/sursur/sursur_config.json, usando valores por defecto.");
  }

  config.volcar_configuracion();

  // ── 2. Inicializar módulos base (Evaluador central) ─────────────────────
  auto evaluador = std::make_shared<sursur::Evaluator>();
  auto feed_gen = std::make_shared<sursur::FeedGenerator>();
  auto spider = std::make_shared<sursur::Spider>();

  // ── 3. Lanzar servicios neurales y workers ─────────────────────────────
  const auto& cfg = config; // Referencia para el snippet del usuario
  auto worker = [&]() {
      while (!g_apagar) {
          sursur::ResultadoRastreo ev;
          {
              std::unique_lock<std::mutex> lk(g_mutex_cola_inferencia);
              g_cv_cola_inferencia.wait(lk, []{ return !g_cola_inferencia.empty() || g_apagar; });
              if (g_apagar && g_cola_inferencia.empty()) break;
              ev = g_cola_inferencia.front();
              g_cola_inferencia.pop();
          }

          try {
              std::string texto_limpio = spider->extraer_texto(ev.html_crudo);
              std::string eval_str = evaluador->evaluar_documento(ev.url, texto_limpio, cfg.llm().system_prompt);
              nlohmann::json eval_json = nlohmann::json::parse(eval_str);
              
              bool aprobado = false;
              if (eval_json.contains("aprobado") && eval_json["aprobado"].is_boolean()) {
                  aprobado = eval_json["aprobado"].get<bool>();
              }

              std::string relevancia = "BAJA";
              if (eval_json.contains("relevancia") && eval_json["relevancia"].is_string()) {
                  relevancia = eval_json["relevancia"].get<std::string>();
              } else if (eval_json.contains("radar_geopolitico") && eval_json["radar_geopolitico"].is_object()) {
                  if (eval_json["radar_geopolitico"].contains("relevancia") && eval_json["radar_geopolitico"]["relevancia"].is_string()) {
                      relevancia = eval_json["radar_geopolitico"]["relevancia"].get<std::string>();
                  }
              }

              relevancia.erase(std::remove_if(relevancia.begin(), relevancia.end(), ::isspace), relevancia.end());
              std::transform(relevancia.begin(), relevancia.end(), relevancia.begin(), ::toupper);

              // 1. EXTRAER DATOS PARA LOG ANTES DEL CORTAFUEGOS
              std::string clasificacion = eval_json.value("clasificacion", eval_json.value("radar_geopolitico", nlohmann::json::object()).value("clasificacion", "DESCONOCIDA"));
              std::string justificacion = eval_json.value("justificacion_seleccion", eval_json.value("radar_geopolitico", nlohmann::json::object()).value("justificacion_seleccion", "Sin justificación evaluada"));
              std::string autor = eval_json.value("autor", eval_json.value("metadatos", nlohmann::json::object()).value("autor", "Fuente en Análisis"));
              std::string tema = eval_json.value("tema_principal", eval_json.value("metadatos", nlohmann::json::object()).value("tema_principal", "Tema no clasificado"));
              
              // 2. REGISTRAR EN TELEMETRÍA (Para poblar el Dashboard)
              {
                  std::lock_guard<std::mutex> lk_log(g_mutex_ollama_log);
                  sursur::EventoLog evento_log;
                  
                  auto ahora = std::chrono::system_clock::now();
                  std::time_t tiempo = std::chrono::system_clock::to_time_t(ahora);
                  std::ostringstream oss;
                  oss << std::put_time(std::localtime(&tiempo), "%Y-%m-%d %H:%M:%S");
                  evento_log.ts = oss.str();

                  evento_log.url = ev.url;
                  evento_log.autor = autor;
                  evento_log.tema = tema;
                  evento_log.aprobado = aprobado;
                  evento_log.falacias = eval_json.value("falacias_detectadas", nlohmann::json::array()).dump();
                  evento_log.justificacion = justificacion;
                  evento_log.clasificacion = clasificacion;
                  evento_log.relevancia = relevancia;
                  
                  g_ollama_log.push_back(evento_log);
                  if (g_ollama_log.size() > 100) g_ollama_log.pop_front();
              }

              // 3. CORTAFUEGOS IMPLACABLE (Protección Fase 5)
              if (!aprobado || (relevancia != "ALTA" && relevancia != "MEDIA")) {
                  spdlog::warn("Spider: Firewall bloqueó noticia. Relevancia: {}", relevancia);
                  g_urls_rechazadas++;
                  continue; // Descarta sin inyectar en el XML
              }

              g_urls_aprobadas++;
              g_urls_procesadas++;

              // 4. SI PASA, INYECTAR EN RSS
              std::string img_prin = ""; 
              spdlog::info("Spider: Inyectando artículo APROBADO: {}", ev.url);
              feed_gen->inyectar_item_rss_atomico(ev.url, ev.html_crudo, eval_json, img_prin);

          } catch (const std::exception& e) {
              spdlog::error("Spider: Hilo protegido atrapó excepción JSON: {}", e.what());
              g_urls_rechazadas++;
              continue;
          }
      }
  };

  std::thread worker_thread(worker);
  worker_thread.detach();

  std::thread hilo_llm([evaluador]() {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      if (evaluador->inicializar_modelo()) {
          sursur::MonitorState::get().model_name = evaluador->obtener_nombre_modelo();
      }
  });
  hilo_llm.detach();

  // Registro de manejadores de señales
  std::signal(SIGTERM, manejador_senal);
  std::signal(SIGINT, manejador_senal);

  // ── 4. Inicializar y Arrancar Spider (AL FINAL) ────────────────────────
  spider->set_feed_gen(feed_gen);
  
  spider->registrar_callback([](const sursur::ResultadoRastreo &res) {
      if (res.exitoso) {
          std::lock_guard<std::mutex> lock(g_mutex_cola_inferencia);
          g_cola_inferencia.push(res);
          g_cv_cola_inferencia.notify_one();
      }
  });

  const auto& net_cfg = config.network();
  spdlog::info("Demonio: Cargando semillas desde SSOT (Max Jumps: {})...", net_cfg.max_jumps);
  for (const auto& semilla : config.sources().contrahegemonic_seeds) {
      spider->iniciar_rastreo(semilla, net_cfg.max_jumps);
  }
  for (const auto& semilla : config.sources().hegemonic_contrast_seeds) {
      spider->iniciar_rastreo(semilla, net_cfg.max_jumps);
  }

  std::thread spider_thread([spider]() {
      spdlog::info("Spider: Iniciando bucle de rastreo...");
      spider->iniciar_bucle();
  });
  spider_thread.detach();

  // ── 5. Bucle principal de vida activa + Telemetría ───────────────────────
  spdlog::info("Demonio: Motor OSINT activo. Red delegada a Nginx. Inferencia delegada a Ollama.");
  const std::string modelo_nombre = evaluador->obtener_nombre_modelo();
  int tick = 0;

  while (!g_apagar.load()) {
      // ── status.json (cada segundo) ───────────────────────────────────────
      {
          nlohmann::json st;
          st["status"]      = "activo";
          st["timestamp"]   = static_cast<long>(time(nullptr));
          st["modelo"]      = modelo_nombre;
          st["procesadas"]  = g_urls_procesadas.load();
          st["aprobadas"]   = g_urls_aprobadas.load();
          st["rechazadas"]  = g_urls_rechazadas.load();
          st["cola"]        = (int)g_cola_inferencia.size();
          st["uptime_s"]    = tick;
          st["msg"]         = "Spider rastreando noticias...";
          std::ofstream f1("/var/lib/sursur/ui/status.json");
          f1 << st.dump();
      }

      // ── ollama_log.json (cada 2 segundos) ────────────────────────────────
      if (tick % 2 == 0) {
          nlohmann::json arr = nlohmann::json::array();
          {
              std::lock_guard<std::mutex> lk(g_mutex_ollama_log);
              for (const auto& ev : g_ollama_log) {
                  nlohmann::json j;
                  j["ts"]          = ev.ts;
                  j["url"]         = ev.url;
                  j["autor"]       = ev.autor;
                  j["tema"]        = ev.tema;
                  j["aprobado"]    = ev.aprobado;
                  j["falacias"]    = ev.falacias;
                  j["justificacion"] = ev.justificacion;
                  j["radar"]       = {{"clasif", ev.clasificacion}, {"relev", ev.relevancia}};
                  arr.push_back(j);
              }
          }
          std::ofstream f2("/var/lib/sursur/ui/ollama_log.json");
          f2 << arr.dump();
      }

      ++tick;
      std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // ── 6. Apagado ordenado ─────────────────────────────────────────────────
  spdlog::info("Demonio: apagando componentes...");
  if (spider) spider->detener_todos();
  
  spdlog::info("Araña OSINT Sur-Sur — Apagado exitoso.");
  return 0;
}
