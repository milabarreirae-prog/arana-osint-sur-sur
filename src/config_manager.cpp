// ============================================================================
// Araña OSINT Sur-Sur — ConfigManager (Implementación)
// ============================================================================
#include "config_manager.hpp"
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>

namespace sursur {

ConfigManager &ConfigManager::instancia() {
  static ConfigManager instancia_unica;
  return instancia_unica;
}

ConfigManager::ConfigManager() {
  spdlog::info("ConfigManager: instanciado. Listo para cargar SSOT.");
}

bool ConfigManager::cargar_configuracion(
    const std::filesystem::path &ruta_json) {
  std::lock_guard<std::mutex> cerrojo(mutex_);

  if (!std::filesystem::exists(ruta_json)) {
    spdlog::warn("ConfigManager: No se encontró {}. Usando defaults.",
                 ruta_json.string());
    return false;
  }

  try {
    std::ifstream f(ruta_json);
    nlohmann::json j = nlohmann::json::parse(f);
    parsing_json_to_structs_(j);
    spdlog::info("ConfigManager: SSOT JSON cargado exitosamente desde {}",
                 ruta_json.string());
    return true;
  } catch (const nlohmann::json::exception &e) {
    spdlog::error("ConfigManager: Error crítico de parseo JSON en {}: {}",
                  ruta_json.string(), e.what());
    return false;
  } catch (const std::exception &e) {
    spdlog::error("ConfigManager: Error de I/O en {}: {}", ruta_json.string(),
                  e.what());
    return false;
  }
}

void ConfigManager::parsing_json_to_structs_(const nlohmann::json &j) {
  // Parseo tolerante a fallos
  try {
    if (j.contains("network")) {
      network_.max_jumps = j["network"].value("max_jumps", 3);
      network_.timeout_ms = j["network"].value("timeout_ms", 50000);
      network_.user_agent =
          j["network"].value("user_agent", "SursurSpider/1.0");
      network_.strict_sur_sur_filter =
          j["network"].value("strict_sur_sur_filter", true);
    }
    if (j.contains("sources")) {
      sources_.contrahegemonic_seeds = j["sources"].value(
          "contrahegemonic_seeds", std::vector<std::string>{});
      sources_.hegemonic_contrast_seeds = j["sources"].value(
          "hegemonic_contrast_seeds", std::vector<std::string>{});
      sources_.target_keywords =
          j["sources"].value("target_keywords", std::vector<std::string>{});
    }
    if (j.contains("llm")) {
      llm_.model_name = j["llm"].value("model_name", "qwen2.5-coder:7b");
      llm_.endpoint =
          j["llm"].value("endpoint", "http://localhost:11434/api/generate");
      llm_.temperatura = j["llm"].value("temperatura", 0.2f);
      llm_.ventana_contexto = j["llm"].value("ventana_contexto", 8192);
      llm_.system_prompt = j["llm"].value("system_prompt", "");
    }
    // Override Ollama endpoint via environment variable (Docker support)
    if (const char *ollama_env = std::getenv("OLLAMA_HOST")) {
      std::string env_endpoint = std::string(ollama_env);
      if (env_endpoint.find("/api/generate") != std::string::npos) {
        env_endpoint.replace(env_endpoint.find("/api/generate"), 13,
                             "/api/chat");
      } else if (env_endpoint.find("/api/") == std::string::npos) {
        env_endpoint += "/api/chat";
      }
      llm_.endpoint = env_endpoint;
      spdlog::info("ConfigManager: OLLAMA_HOST env override: {}", env_endpoint);
    }
    if (j.contains("persistence")) {
      persistence_.sqlite_path =
          j["persistence"].value("sqlite_path", "/var/lib/sursur/db/graph.db");
    }
    if (j.contains("media")) {
      const auto &m = j["media"];
      media_.output_dir = m.value("output_dir", "/var/lib/sursur/media");
      media_.ffmpeg_path = m.value("ffmpeg_path", "/usr/bin/ffmpeg");
      media_.yt_dlp_path = m.value("yt_dlp_path", "/usr/local/bin/yt-dlp");
      media_.max_filesize_mb = m.value("max_filesize_mb", 50);
      media_.max_resolution = m.value("max_resolution", "720p");
      media_.bitrate_audio = m.value("bitrate_audio", 128);
      media_.formato_audio = m.value("formato_audio", "mp3");
      media_.formato_subtitulos = m.value("formato_subtitulos", "vtt");
    }
  } catch (const std::exception &e) {
    spdlog::error("ConfigManager: Fallo al extraer variables del JSON: {}",
                  e.what());
  }
}

void ConfigManager::volcar_configuracion() const {
  std::lock_guard<std::mutex> cerrojo(mutex_);
  spdlog::info("═══════════════════════════════════════════════════════════");
  spdlog::info("  RADAR CONFIG — MODO CONTRAHEGEMÓNICO ACTIVADO");
  spdlog::info("═══════════════════════════════════════════════════════════");
  spdlog::info("[LLM] Modelo: {}, Temp: {}", llm_.model_name, llm_.temperatura);
  spdlog::info("[Semillas] Contrahegemónicas: {}, Hegemónicas: {}",
               sources_.contrahegemonic_seeds.size(),
               sources_.hegemonic_contrast_seeds.size());
  spdlog::info("[Persistencia] SQLite en: {}",
               persistence_.sqlite_path.string());
}

} // namespace sursur
