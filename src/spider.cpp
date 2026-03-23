#include "spider.hpp"
#include "graph_connector.hpp"
#include "feed_generator.hpp"
#include <chrono>
#include <ctime>
#include <stack>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <curl/curl.h>
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <cstring>
#include <strings.h>

namespace sursur {

std::string Spider::obtener_host(const std::string& url) {
    CURLU *h = curl_url();
    std::string host_str = "";
    if (h && curl_url_set(h, CURLUPART_URL, url.c_str(), 0) == CURLUE_OK) {
        char *host = nullptr;
        if (curl_url_get(h, CURLUPART_HOST, &host, 0) == CURLUE_OK && host) {
            host_str = host;
            curl_free(host);
        }
    }
    if (h) curl_url_cleanup(h);
    return host_str;
}

std::string Spider::obtener_path(const std::string& url) {
    CURLU *h = curl_url();
    std::string path_str = "/";
    if (h && curl_url_set(h, CURLUPART_URL, url.c_str(), 0) == CURLUE_OK) {
        char *path = nullptr;
        if (curl_url_get(h, CURLUPART_PATH, &path, 0) == CURLUE_OK && path) {
            path_str = path;
            curl_free(path);
        }
    }
    if (h) curl_url_cleanup(h);
    return path_str;
}

std::string Spider::iniciar_rastreo(const std::string& url_rss, int max_saltos) {
    // Stub simple para compatibilidad gRPC
    std::string dominio = obtener_host(url_rss);
    std::string path = obtener_path(url_rss);
    urls_pendientes_.push({url_rss, dominio, path, 0});
    return "rastreo-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

EstadoSesionRastreo Spider::obtener_estado(const std::string& id_rastreo) {
    EstadoSesionRastreo est;
    est.id_rastreo = id_rastreo;
    est.estado = EstadoSesionRastreo::Estado::RASTREANDO;
    est.urls_descubiertas = 0; 
    est.urls_procesadas = urls_procesadas_.load();
    est.profundidad_actual = 0;
    return est;
}

void Spider::iniciar_bucle(size_t num_hilos) {
    spdlog::info("Spider: arrancando {} hilo(s) de rastreo", num_hilos);
    for (size_t i = 0; i < num_hilos; ++i) {
        hilos_trabajadores_.emplace_back([this, i](std::stop_token stoken) {
            spdlog::info("Spider[{}]: hilo iniciado, esperando URLs...", i);
            int ciclo = 0;
            while (!stoken.stop_requested()) {
                LinkItem current_link;
                if (urls_pendientes_.try_pop(current_link)) {
                    if (!marcar_como_visitada(current_link.url)) {
                        spdlog::debug("Spider[{}]: URL ya visitada, saltando: {}", i, current_link.url);
                        continue;
                    }
                    urls_procesadas_++;
                    spdlog::info("Spider[{}]: descargando [{}] {}", i,
                        (int)urls_procesadas_.load(),
                        current_link.url.substr(0, 100));

                    // 📡 Verificación de Persistencia Absoluta (Evitar Eco Hegemónico)
                    if (grafo_ && grafo_->url_ya_visitada(current_link.url)) {
                        spdlog::debug("Spider[{}]: Radar detectó duplicado histórico, omitiendo descarga -> {}", i, current_link.url);
                        continue;
                    }

                    std::string html = descargar_url(current_link.url, stoken);
                    if (html.empty()) {
                        spdlog::warn("Spider[{}]: descarga vacía o fallida -> {}", i, current_link.url.substr(0, 80));
                        continue;
                    }
                    spdlog::info("Spider[{}]: descargados {} bytes de {}", i, html.size(), current_link.url.substr(0, 70));
                    descargas_exitosas_++;
                    bytes_descargados_ += html.size();

                    extraer_y_encolar_enlaces(html, current_link);

                    auto noticia_opt = analizar_noticia(html, current_link.url);
                    if (!noticia_opt) {
                        // El descarte por heurística ya fue logueado en analizar_noticia
                        continue;
                    }
                    spdlog::info("Spider[{}]: NOTICIA extraída — '{}'", i,
                        noticia_opt->titulo.substr(0, 80));

                    std::string texto = noticia_opt->titulo + "\n\n" + noticia_opt->cuerpo_plano;

                    // Deduplicación Vectorial Activa
                    if (grafo_) {
                        std::vector<float> mock_vec(384, 0.01f); // ToDo: RAG Pipeline Embeddings reales
                        auto dup_id = grafo_->buscar_similitud_vectorial(mock_vec, 0.15f);
                        if (dup_id) {
                            spdlog::info("Spider[{}]: Duplicado vectorial -> {}", i, *dup_id);
                            grafo_->vincular_fuente_duplicada(*dup_id, current_link.url);
                            if (feed_gen_) feed_gen_->agregar_fuente_duplicada(*dup_id, current_link.url);
                            continue;
                        }

                        std::unordered_map<std::string, std::string> props = {
                            {"url", current_link.url},
                            {"longitud_texto", std::to_string(texto.size())},
                            {"profundidad", std::to_string(current_link.saltos)}
                        };
                        grafo_->agregar_nodo(TipoNodo::DOCUMENTO, current_link.url, props);
                    }

                    // Encolar texto limpio (Para legacy y debug)
                    resultados_procesados_.push(texto);

                    // Disparar Callback IA EvaluatorLLM (Inferencia en Cola Asincrona)
                    if (callback_) {
                        ResultadoRastreo res;
                        res.exitoso = true;
                        res.url = current_link.url;
                        res.contenido_texto = texto;
                        res.html_crudo = html;
                        res.imagen_principal = noticia_opt->imagen_principal;
                        res.idioma_detectado = "es";
                        res.profundidad = current_link.saltos;
                        callback_(res);
                        if (grafo_) grafo_->registrar_url(current_link.url);
                        spdlog::info("Spider[{}]: Inteligencia transferida al motor de evaluación LLM -> {} (Img: {})", i, current_link.url, res.imagen_principal.substr(0, 30));
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });
    }
}

void Spider::detener_todos() {
    parada_solicitada_.request_stop();
}

void Spider::cargar_semillas(const std::string& ruta) {
    std::ifstream archivo(ruta);
    if (!archivo.is_open()) return;

    std::string linea;
    while (std::getline(archivo, linea)) {
        if (!linea.empty() && linea.find("http") == 0) {
            // Strip right comment
            auto pos_hash = linea.find('#');
            if (pos_hash != std::string::npos) {
                linea = linea.substr(0, pos_hash);
            }
            // Trim whitespace right
            while (!linea.empty() && std::isspace(linea.back())) {
                linea.pop_back();
            }

            if (!linea.empty()) {
                std::string dominio = obtener_host(linea);
                std::string path    = obtener_path(linea);
                urls_pendientes_.push({linea, dominio, path, 0});
            }
        }
    }
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total_size);
    return total_size;
}

struct CurlProgressData {
    const std::stop_token* stoken;
};

static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (clientp) {
        auto* data = static_cast<CurlProgressData*>(clientp);
        if (data->stoken && data->stoken->stop_requested()) {
            return 1; // Abort
        }
    }
    return 0; // Continuar
}

std::string Spider::descargar_url(const std::string& url, std::stop_token stoken) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string buffer;
    CurlProgressData prog_data{&stoken};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 SursurSpider/0.1.0");

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog_data);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        spdlog::error("Spider: Curl falló [CURLcode={}] para {} -> {}", (int)res, url.substr(0, 80), curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "";
    }

    return buffer;
}

std::optional<NoticiaExtraida> Spider::analizar_noticia(const std::string& html, const std::string& url) {
    if (html.empty()) return std::nullopt;

    NoticiaExtraida noticia;
    noticia.url = url;

    // 1. Extracción de Imagen Principal (og:image) vía find (O(n))
    size_t pos_og = html.find("og:image");
    if (pos_og != std::string::npos) {
        size_t pos_content = html.find("content=\"", pos_og);
        if (pos_content != std::string::npos && pos_content < pos_og + 100) {
            size_t start = pos_content + 9;
            size_t end = html.find("\"", start);
            if (end != std::string::npos) {
                noticia.imagen_principal = html.substr(start, end - start);
            }
        }
    }

    // 2. Máquina de Estados Finitos (FSM) O(n) para Texto Plano
    enum class State { TEXT, TAG, SCRIPT, STYLE, COMMENT };
    State state = State::TEXT;
    
    std::string body_text;
    body_text.reserve(html.size());
    std::string current_tag;
    
    bool last_was_space = false;
    int paragraph_count = 0;

    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];

