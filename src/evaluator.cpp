#include "evaluator.hpp"
#include "config_manager.hpp"
#include "monitor.hpp"
#include "graph_connector.hpp"
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;
extern std::atomic<bool> g_apagar;

namespace sursur {

struct Evaluator::ImplLLM {
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string inferir(const std::string& system_prompt, const std::string& user_prompt) {
        const auto cfg = ConfigManager::instancia().llm();
        
        CURL* curl = curl_easy_init();
        if (!curl) return "{}";
        
        std::string readBuffer;

        // 1. Definición del JSON Schema Estricto (Radar Geopolítico)
        json schema = {
            {"type", "object"},
            {"properties", {
                {"aprobado", {{"type", "boolean"}}},
                {"motivo_rechazo", {{"type", "string"}}},
                {"relevancia", {{"type", "string"}, {"enum", {"ALTA", "MEDIA", "BAJA"}}}},
                {"clasificacion", {{"type", "string"}, {"enum", {"HEGEMONICA", "CONTRAHEGEMONICA", "HIBRIDA"}}}},
                {"justificacion_seleccion", {{"type", "string"}}},
                {"autor", {{"type", "string"}}},
                {"nivel_autoridad", {{"type", "string"}, {"enum", {"Academico", "Experto", "Agencia", "Desconocido"}}}},
                {"tema_principal", {{"type", "string"}}},
                {"hashtags", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                {"resumen", {{"type", "string"}}},
                {"texto_completo_curado", {{"type", "string"}}},
                {"falacias_detectadas", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                {"cita_apa7", {{"type", "string"}}}
            }},
            {"required", {"aprobado", "motivo_rechazo", "relevancia", "clasificacion", "justificacion_seleccion", "autor", "nivel_autoridad", "tema_principal", "hashtags", "resumen", "texto_completo_curado", "falacias_detectadas", "cita_apa7"}}
        };

        json payload = {
            {"model", cfg.model_name},
            {"messages", {
                {{"role", "system"}, {"content", system_prompt}},
                {{"role", "user"}, {"content", user_prompt}}
            }},
            {"format", schema},
            {"stream", false},
            {"options", {
                {"temperature", cfg.temperatura},
                {"num_ctx", cfg.ventana_contexto},
                {"num_predict", 4096}, // Asegurar espacio para el JSON largo
                {"num_gpu", 99}         // Forzar delegación total a VRAM (NVIDIA RTX 3080)
            }}
        };
        
        std::string payload_str = payload.dump();
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        std::string url_api = cfg.endpoint;
        if (url_api.find("/api/generate") != std::string::npos) {
            url_api.replace(url_api.find("/api/generate"), 13, "/api/chat");
        }
        curl_easy_setopt(curl, CURLOPT_URL, url_api.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        
        CURLcode res = curl_easy_perform(curl);
        if (readBuffer.find("\"error\"") != std::string::npos) {
            spdlog::error("Evaluador: Ollama rechazó la petición. Respuesta cruda: {}", readBuffer);
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            spdlog::warn("Evaluador: fallo de red curl ({}) para Ollama en {}.", curl_easy_strerror(res), cfg.endpoint);
            return "{}";
        }
        
        std::string json_limpio = readBuffer;
        size_t pos_start = json_limpio.find("```json");
        if (pos_start != std::string::npos) {
            json_limpio.erase(pos_start, 7);
        } else {
            pos_start = json_limpio.find("```");
            if (pos_start != std::string::npos) json_limpio.erase(pos_start, 3);
        }
        size_t pos_end = json_limpio.rfind("```");
        if (pos_end != std::string::npos) {
            json_limpio.erase(pos_end, 3);
        }

        try {
            // Usar versión robusta de parseo que no lanza excepciones por UTF-8 malformado
            auto j = json::parse(json_limpio, nullptr, false, true);
            if (j.is_discarded()) {
                spdlog::error("Evaluador: Ollama devolvió un JSON descartable (malformado o truncado).");
                return "{}";
            }
            return j.value("message", json::object()).value("content", "{}");
        } catch (const std::exception& e) { 
            spdlog::error("Evaluador: Excepción en post-procesamiento: {}", e.what());
            return "{}"; 
        }
    }
};

Evaluator::Evaluator() : impl_llm_(std::make_unique<ImplLLM>()), modelo_cargado_(true) {}
Evaluator::~Evaluator() {}

bool Evaluator::inicializar_modelo() { return true; }
bool Evaluator::modelo_listo() const { return true; }
std::string Evaluator::obtener_nombre_modelo() const { 
    return ConfigManager::instancia().llm().model_name; 
}

std::string Evaluator::evaluar_documento(const std::string &url, const std::string &contenido, const std::string &system_prompt) {
    // Prompt de usuario para el artículo específico
    std::string user_prompt = 
        "URL del Artículo: " + url + "\n" +
        "Contenido a evaluar:\n" + contenido.substr(0, 5000);

    std::string respuesta_raw = impl_llm_->inferir(system_prompt, user_prompt);

    // ── Parseo y Sanitización de la respuesta del Radar Geopolítico ───────────
    try {
        auto j = json::parse(respuesta_raw);
        
        // Fallbacks planos obligatorios para garantizar compatibilidad con el esquema
        if (!j.contains("aprobado") || !j["aprobado"].is_boolean()) j["aprobado"] = false;
        if (!j.contains("motivo_rechazo")) j["motivo_rechazo"] = "Sin motivo especificado";
        if (!j.contains("relevancia")) j["relevancia"] = "BAJA";
        if (!j.contains("clasificacion")) j["clasificacion"] = "DESCONOCIDA";
        if (!j.contains("justificacion_seleccion")) j["justificacion_seleccion"] = "Análisis estructural incompleto (Ollama retornó vacío o falló)";
        if (!j.contains("autor")) j["autor"] = "Desconocido";
        if (!j.contains("nivel_autoridad")) j["nivel_autoridad"] = "Desconocido";
        if (!j.contains("tema_principal")) j["tema_principal"] = "Desconocido";
        if (!j.contains("resumen")) j["resumen"] = "Sin resumen disponible";
        if (!j.contains("texto_completo_curado")) j["texto_completo_curado"] = contenido.substr(0, 1000);
        if (!j.contains("cita_apa7")) j["cita_apa7"] = "Referencia no generada";
        if (!j.contains("hashtags")) j["hashtags"] = json::array();
        if (!j.contains("falacias_detectadas")) j["falacias_detectadas"] = json::array();
        
        return j.dump();
    } catch (const std::exception& e) {
        spdlog::warn("Evaluador: Error crítico de parseo JSON para {}: {}.", url, e.what());
        return R"({"aprobado":false,"relevancia":"BAJA","clasificacion":"ERROR","justificacion_seleccion":"Excepción de parseo interno","autor":"Sistema"})";
    }
}

std::vector<float> Evaluator::generar_embedding(const std::string& texto) { return {}; }
std::vector<std::string> Evaluator::segmentar_texto(const std::string& texto, size_t max_chars) { return {texto}; }
std::vector<FragmentoRAG> Evaluator::busqueda_rag(const std::string &consulta, int32_t top_k, const std::string &idioma_preferido) { return {}; }
ResultadoTraduccion Evaluator::traducir(const std::string &texto, const std::string &idioma_origen, const std::string &idioma_destino) { return {texto, 1.0f}; }
bool Evaluator::vectorizar_pdf(const std::filesystem::path &ruta_pdf) { return true; }
int32_t Evaluator::indexar_corpus() { return 0; }

std::string CitaAPA7::formatear() const {
    std::ostringstream oss;
    if (autores.empty()) oss << "S/A. ";
    else {
        for (size_t i = 0; i < autores.size(); ++i) {
            oss << autores[i];
            if (i < autores.size() - 1) oss << ", ";
        }
        oss << ". ";
    }
    if (anio > 0) oss << "(" << anio << "). ";
    else oss << "(s.f.). ";
    if (!titulo.empty()) oss << titulo << ". ";
    if (!fuente.empty()) oss << fuente << ". ";
    if (!doi.empty()) oss << "https://doi.org/" << doi;
    else if (!url.empty()) oss << "Recuperado de " << url;
    return oss.str();
}

CitaAPA7 Evaluator::generar_cita_apa7(const std::string &titulo, const std::vector<std::string> &autores, int32_t anio, const std::string &fuente, const std::string &url, const std::string &doi) {
    CitaAPA7 cita;
    cita.autores = autores; cita.anio = anio; cita.titulo = titulo; cita.fuente = fuente; cita.url = url; cita.doi = doi;
    auto ahora = std::chrono::system_clock::now();
    auto tiempo = std::chrono::system_clock::to_time_t(ahora);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&tiempo), "%Y-%m-%dT%H:%M:%SZ");
    cita.fecha_acceso = oss.str();
    return cita;
}

std::optional<std::string> Evaluator::verificar_orcid_(const std::string &nombre) { return std::nullopt; }
std::optional<std::string> Evaluator::verificar_google_scholar_(const std::string &nombre) { return std::nullopt; }

} // namespace sursur
