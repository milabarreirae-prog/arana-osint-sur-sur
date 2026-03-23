// ============================================================================
// Araña OSINT Sur-Sur — MediaProcessor (Implementación)
// Procesamiento asíncrono de medios: corte con ffmpeg, subtítulos VTT/SRT.
// ============================================================================
#include "media_processor.hpp"
#include "config_manager.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <array>
#include <random>
#include <chrono>

namespace sursur {

// ── Formateo de tiempos ────────────────────────────────────────────────────
std::string MarcaTiempo::formato_ffmpeg_inicio() const {
    int h = static_cast<int>(inicio) / 3600;
    int m = (static_cast<int>(inicio) % 3600) / 60;
    double s = inicio - (h * 3600 + m * 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setfill('0') << std::setw(2) << m << ":"
        << std::fixed << std::setprecision(3) << s;
    return oss.str();
}

std::string MarcaTiempo::formato_ffmpeg_fin() const {
    int h = static_cast<int>(fin) / 3600;
    int m = (static_cast<int>(fin) % 3600) / 60;
    double s = fin - (h * 3600 + m * 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setfill('0') << std::setw(2) << m << ":"
        << std::fixed << std::setprecision(3) << s;
    return oss.str();
}

static std::string formato_tiempo_vtt(double seg) {
    int h = static_cast<int>(seg) / 3600;
    int m = (static_cast<int>(seg) % 3600) / 60;
    double s = seg - (h * 3600 + m * 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setfill('0') << std::setw(2) << m << ":"
        << std::fixed << std::setprecision(3) << s;
    return oss.str();
}

static std::string formato_tiempo_srt(double seg) {
    int h = static_cast<int>(seg) / 3600;
    int m = (static_cast<int>(seg) % 3600) / 60;
    int si = static_cast<int>(seg) % 60;
    int ms = static_cast<int>((seg - static_cast<int>(seg)) * 1000);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setfill('0') << std::setw(2) << m << ":"
        << std::setfill('0') << std::setw(2) << si << ","
        << std::setfill('0') << std::setw(3) << ms;
    return oss.str();
}

// ── Constructor / Destructor ───────────────────────────────────────────────
MediaProcessor::MediaProcessor() {
    spdlog::info("MediaProcessor: instanciado");
}
MediaProcessor::~MediaProcessor() {
    spdlog::info("MediaProcessor: destruido");
}

std::string MediaProcessor::generar_id_trabajo_() {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::random_device rd;
    return "media-" + std::to_string(ms) + "-" + std::to_string(rd() % 900 + 100);
}

void MediaProcessor::actualizar_estado_trabajo_(
    const std::string& id, EstadoTrabajoMedia::Estado estado,
    float progreso, const std::string& mensaje) {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    auto& t = trabajos_[id];
    t.id_trabajo = id; t.estado = estado;
    t.progreso = progreso; t.mensaje = mensaje;
    for (const auto& cb : callbacks_progreso_) cb(t);
}

bool MediaProcessor::verificar_ffmpeg() {
    const auto& ruta = ConfigManager::instancia().media().ffmpeg_path;
    std::string cmd = "LC_ALL=C " + ruta.string() + " -version 2>&1";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) { spdlog::error("MediaProcessor: ffmpeg no ejecutable"); return false; }
    std::array<char, 256> buf;
    std::string out;
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    int r = pclose(p);
    if (r == 0) { spdlog::info("MediaProcessor: ffmpeg OK"); return true; }
    spdlog::error("MediaProcessor: ffmpeg no disponible"); return false;
}

bool MediaProcessor::ejecutar_ffmpeg_(const std::vector<std::string>& args,
                                       std::string& salida) {
    std::stringstream cmd;
    cmd << "LC_ALL=C " << ConfigManager::instancia().media().ffmpeg_path.string();
    for (const auto& a : args) cmd << " " << a;
    cmd << " 2>&1";
    FILE* p = popen(cmd.str().c_str(), "r");
    if (!p) { salida = "Error al ejecutar ffmpeg"; return false; }
    std::array<char, 1024> buf;
    while (fgets(buf.data(), buf.size(), p)) salida += buf.data();
    return pclose(p) == 0;
}

std::vector<std::string> MediaProcessor::construir_args_extraccion_(
    const std::string& fuente, const MarcaTiempo& marcas,
    const std::filesystem::path& destino, const std::string& formato) {
    auto br = std::to_string(ConfigManager::instancia().media().bitrate_audio) + "k";
    std::vector<std::string> a = {
        "-y", "-ss", marcas.formato_ffmpeg_inicio(),
        "-i", "\"" + fuente + "\"",
        "-t", std::to_string(marcas.duracion()), "-vn", "-b:a", br
    };
    if (formato == "opus") { a.push_back("-c:a"); a.push_back("libopus"); }
    else if (formato == "mp3") { a.push_back("-c:a"); a.push_back("libmp3lame"); }
    else if (formato == "wav") { 
        a.push_back("-c:a"); a.push_back("pcm_s16le"); 
        a.push_back("-ar"); a.push_back("16000"); 
        a.push_back("-ac"); a.push_back("1"); 
    }
    a.push_back("\"" + destino.string() + "\"");
    return a;
}

ResultadoExtraccion MediaProcessor::extraer_fragmento(
    const std::string& url_media, const MarcaTiempo& marcas,
    const std::string& formato_salida, const std::string& slug) {
    ResultadoExtraccion r;
    r.marcas = marcas;
    auto id = generar_id_trabajo_();
    actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::PROCESANDO, 0.0f,
                               "Extrayendo fragmento");
    const auto& cfg = ConfigManager::instancia().media();
    std::string fmt = formato_salida.empty() ? cfg.formato_audio : formato_salida;
    r.formato = fmt;
    
    // Usar ruta cronológica: YYYY/MM/DD/HHMM_slug/
    // Asegurar que el árbol de directorios cronológico existe
    auto ruta_base_cron = construir_ruta_cronologica(slug);
    r.ruta_archivo = ruta_base_cron / (id + "." + fmt);
    auto args = construir_args_extraccion_(url_media, marcas, r.ruta_archivo, fmt);
    std::string err;
    bool ok = ejecutar_ffmpeg_(args, err);
    if (ok && std::filesystem::exists(r.ruta_archivo)) {
        r.exitoso = true;
        r.tamano_bytes = static_cast<int64_t>(std::filesystem::file_size(r.ruta_archivo));
        actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::FINALIZADO, 1.0f, "OK");
        spdlog::info("MediaProcessor: fragmento extraído ({} bytes)", r.tamano_bytes);
    } else {
        r.exitoso = false; r.mensaje_error = "Error ffmpeg: " + err;
        actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::FALLIDO, 0.0f, r.mensaje_error);
    }
    return r;
}