        switch (state) {
            case State::TEXT:
                if (c == '<') {
                    state = State::TAG;
                    current_tag.clear();
                } else {
                    if (std::isspace(c)) {
                        if (!last_was_space) {
                            body_text += ' ';
                            last_was_space = true;
                        }
                    } else {
                        body_text += c;
                        last_was_space = false;
                    }
                }
                break;

            case State::TAG:
                if (c == '>') {
                    std::string tag_lower = current_tag;
                    std::transform(tag_lower.begin(), tag_lower.end(), tag_lower.begin(), ::tolower);
                    
                    if (tag_lower.starts_with("script")) state = State::SCRIPT;
                    else if (tag_lower.starts_with("style")) state = State::STYLE;
                    else if (tag_lower.starts_with("!--")) state = State::COMMENT;
                    else {
                        // Inyectar espacio al cerrar etiquetas de bloque para evitar concatenación de palabras
                        if (tag_lower == "/p" || tag_lower == "/div" || tag_lower == "br") {
                            if (!last_was_space) {
                                body_text += ' ';
                                last_was_space = true;
                            }
                            if (tag_lower == "/p") paragraph_count++;
                        }
                        state = State::TEXT;
                    }
                } else {
                    current_tag += c;
                }
                break;

            case State::SCRIPT:
                if (c == '<' && i + 8 < html.size() && 
                    (strncasecmp(&html[i], "</script>", 9) == 0)) {
                    i += 8; state = State::TEXT;
                }
                break;

            case State::STYLE:
                if (c == '<' && i + 7 < html.size() && 
                    (strncasecmp(&html[i], "</style>", 8) == 0)) {
                    i += 7; state = State::TEXT;
                }
                break;

            case State::COMMENT:
                if (c == '-' && i + 2 < html.size() && 
                    (strncmp(&html[i], "-->", 3) == 0)) {
                    i += 2; state = State::TEXT;
                }
                break;
        }
    }

    // 3. Heurística Anti-Portal (Geopolítica)
    if (body_text.size() < 800) {
        spdlog::info("Spider: Descartado por Heurística Anti-Portal (Size={}, Pars={}) -> {}", body_text.size(), paragraph_count, url);
        return std::nullopt;
    }

    noticia.cuerpo_plano = body_text;
    
    // Título fallback: primera línea o substr
    size_t first_nl = body_text.find('\n');
    noticia.titulo = body_text.substr(0, std::min(first_nl, (size_t)80));
    if (noticia.titulo.empty()) noticia.titulo = "Noticia sin título";

    // Limitar tamaño para evitar desbordamiento en Ollama
    if (noticia.cuerpo_plano.size() > 25000) {
        noticia.cuerpo_plano.resize(25000);
    }

    return noticia;
}

