// ============================================================================
// Araña OSINT Sur-Sur — MediaProcessor (Procesador de Medios)
// Extracción quirúrgica de fragmentos de audio/video con ffmpeg.
// Generación de subtítulos VTT/SRT en español sincronizados con el audio
// original, independientemente del idioma de entrada.
// ============================================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>

namespace sursur {

/// Marca de tiempo para un fragmento de medios.
struct MarcaTiempo {
    double inicio = 0.0;   // Segundos desde el inicio
    double fin = 0.0;      // Segundos desde el inicio

    /// Duración del fragmento en segundos.
    double duracion() const { return fin - inicio; }

    /// Formato de tiempo para ffmpeg (HH:MM:SS.mmm)
    std::string formato_ffmpeg_inicio() const;
    std::string formato_ffmpeg_fin() const;
};

/// Resultado de la extracción de un fragmento de audio.
struct ResultadoExtraccion {
    std::filesystem::path ruta_archivo;
    std::string formato;               // "opus", "mp3", "wav"
    MarcaTiempo marcas;
    std::string idioma_original;
    int64_t     tamano_bytes = 0;
    bool        exitoso = false;
    std::string mensaje_error;
};

/// Línea individual de subtítulo.
struct LineaSubtitulo {
    int32_t indice = 0;
    double  tiempo_inicio = 0.0;       // Segundos
    double  tiempo_fin = 0.0;          // Segundos
    std::string texto_original;        // Texto en idioma original
    std::string texto_traducido;       // Texto traducido a español
};

/// Resultado de la generación de subtítulos.
struct ResultadoSubtitulos {
    std::filesystem::path ruta_archivo;
    std::string formato;               // "vtt" o "srt"
    std::string idioma_origen;
    std::string idioma_destino;        // Siempre "es"
    int32_t     lineas_generadas = 0;
    std::vector<LineaSubtitulo> lineas;
    bool        exitoso = false;
    std::string mensaje_error;
};

/// Resultado de descarga pura yt-dlp
struct ResultadoDescarga {
    std::filesystem::path ruta_archivo;
    bool exitoso = false;
    std::string mensaje_error;
};

/// Estado de un trabajo de procesamiento asíncrono.
struct EstadoTrabajoMedia {
    std::string id_trabajo;
    enum class Estado {
        PENDIENTE,
        PROCESANDO,
        FINALIZADO,
        FALLIDO
    };
    Estado  estado = Estado::PENDIENTE;
    float   progreso = 0.0f;          // 0.0 — 1.0
    std::string mensaje;
};

/// Callback de progreso para trabajos de medios.
using CallbackProgresoMedia = std::function<void(const EstadoTrabajoMedia&)>;

// ============================================================================
// Procesador Asíncrono de Medios
// ============================================================================
class MediaProcessor {
public:
    MediaProcessor();
    ~MediaProcessor();

    /// Prohibir copia.
    MediaProcessor(const MediaProcessor&) = delete;
    MediaProcessor& operator=(const MediaProcessor&) = delete;

    /// Verificar que ffmpeg esté disponible en la ruta configurada.
    /// @return true si ffmpeg es accesible y funcional.
    bool verificar_ffmpeg();

    /// Extraer un fragmento de audio/video por marcas de tiempo.
    /// @param url_media URL del contenido multimedia (YouTube, PeerTube, etc.)
    /// @param marcas Marcas de tiempo de inicio y fin
    /// @param formato_salida Formato de audio deseado ("opus", "mp3", "wav")
    /// @return Resultado de la extracción
    ResultadoExtraccion extraer_fragmento(const std::string& url_media,
                                           const MarcaTiempo& marcas,
                                           const std::string& formato_salida = "",
                                           const std::string& slug = "extracción");

    /// Extraer fragmento desde un archivo local.
    /// @param ruta_local Ruta al archivo de audio/video local
    /// @param marcas Marcas de tiempo de inicio y fin
    /// @param formato_salida Formato de audio deseado
    /// @return Resultado de la extracción
    ResultadoExtraccion extraer_fragmento_local(const std::filesystem::path& ruta_local,
                                                 const MarcaTiempo& marcas,
                                                 const std::string& formato_salida = "",
                                                 const std::string& slug = "extracción_local");

    /// Generar subtítulos VTT/SRT en español para un archivo de audio.
    /// El audio se mantiene en su idioma original; los subtítulos son
    /// la traducción sincronizada al español.
    /// @param ruta_audio Ruta al archivo de audio
    /// @param idioma_origen Código ISO 639 del idioma del audio
    /// @param formato Formato de subtítulos ("vtt" o "srt")
    /// @return Resultado con el archivo de subtítulos generado
    ResultadoSubtitulos generar_subtitulos(const std::filesystem::path& ruta_audio,
                                            const std::string& idioma_origen,
                                            const std::string& formato = "");

    /// Obtener el estado de un trabajo de procesamiento.
    EstadoTrabajoMedia obtener_estado_trabajo(const std::string& id_trabajo) const;

    /// Registrar callback de progreso.
    void registrar_callback_progreso(CallbackProgresoMedia callback);

    /// Cancelar un trabajo en curso.
    bool cancelar_trabajo(const std::string& id_trabajo);

    /// Construir ruta de salida cronológica.
    /// @param slug Nombre código del contenido (ej. "analisis_decolonial_bolivia")
    /// @return Ruta tipo YYYY/MM/DD/HHMM_slug/
    std::filesystem::path construir_ruta_cronologica(
        const std::string& slug) const;

    /// Descargar video completo controlando límites via yt-dlp.
    ResultadoDescarga descargar_video(const std::string& url_media, const std::string& slug = "descarga");

    /// Borrar archivos temporales de forma segura.
    void limpiar_archivos_temporales(const std::filesystem::path& ruta);

private:
    // ── Operaciones ffmpeg ─────────────────────────────────────────────────
    // Ejecutar comando ffmpeg como subproceso
    bool ejecutar_ffmpeg_(const std::vector<std::string>& argumentos,
                           std::string& salida_stderr);

    // Construir argumentos ffmpeg para extracción de fragmento
    std::vector<std::string> construir_args_extraccion_(
        const std::string& fuente,
        const MarcaTiempo& marcas,
        const std::filesystem::path& destino,
        const std::string& formato);

    // ── Generación de subtítulos ───────────────────────────────────────────
    // Generar contenido VTT desde líneas de subtítulo
    std::string generar_contenido_vtt_(const std::vector<LineaSubtitulo>& lineas);

    // Generar contenido SRT desde líneas de subtítulo
    std::string generar_contenido_srt_(const std::vector<LineaSubtitulo>& lineas);

    // Formatear tiempo para VTT (HH:MM:SS.mmm)
    static std::string formato_tiempo_vtt_(double segundos);

    // Formatear tiempo para SRT (HH:MM:SS,mmm)
    static std::string formato_tiempo_srt_(double segundos);

    // ── Gestión de trabajos ────────────────────────────────────────────────
    std::string generar_id_trabajo_();
    void actualizar_estado_trabajo_(const std::string& id,
                                     EstadoTrabajoMedia::Estado estado,
                                     float progreso,
                                     const std::string& mensaje = "");

    // ── Estado interno ─────────────────────────────────────────────────────
    mutable std::mutex mutex_;
    std::unordered_map<std::string, EstadoTrabajoMedia> trabajos_;
    std::vector<CallbackProgresoMedia> callbacks_progreso_;
};

} // namespace sursur
