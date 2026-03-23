// ============================================================================
// Araña OSINT Sur-Sur — ConfigManager (Header)
// SSOT: Carga dinámica desde sursur_config.json
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace sursur {

struct NetworkConfig {
    int max_jumps = 3;
    int timeout_ms = 5000;
    std::string user_agent;
    bool strict_sur_sur_filter = true;
};

struct SourcesConfig {
    std::vector<std::string> contrahegemonic_seeds;
    std::vector<std::string> hegemonic_contrast_seeds;
    std::vector<std::string> target_keywords;
};

struct LLMConfig {
    std::string model_name = "qwen2.5-coder:7b";
    std::string endpoint = "http://localhost:11434/api/generate";
    float temperatura = 0.2f;
    int ventana_contexto = 8192;
    std::string system_prompt;
};

struct PersistenceConfig {
    std::filesystem::path sqlite_path = "/var/lib/sursur/db/graph.db";
};

struct MediaConfig {
    std::filesystem::path output_dir = "/var/lib/sursur/media";
    std::filesystem::path ffmpeg_path = "/usr/bin/ffmpeg";
    std::filesystem::path yt_dlp_path = "/usr/local/bin/yt-dlp";
    int max_filesize_mb = 50;
    std::string max_resolution = "720p";
    int bitrate_audio = 128;
    std::string formato_audio = "mp3";
    std::string formato_subtitulos = "vtt";
};

class ConfigManager {
public:
    static ConfigManager& instancia();

    bool cargar_configuracion(const std::filesystem::path& ruta_json = "/etc/sursur/sursur_config.json");
    void volcar_configuracion() const;

    NetworkConfig network() const { std::lock_guard<std::mutex> lk(mutex_); return network_; }
    SourcesConfig sources() const { std::lock_guard<std::mutex> lk(mutex_); return sources_; }
    LLMConfig llm() const { std::lock_guard<std::mutex> lk(mutex_); return llm_; }
    PersistenceConfig persistence() const { std::lock_guard<std::mutex> lk(mutex_); return persistence_; }
    MediaConfig media() const { std::lock_guard<std::mutex> lk(mutex_); return media_; }

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void parsing_json_to_structs_(const nlohmann::json& j);

    mutable std::mutex mutex_;
    NetworkConfig network_;
    SourcesConfig sources_;
    LLMConfig llm_;
    PersistenceConfig persistence_;
    MediaConfig media_;
};

} // namespace sursur
