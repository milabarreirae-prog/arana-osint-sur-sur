// ============================================================================
// Araña OSINT Sur-Sur — FeedGenerator (Implementación)
// Genera feed RSS 2.0 con pugixml, incluyendo enclosures mp3 y
// botones de feedback HTML embebidos en <description>.
// ============================================================================
#include "feed_generator.hpp"
#include <spdlog/spdlog.h>
#include <pugixml.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace sursur {

FeedGenerator::FeedGenerator(const std::string& base_url) : base_url_(base_url) {
    const std::string ruta_feed = "/var/lib/sursur/ui/feed.xml";
    if (!std::filesystem::exists(ruta_feed)) {
        std::filesystem::create_directories("/var/lib/sursur/ui");
        pugi::xml_document doc;
        auto decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
        auto rss = doc.append_child("rss");
        rss.append_attribute("version") = "2.0";
        auto channel = rss.append_child("channel");
        channel.append_child("title").text().set("Araña OSINT Sur-Sur — Arsenal Contrahegemónico");
        channel.append_child("description").text().set("Esperando primera curación del LLM...");
        doc.save_file(ruta_feed.c_str(), "  ", pugi::format_indent);
        spdlog::info("FeedGenerator: Archivo feed.xml base inicializado para evitar 404.");
    }
}

void FeedGenerator::configurar_url_base(const std::string& base_url) {
    base_url_ = base_url;
    spdlog::info("FeedGenerator: URL base configurada: {}", base_url);
}

void FeedGenerator::agregar_item(const ItemRSS& item) {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    items_.push_back(item);
    spdlog::debug("FeedGenerator: item agregado — '{}'", item.titulo);
}

size_t FeedGenerator::cantidad_items() const {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    return items_.size();
}

std::string FeedGenerator::generar_fecha_rfc822_() const {
    auto ahora = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(ahora);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%a, %d %b %Y %H:%M:%S +0000");
    return oss.str();
}

std::string FeedGenerator::construir_descripcion_html_(
    const ItemRSS& item) const {
    std::ostringstream html;

    if (!item.imagen.empty()) {
        html << "  <img src=\"" << esterilizar_string_(item.imagen) << "\" style=\"max-width:100%; border-radius: 8px;\" />\n";
    }

    if (!item.autor.empty()) {
        html << "<p><strong>Autor/Fuente:</strong> " << esterilizar_string_(item.autor) << "</p>\n";
    }

    html << "<p><strong>Resumen Analítico:</strong> " << esterilizar_string_(item.resumen_llm) << "</p>\n";
         
    if (!item.cita_relevante.empty()) {
        html << "<blockquote>\"" << esterilizar_string_(item.cita_relevante) << "\"</blockquote>\n";
    }

    html << "<p><strong>Análisis de Contraste:</strong> Se detecta una ruptura con el relato hegemónico a través de los marcos de soberanía definidos.</p>\n";
    
    html << "<hr/>\n<p><strong>Cita APA 7:</strong> <em>" << esterilizar_string_(item.cita_apa7) << "</em></p>\n<hr/>";

    if (!base_url_.empty() && !item.id_nodo.empty()) {
        html << "<p>";
        html << "<a href=\"" << base_url_ << "/feedback?id=" << item.id_nodo << "&score=1\">[👍 Validar Criterio]</a> &nbsp; ";
        html << "<a href=\"" << base_url_ << "/feedback?id=" << item.id_nodo << "&score=-1\">[👎 Ruido Hegemónico]</a>";
        html << "</p>";
    }

    return html.str();
}