ResultadoExtraccion MediaProcessor::extraer_fragmento_local(
    const std::filesystem::path& ruta, const MarcaTiempo& marcas,
    const std::string& fmt, const std::string& slug) {
    if (!std::filesystem::exists(ruta)) {
        ResultadoExtraccion r; r.exitoso = false;
        r.mensaje_error = "Archivo no encontrado: " + ruta.string();
        return r;
    }
    return extraer_fragmento(ruta.string(), marcas, fmt, slug);
}

ResultadoDescarga MediaProcessor::descargar_video(const std::string& url_media, const std::string& slug) {
    ResultadoDescarga r;
    auto id = generar_id_trabajo_();
    auto ruta_base_cron = construir_ruta_cronologica(slug);
    
    // Config de limits
    const auto& cfg = ConfigManager::instancia().media();
    
    // Formato paramétrico para la resolución base: "720p" -> "720"
    std::string max_res = cfg.max_resolution;
    if(!max_res.empty() && max_res.back() == 'p') {
        max_res.pop_back(); // elimina la p
    }
    
    // Plantilla de salida
    std::string template_salida = (ruta_base_cron / (id + ".%(ext)s")).string();
    
    std::stringstream cmd;
    cmd << "LC_ALL=C " << cfg.yt_dlp_path.string()
        << " --max-filesize " << cfg.max_filesize_mb << "M"
        << " -f \"bestvideo[height<=" << max_res << "]+bestaudio/best[height<=" << max_res << "]\""
        << " -o \"" << template_salida << "\""
        << " \"" << url_media << "\" 2>&1";

    spdlog::info("MediaProcessor: yt-dlp -> {}", cmd.str());
    
    actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::PROCESANDO, 0.0f, "Descargando video: " + slug);
    
    FILE* p = popen(cmd.str().c_str(), "r");
    if (!p) {
        r.exitoso = false;
        r.mensaje_error = "Error al iniciar yt-dlp";
        actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::FALLIDO, 0.0f, r.mensaje_error);
        return r;
    }

    std::array<char, 1024> buf;
    std::string out;
    while (fgets(buf.data(), buf.size(), p)) {
        out += buf.data();
    }
    
    int wstat = pclose(p);
    if (wstat == 0) {
        r.exitoso = true;
        // Encontrar el archivo que creó yt-dlp en esa ruta específica
        for (const auto& entry : std::filesystem::directory_iterator(ruta_base_cron)) {
            if (entry.path().string().find(id) != std::string::npos) {
                r.ruta_archivo = entry.path();
                break;
            }
        }
        actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::FINALIZADO, 1.0f, "Video descargado");
    } else {
        r.exitoso = false;
        r.mensaje_error = "yt-dlp falló: " + out;
        actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::FALLIDO, 0.0f, r.mensaje_error);
    }
    return r;
}