std::string Spider::extraer_texto(const std::string& html_crudo) {
    auto noticia = analizar_noticia(html_crudo, "temp_url");
    if (noticia) return noticia->cuerpo_plano;
    return "";
}

std::string Spider::obtener_resultado_procesado() {
    std::string result;
    if (resultados_procesados_.try_pop(result)) {
        return result;
    }
    return "";
}

bool Spider::marcar_como_visitada(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_visitadas_);
    return urls_visitadas_.insert(url).second;
}

void Spider::extraer_y_encolar_enlaces(const std::string& html_crudo, const LinkItem& padre) {
    if (html_crudo.empty() || padre.url.empty()) return;

    pugi::xml_document doc;
    if (!doc.load_string(html_crudo.c_str(), pugi::parse_default | pugi::parse_fragment)) {
        return;
    }

    CURLU *h_url_base = curl_url();
    if (!h_url_base) return;
    
    if (curl_url_set(h_url_base, CURLUPART_URL, padre.url.c_str(), 0) != CURLUE_OK) {
        curl_url_cleanup(h_url_base);
        return;
    }

    auto es_extension_ignorada = [](const std::string& path) {
        std::string p = path;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        const std::vector<std::string> exts = {
            ".jpg", ".jpeg", ".png", ".gif", ".svg", ".css", ".js", ".ico",
            ".mp4", ".mp3", ".pdf", ".zip", ".tar", ".gz", ".docx", ".webp"
        };
        for (const auto& ext : exts) {
            if (p.size() >= ext.size() && p.compare(p.size() - ext.size(), ext.size(), ext) == 0) {
                return true;
            }
        }
        return false;
    };

    std::stack<pugi::xml_node> pila;
    pila.push(doc.first_child());

    while (!pila.empty()) {
        pugi::xml_node nodo = pila.top(); pila.pop();
        std::string tag = nodo.name();
        std::string link = "";

        // Soporte HTML <a>
        if (tag == "a") {
            pugi::xml_attribute href = nodo.attribute("href");
            if (href) link = href.value();
        } 
        // Soporte Atom/RSS <link>
        else if (tag == "link") {
            pugi::xml_attribute href = nodo.attribute("href");
            if (href) {
                link = href.value(); // Formato Atom
            } else {
                // Formato RSS 2.0: <link>http...</link>
                link = std::string(nodo.child_value());
                // Evitamos saltos de línea y basura
                link.erase(0, link.find_first_not_of(" \n\r\t"));
                link.erase(link.find_last_not_of(" \n\r\t") + 1);
            }
        }

        if (!link.empty() && link.find("mailto:") == std::string::npos && 
            link.find("javascript:") == std::string::npos &&
            link.find("tel:") == std::string::npos) {
            
            CURLU *h = curl_url_dup(h_url_base);
            if (h && curl_url_set(h, CURLUPART_URL, link.c_str(), 0) == CURLUE_OK) {
                char *url_abs = nullptr;
                if (curl_url_get(h, CURLUPART_URL, &url_abs, 0) == CURLUE_OK && url_abs) {
                    std::string absolute_url = url_abs;
                    curl_free(url_abs);

                    char *path_extr = nullptr;
                    std::string extract_path = "/";
                    if (curl_url_get(h, CURLUPART_PATH, &path_extr, 0) == CURLUE_OK && path_extr) {
                        extract_path = path_extr;
                        curl_free(path_extr);
                    }
                    
                    char *host_extr = nullptr;
                    std::string host = "";
                    if (curl_url_get(h, CURLUPART_HOST, &host_extr, 0) == CURLUE_OK && host_extr) {
                        host = host_extr;
                        curl_free(host_extr);
                    }

                    if (!es_extension_ignorada(extract_path)) {
                        /* Ignorar subdominios de youtube y feed en sí para evitar ciclos, la recursión RSS es inútil 
                           para OSINT a menos que sea la noticia final. */
                        if (absolute_url.find("/feeds/") == std::string::npos && 
                            absolute_url.find(".xml") == std::string::npos &&
                            absolute_url.find(".rss") == std::string::npos) {
                            
                            int nuevos_saltos = padre.saltos;
                            if (host == padre.dominio_base) {
                                // Antes de encolar, verificar si ya está en la DB para reducir presión en la cola
                                if (!grafo_ || !grafo_->url_ya_visitada(absolute_url)) {
                                    urls_pendientes_.push({absolute_url, padre.dominio_base, extract_path, padre.saltos});
                                }
                            } else {
                                nuevos_saltos++;
                                if (nuevos_saltos <= 3) {
                                    if (!grafo_ || !grafo_->url_ya_visitada(absolute_url)) {
                                        urls_pendientes_.push({absolute_url, host, extract_path, nuevos_saltos});
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (h) curl_url_cleanup(h);
        }

        // Encolar hijos en pila
        std::vector<pugi::xml_node> hijos;
        for (pugi::xml_node h : nodo.children()) hijos.push_back(h);
        for (auto it = hijos.rbegin(); it != hijos.rend(); ++it) pila.push(*it);
    }

    curl_url_cleanup(h_url_base);
}

} // namespace sursur