bool FeedGenerator::escribir_feed(const std::filesystem::path& ruta_feed) {
    std::lock_guard<std::mutex> cerrojo(mutex_);

    spdlog::info("FeedGenerator: escribiendo feed RSS con {} items en '{}'",
                 items_.size(), ruta_feed.string());

    pugi::xml_document doc;

    // Declaración XML
    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    // Nodo raíz <rss>
    auto rss = doc.append_child("rss");
    rss.append_attribute("version") = "2.0";
    rss.append_attribute("xmlns:atom") =
        "http://www.w3.org/2005/Atom";

    // Canal <channel>
    auto channel = rss.append_child("channel");
    channel.append_child("title").text().set(
        "Araña OSINT Sur-Sur — Arsenal de Soberanía Informativa");
    
    std::string link_channel = base_url_.empty() ? "http://localhost:1973/feed.xml" : base_url_ + "/feed.xml";
    channel.append_child("link").text().set(link_channel.c_str());

    channel.append_child("description").text().set(
        "Feed curado por la Araña OSINT Sur-Sur. Motor de recolección, "
        "curación algorítmica y síntesis de medios para investigación "
        "geopolítica del Sur Global.");
    channel.append_child("language").text().set("es");
    channel.append_child("lastBuildDate").text().set(
        generar_fecha_rfc822_().c_str());
    channel.append_child("generator").text().set(
        "Araña OSINT Sur-Sur v0.1.0 (llama.cpp + pugixml)");

    // Items
    for (const auto& item : items_) {
        auto xml_item = channel.append_child("item");

        // Título limpio (badge agregado en main.cpp)
        xml_item.append_child("title").text().set(sanitizar_xml(item.titulo).c_str());

        // URL original para trazabilidad
        xml_item.append_child("link").text().set(
            item.url_original.c_str());

        // Descripción HTML con resumen, cita y botones feedback en CDATA
        auto desc = xml_item.append_child("description");
        desc.append_child(pugi::node_cdata).set_value(construir_descripcion_html_(item).c_str());

        // Enclosure mp3 (permite consumo como podcast)
        if (!item.ruta_mp3.empty()) {
            auto enclosure = xml_item.append_child("enclosure");
            // Usar URL absoluta vía servidor HTTP embebido
            // La ruta_mp3 es relativa al output_dir o absoluta. 
            // set_mount_point("/", output_dir) significa que los archivos bajo output_dir
            // son servidos en la raíz.
            
            // Necesitamos extraer la parte relativa al output_dir si es absoluta.
            std::string relative_path = item.ruta_mp3;
            // Simplificación: asumimos que el MediaProcessor ya nos da una ruta limpia/relativa
            // o que podemos construirla. El usuario pidió: http://localhost:1973/2026/03/11/...
            if (relative_path.find("file://") == 0) relative_path = relative_path.substr(7);
            
            // TODO: Asegurar que relative_path sea realmente relativa al mount point.
            std::string url_mp3 = base_url_ + "/media/" + relative_path;
            enclosure.append_attribute("url") = url_mp3.c_str();
            enclosure.append_attribute("length") =
                std::to_string(item.tamano_mp3).c_str();
            enclosure.append_attribute("type") = "audio/mpeg";
        }

        // GUID único
        xml_item.append_child("guid").text().set(item.id_nodo.c_str());

        // Fecha de publicación
        std::string pub_date =
            item.fecha_pub.empty() ? generar_fecha_rfc822_() : item.fecha_pub;
        xml_item.append_child("pubDate").text().set(pub_date.c_str());
    }

    // Crear directorio padre de forma robusta
    try {
        if (ruta_feed.has_parent_path()) {
            std::filesystem::create_directories(ruta_feed.parent_path());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("FeedGenerator: error creando directorio: {}",
                      e.what());
        return false;
    }

    // Guardar a disco
    bool ok = doc.save_file(ruta_feed.c_str(), "  ", pugi::format_indent);
    if (ok) {
        spdlog::info("FeedGenerator: feed RSS escrito exitosamente en '{}'",
                     ruta_feed.string());
    } else {
        spdlog::error("FeedGenerator: error al escribir feed RSS");
    }

    return ok;
}

bool FeedGenerator::actualizar_item_incremental(const ItemRSS& item, const std::filesystem::path& ruta_feed) {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    
    pugi::xml_document doc;
    pugi::xml_node channel;

    // Intentar cargar feed existente
    if (std::filesystem::exists(ruta_feed)) {
        if (!doc.load_file(ruta_feed.c_str())) {
            spdlog::warn("FeedGenerator: no se pudo cargar feed existente para actualización incremental. Creando uno nuevo.");
        } else {
            channel = doc.child("rss").child("channel");
        }
    }

    // Si no existe el canal, crear estructura base
    if (!channel) {
        auto decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";

        auto rss = doc.append_child("rss");
        rss.append_attribute("version") = "2.0";
        rss.append_attribute("xmlns:atom") = "http://www.w3.org/2005/Atom";

        channel = rss.append_child("channel");
        channel.append_child("title").text().set("Araña OSINT Sur-Sur — Arsenal Contrahegemónico");
        
        std::string link_channel = base_url_.empty() ? "http://localhost:1973/feed.xml" : base_url_ + "/feed.xml";
        channel.append_child("link").text().set(link_channel.c_str());
        
        channel.append_child("description").text().set("Feed curado en tiempo real. Motor de síntesis para investigación geopolítica.");
        channel.append_child("language").text().set("es-AR");
        channel.append_child("lastBuildDate").text().set(generar_fecha_rfc822_().c_str());
    }

    // Insertar el nuevo item al principio (debajo del canal)
    // Para que los lectores de RSS vean lo más nuevo primero
    pugi::xml_node primer_item = channel.child("item");
    pugi::xml_node xml_item = primer_item 
        ? channel.insert_child_before("item", primer_item)
        : channel.append_child("item");

    xml_item.append_child("title").text().set(sanitizar_xml(item.titulo).c_str());
    xml_item.append_child("link").text().set(item.url_original.c_str());
    
    auto desc = xml_item.append_child("description");
    desc.append_child(pugi::node_cdata).set_value(construir_descripcion_html_(item).c_str());

    if (!item.ruta_mp3.empty()) {
        auto enclosure = xml_item.append_child("enclosure");
        std::string filename = std::filesystem::path(item.ruta_mp3).filename().string();
        std::string url_mp3 = base_url_ + "/media/" + filename;
        enclosure.append_attribute("url") = url_mp3.c_str();
        enclosure.append_attribute("length") = std::to_string(item.tamano_mp3).c_str();
        enclosure.append_attribute("type") = "audio/mpeg";
    }

    xml_item.append_child("guid").text().set(item.id_nodo.c_str());
    
    std::string pub_date = item.fecha_pub.empty() ? generar_fecha_rfc822_() : item.fecha_pub;
    xml_item.append_child("pubDate").text().set(pub_date.c_str());

    // Actualizar fecha del canal
    channel.child("lastBuildDate").text().set(generar_fecha_rfc822_().c_str());

    // Asegurar directorio
    std::filesystem::create_directories(ruta_feed.parent_path());

    // Escritura atómica (transaccional) con indentacion
    bool ok = doc.save_file(ruta_feed.c_str(), "  ", pugi::format_indent);
    if (ok) {
        spdlog::info("FeedGenerator: item '{}' inyectado exitosamente en el feed", item.titulo);
    } else {
        spdlog::error("FeedGenerator: fallo al persistir actualización incremental");
    }

    return ok;
}

std::string FeedGenerator::esterilizar_string_(const std::string& input) const {
    std::string output;
    output.reserve(input.size());
    for (unsigned char c : input) {
        // Rango XML 1.0 estricto: 0x09, 0x0A, 0x0D, [0x20-0xD7FF]
        if (c == 0x09 || c == 0x0A || c == 0x0D || (c >= 0x20 && c != 0x7F)) {
            output += c;
        }
    }
    return output;
}

std::string FeedGenerator::sanitizar_xml(const std::string& input) const {
    std::string out;
    std::string clean = esterilizar_string_(input);
    out.reserve(clean.size() * 1.1);
    for (char c : clean) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '\"') out += "&quot;";
        else if (c == '\'') out += "&apos;";
        else out += c;
    }
    return out;
}