void MediaProcessor::limpiar_archivos_temporales(const std::filesystem::path& ruta) {
    if (std::filesystem::exists(ruta)) {
        std::error_code ec;
        std::filesystem::remove(ruta, ec);
        if (!ec) {
            spdlog::info("MediaProcessor: Archivo temporal de video limpiado -> {}", ruta.string());
        } else {
            spdlog::warn("MediaProcessor: Error limpiando temporal -> {}", ec.message());
        }
    }
}

std::string MediaProcessor::generar_contenido_vtt_(const std::vector<LineaSubtitulo>& l) {
    std::ostringstream o;
    o << "WEBVTT\nKind: captions\nLanguage: es\n\n";
    for (const auto& s : l)
        o << formato_tiempo_vtt(s.tiempo_inicio) << " --> "
          << formato_tiempo_vtt(s.tiempo_fin) << "\n" << s.texto_traducido << "\n\n";
    return o.str();
}

std::string MediaProcessor::generar_contenido_srt_(const std::vector<LineaSubtitulo>& l) {
    std::ostringstream o;
    for (const auto& s : l)
        o << s.indice << "\n" << formato_tiempo_srt(s.tiempo_inicio) << " --> "
          << formato_tiempo_srt(s.tiempo_fin) << "\n" << s.texto_traducido << "\n\n";
    return o.str();
}

ResultadoSubtitulos MediaProcessor::generar_subtitulos(
    const std::filesystem::path& ruta_audio, const std::string& idioma_origen,
    const std::string& formato) {
    ResultadoSubtitulos r;
    r.idioma_origen = idioma_origen; r.idioma_destino = "es";
    auto fmt = formato.empty() ? ConfigManager::instancia().media().formato_subtitulos : formato;
    r.formato = fmt;
    if (!std::filesystem::exists(ruta_audio)) {
        r.exitoso = false; r.mensaje_error = "Audio no encontrado"; return r;
    }
    // Punto de integración: Whisper + LLM para transcripción real
    std::vector<LineaSubtitulo> lineas = {
        {1, 0.0, 5.0, "[Original " + idioma_origen + "]", "[Requiere Whisper + LLM]"},
        {2, 5.0, 10.0, "[Continuación]", "[Integrar whisper.cpp aquí]"},
    };
    r.lineas = lineas;
    r.lineas_generadas = static_cast<int32_t>(lineas.size());
    std::string contenido = (fmt == "vtt") ? generar_contenido_vtt_(lineas) : generar_contenido_srt_(lineas);
    auto ruta_sub = ruta_audio; ruta_sub.replace_extension("." + fmt);
    r.ruta_archivo = ruta_sub;
    std::ofstream f(ruta_sub);
    if (f.is_open()) { f << contenido; r.exitoso = true; }
    else { r.exitoso = false; r.mensaje_error = "No se pudo escribir subtítulos"; }
    return r;
}

EstadoTrabajoMedia MediaProcessor::obtener_estado_trabajo(const std::string& id) const {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    auto it = trabajos_.find(id);
    return (it != trabajos_.end()) ? it->second : EstadoTrabajoMedia{};
}

void MediaProcessor::registrar_callback_progreso(CallbackProgresoMedia cb) {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    callbacks_progreso_.push_back(std::move(cb));
}

bool MediaProcessor::cancelar_trabajo(const std::string& id) {
    actualizar_estado_trabajo_(id, EstadoTrabajoMedia::Estado::FALLIDO, 0.0f, "Cancelado");
    return true;
}

// Implementación de métodos estáticos declarados en el encabezado (delegan a funciones libres)
std::string MediaProcessor::formato_tiempo_vtt_(double s) { return formato_tiempo_vtt(s); }
std::string MediaProcessor::formato_tiempo_srt_(double s) { return formato_tiempo_srt(s); }

// ── Salida cronológica: YYYY/MM/DD/HHMM_slug/ ─────────────────────────────
std::filesystem::path MediaProcessor::construir_ruta_cronologica(
    const std::string& slug) const {
    auto ahora = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(ahora);
    auto tm = *std::localtime(&t);

    std::ostringstream ruta;
    ruta << std::put_time(&tm, "%Y") << "/"
         << std::put_time(&tm, "%m") << "/"
         << std::put_time(&tm, "%d") << "/"
         << std::put_time(&tm, "%H%M") << "_" << slug;

    auto dir_base = ConfigManager::instancia().media().output_dir;
    auto ruta_completa = dir_base / ruta.str();

    // Crear directorios automáticamente
    try {
        std::filesystem::create_directories(ruta_completa);
        spdlog::debug("MediaProcessor: ruta cronológica creada: '{}'",
                      ruta_completa.string());
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("MediaProcessor: error creando ruta cronológica: {}",
                      e.what());
    }

    return ruta_completa;
}

} // namespace sursur
