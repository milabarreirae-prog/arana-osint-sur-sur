// ============================================================================
// Araña OSINT Sur-Sur — FeedGenerator (Generador de Feed RSS Local)
// Produce un archivo feed.xml RSS 2.0 para consumo en lectores locales
// (Thunderbird, QuiteRSS, Fluent Reader). Incluye <enclosure> mp3 y
// botones de feedback HTML [👍/👎] con enlace al servidor HTTP embebido.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <set>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace sursur {

/// Item individual del feed RSS.
struct ItemRSS {
    std::string titulo;          // Título del artículo/video + etiqueta $$AUDIO CURADO$$
    std::string url_original;    // URL original para trazabilidad
    std::string resumen_llm;     // Resumen generado por el LLM
    std::string cita_apa7;       // Cita APA 7 completa
    std::string autor;           // Autor o fuente
    std::string imagen;          // Imagen extraída oj:image
    std::string cita_relevante;  // Cita textual relevante
    std::string ruta_mp3;        // Ruta local al .mp3 (enclosure)
    std::string id_nodo;         // ID del nodo en GraphDB (para feedback)
    int64_t     tamano_mp3 = 0;  // Tamaño en bytes del mp3
    std::string fecha_pub;       // RFC-822 date string
};

/// Generador de feed RSS 2.0 local usando pugixml.
class FeedGenerator {
public:
    FeedGenerator(const std::string& base_url = "");
    ~FeedGenerator() = default;

    FeedGenerator(const FeedGenerator&) = delete;
    FeedGenerator& operator=(const FeedGenerator&) = delete;

    /// Configurar la URL base para los enlaces del feed (enclosures, links).
    void configurar_url_base(const std::string& base_url);

    /// Agregar un item al feed.
    void agregar_item(const ItemRSS& item);

    /// Escribir el feed RSS 2.0 a disco.
    /// @param ruta_feed Ruta al archivo feed.xml de salida
    /// @return true si se escribió exitosamente
    bool escribir_feed(const std::filesystem::path& ruta_feed);

    /// Agregar e inmediamente guardar un item al feed de forma incremental.
    /// @param item Item a agregar
    /// @param ruta_feed Ruta al archivo feed.xml
    /// @return true si se guardó exitosamente
    bool actualizar_item_incremental(const ItemRSS& item, const std::filesystem::path& ruta_feed);

    /// @param url URL original del contenido
    /// @param html Contenido HTML crudo para fallback heurístico
    /// @param evaluacion JSON con la evaluación del LLM
    /// @param img_url URL de la imagen principal
    /// @return true si la inyección fue exitosa
    bool inyectar_item_rss_atomico(const std::string& url, const std::string& html, const nlohmann::json& evaluacion, const std::string& img_url);

    /// Agrega una URL adicional a la descripción de un item existente cuando se detecta duplicidad.
    /// @param id_original El (URL) original bajo el cual se indexó la noticia.
    /// @param url_nueva La nueva URL que es reportada como duplicada (misma noticia).
    /// @return true si se encontró el item o se actualizó el feed exitosamente.
    void agregar_fuente_duplicada(const std::string& id_noticia_existente, const std::string& url_nueva);

    /// Obtener el número de items en el feed actual.
    size_t cantidad_items() const;

    /// Actualizar el índice OPML maestro con la categoría especificada.
    void actualizar_opml_maestro(const std::string& categoria);

private:
    std::string generar_fecha_rfc822_() const;
    std::string construir_descripcion_html_(const ItemRSS& item) const;
    std::string esterilizar_string_(const std::string& input) const;
    std::string sanitizar_xml(const std::string& input) const;

    mutable std::mutex mutex_;
    std::vector<ItemRSS> items_;
    uint16_t puerto_feedback_ = 0;
    std::string base_url_;
    std::set<std::string> categorias_conocidas_; ///< Para OPML dinámico
};


} // namespace sursur