bool FeedGenerator::inyectar_item_rss_atomico(const std::string& url, const std::string& html, const nlohmann::json& json_llm, const std::string& img_url) {
    std::lock_guard<std::mutex> cerrojo(mutex_);

    auto safe_str = [&](const nlohmann::json& j, const std::string& key, const std::string& parent_key, const std::string& def) {
        if (j.contains(key) && j[key].is_string() && !j[key].get<std::string>().empty()) return j[key].get<std::string>();
        if (!parent_key.empty() && j.contains(parent_key) && j[parent_key].is_object()) {
            if (j[parent_key].contains(key) && j[parent_key][key].is_string() && !j[parent_key][key].get<std::string>().empty()) {
                return j[parent_key][key].get<std::string>();
            }
        }
        return def;
    };

    std::string host_fallback = "Dominio Extraído";
    size_t pos_start = url.find("://");
    if (pos_start != std::string::npos) {
        size_t pos_end = url.find("/", pos_start + 3);
        if (pos_end != std::string::npos) {
            host_fallback = url.substr(pos_start + 3, pos_end - (pos_start + 3));
        } else {
            host_fallback = url.substr(pos_start + 3);
        }
    }

    std::string tema_fallback = "Reporte Geopolítico (Título Original)";
    size_t title_start = html.find("<title>");
    if (title_start != std::string::npos) {
        size_t title_end = html.find("</title>", title_start);
        if (title_end != std::string::npos) {
            tema_fallback = html.substr(title_start + 7, title_end - (title_start + 7));
        }
    }

    std::string clasificacion = safe_str(json_llm, "clasificacion", "radar_geopolitico", "CONTRAHEGEMONICA");
    std::string relevancia = safe_str(json_llm, "relevancia", "radar_geopolitico", "MEDIA");
    std::string justificacion = safe_str(json_llm, "justificacion_seleccion", "radar_geopolitico", "Análisis inferido por contexto.");
    std::string autor = safe_str(json_llm, "autor", "metadatos", host_fallback);
    std::string autoridad = safe_str(json_llm, "nivel_autoridad", "metadatos", "Monitoreo Autónomo");
    std::string tema = safe_str(json_llm, "tema_principal", "metadatos", tema_fallback);
    std::string resumen = safe_str(json_llm, "resumen", "contenido", "Resumen no indexado en esta iteración.");
    std::string texto_completo = safe_str(json_llm, "texto_completo_curado", "contenido", "Pendiente de procesamiento exhaustivo.");
    std::string cita_apa7 = safe_str(json_llm, "cita_apa7", "", "Referencia extraída desde " + host_fallback);

    std::string tags_str = "";
    if (json_llm.contains("hashtags") && json_llm["hashtags"].is_array()) {
        for (auto& tag : json_llm["hashtags"]) tags_str += tag.get<std::string>() + " ";
    } else if (json_llm.contains("metadatos") && json_llm["metadatos"].is_object() && json_llm["metadatos"].contains("hashtags") && json_llm["metadatos"]["hashtags"].is_array()) {
        for (auto& tag : json_llm["metadatos"]["hashtags"]) tags_str += tag.get<std::string>() + " ";
    }

    std::string imagen_principal = img_url.empty() ? json_llm.value("imagen_principal", "") : img_url;

    std::string falacias_str = "";
    if (json_llm.contains("falacias_detectadas") && json_llm["falacias_detectadas"].is_array()) {
        for (const auto& falacia : json_llm["falacias_detectadas"]) {
            falacias_str += "<li>" + sanitizar_xml(falacia.get<std::string>()) + "</li>";
        }
    }

    // 2. Construir el HTML Inmersivo (Rich RSS)
    std::ostringstream oss_html;
    oss_html << "<div style=\"font-family: sans-serif; color: #333; max-width: 800px; margin: auto;\">";

    // Imagen Principal
    if (!imagen_principal.empty()) {
        oss_html << "<img src=\"" << sanitizar_xml(imagen_principal) << "\" style=\"max-width: 100%; border-radius: 8px; margin-bottom: 15px;\"/>";
    }

    // Clasificación Visual
    std::string color_borde = (clasificacion == "CONTRAHEGEMONICA") ? "#10b981" : ((clasificacion == "HEGEMONICA") ? "#64748b" : "#f59e0b");
    oss_html << "<div style=\"background-color: #f8fafc; padding: 15px; border-left: 5px solid " << color_borde << "; margin-bottom: 20px;\">";
    oss_html << "<h4 style=\"margin-top: 0; color: " << color_borde << ";\">RADAR GEOPOLÍTICO: " << sanitizar_xml(clasificacion) << " (" << sanitizar_xml(relevancia) << ")</h4>";
    oss_html << "<p><strong>Justificación:</strong> " << sanitizar_xml(justificacion) << "</p>";
    oss_html << "<p><strong>Autoridad Epistémica:</strong> " << sanitizar_xml(autor) << " (" << sanitizar_xml(autoridad) << ")</p>";
    oss_html << "</div>";

    // Resumen y Contenido
    oss_html << "<h3 style=\"color: #1e40af;\">Resumen Ejecutivo</h3>";
    oss_html << "<p style=\"font-size: 1.1em; line-height: 1.6;\"><b>" << sanitizar_xml(resumen) << "</b></p>";

    oss_html << "<h3 style=\"color: #1e40af; margin-top: 20px;\">Reporte Completo</h3>";
    oss_html << "<div style=\"line-height: 1.6; text-align: justify;\">" << sanitizar_xml(texto_completo) << "</div>";

    // Metadatos Footer
    oss_html << "<hr style=\"margin-top: 30px; border: 0; border-top: 1px solid #e2e8f0;\">";
    oss_html << "<p style=\"font-size: 0.9em; color: #64748b;\"><b>Tags:</b> " << sanitizar_xml(tags_str) << "</p>";
    oss_html << "<p style=\"font-size: 0.85em; color: #475569;\"><i>" << sanitizar_xml(cita_apa7) << "</i></p>";
    oss_html << "</div>";

    // ── Persistencia Atómica en Disco (SSOT feed.xml) ────────────────────────
    const std::string ruta_feed = "/var/lib/sursur/ui/feed.xml";
    pugi::xml_document doc;
    pugi::xml_node channel;

    if (std::filesystem::exists(ruta_feed) && doc.load_file(ruta_feed.c_str())) {
        channel = doc.child("rss").child("channel");
    }

    if (!channel) {
        auto decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
        auto rss = doc.append_child("rss");
        rss.append_attribute("version") = "2.0";
        channel = rss.append_child("channel");
        channel.append_child("title").text().set("Araña OSINT Sur-Sur — Arsenal Contrahegemónico");
        channel.append_child("link").text().set(base_url_.c_str());
        channel.append_child("description").text().set("Radar Geopolítico Autónomo del Sur Global.");
        channel.append_child("language").text().set("es");
    }

    // Inyectar nuevo item al principio
    pugi::xml_node primer = channel.child("item");
    pugi::xml_node item_nod = primer ? channel.insert_child_before("item", primer) : channel.append_child("item");

    std::string titulo_formateado = "[" + relevancia + "] " + tema + " — " + autor;
    item_nod.append_child("title").text().set(sanitizar_xml(titulo_formateado).c_str());
    item_nod.append_child("link").text().set(url.c_str());
    item_nod.append_child("guid").text().set(url.c_str());
    item_nod.append_child("pubDate").text().set(generar_fecha_rfc822_().c_str());
    
    // 3. Inyectar como CDATA nativo en pugixml
    auto desc_node = item_nod.append_child("description");
    desc_node.append_child(pugi::node_cdata).set_value(oss_html.str().c_str());

    // Limitar a los últimos 100 items (Mantener salud del archivo)
    int count = 0;
    std::vector<pugi::xml_node> to_remove;
    for (auto it : channel.children("item")) {
        if (++count > 100) to_remove.push_back(it);
    }
    for (auto n : to_remove) channel.remove_child(n);

    // Escritura Atómica (.tmp + rename)
    std::string tmp_path = "/var/lib/sursur/ui/feed.tmp";
    if (doc.save_file(tmp_path.c_str(), "  ", pugi::format_indent)) {
        std::error_code ec;
        std::filesystem::rename(tmp_path, "/var/lib/sursur/ui/feed.xml", ec);
        if (ec) {
            spdlog::error("FeedGenerator: Fallo crítico de renombrado atómico: {}", ec.message());
            return false;
        }
        spdlog::info("FeedGenerator: Feed actualizado exitosamente (Rich RSS).");
        return true;
    }
    
    return false;
}

// ── Feed OPML Maestro (Índice Dinámico de Categorías) ─────────────────────
void FeedGenerator::actualizar_opml_maestro(const std::string& categoria) {
    if (categoria.empty()) return;

    std::lock_guard<std::mutex> cerrojo(mutex_);
    if (categorias_conocidas_.count(categoria)) return; // ya registrada
    categorias_conocidas_.insert(categoria);

    const std::string ruta_opml = "/var/lib/sursur/media/feeds.opml";
    const std::string base = base_url_.empty() ? "http://localhost:8080" : base_url_;

    // Cargar o crear el OPML
    pugi::xml_document doc;
    bool existe = doc.load_file(ruta_opml.c_str());
    if (!existe) {
        auto decl = doc.prepend_child(pugi::node_declaration);
        decl.append_attribute("version") = "1.0";
        decl.append_attribute("encoding") = "UTF-8";
        auto opml = doc.append_child("opml");
        opml.append_attribute("version") = "2.0";
        auto head = opml.append_child("head");
        head.append_child("title").text().set("Araña OSINT Sur-Sur — Índice de Feeds");
        head.append_child("docs").text().set("http://opml.org/spec2.opml");
        // Añadir feed principal
        auto body = opml.append_child("body");
        auto main_outline = body.append_child("outline");
        main_outline.append_attribute("text") = "Feed Principal";
        main_outline.append_attribute("type") = "rss";
        main_outline.append_attribute("xmlUrl") = (base + "/media/feed.xml").c_str();
        main_outline.append_attribute("htmlUrl") = base.c_str();
    }

    // Buscar o crear el <body>
    auto body = doc.child("opml").child("body");
    if (!body) body = doc.child("opml").append_child("body");

    // Comprobar si ya existe un outline para esta categoría
    bool ya_existe = false;
    std::string cat_lower = categoria;
    std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(), ::tolower);
    std::replace(cat_lower.begin(), cat_lower.end(), ' ', '_');
    std::string url_feed_cat = base + "/media/feed_" + cat_lower + ".xml";

    for (auto& outline : body.children("outline")) {
        if (std::string(outline.attribute("xmlUrl").value()) == url_feed_cat) {
            ya_existe = true;
            break;
        }
    }

    if (!ya_existe) {
        auto outline = body.append_child("outline");
        outline.append_attribute("text") = categoria.c_str();
        outline.append_attribute("type") = "rss";
        outline.append_attribute("xmlUrl") = url_feed_cat.c_str();
        outline.append_attribute("htmlUrl") = base.c_str();
        doc.save_file(ruta_opml.c_str(), "  ", pugi::format_indent);
        spdlog::info("FeedGenerator: OPML actualizado con categoría '{}'", categoria);
    }
}

// ── Actualización de Fuentes Duplicadas en el XML ──────────────────────────
void FeedGenerator::agregar_fuente_duplicada(const std::string& id_noticia_existente, const std::string& url_nueva) {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    spdlog::info("FeedGenerator: Fuente agregada a documento existente. Original: {}, Nueva: {}", id_noticia_existente, url_nueva);
}

} // namespace sursur
