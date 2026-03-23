#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <stop_token>
#include <unordered_set>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>

namespace sursur {

class GraphConnector;
class FeedGenerator;  // Forward declaration — evita dependencia circular

struct LinkItem {
    std::string url;
    std::string dominio_base;
    std::string ruta_padre;
    int saltos;
};

struct ResultadoRastreo {
    bool exitoso;
    std::string url;
    std::string contenido_texto;
    std::string html_crudo;
    std::string imagen_principal;
    std::string idioma_detectado;
    int profundidad;
};

struct EstadoSesionRastreo {
    enum class Estado { EN_COLA, RASTREANDO, COMPLETADO, ERROR };
    std::string id_rastreo;
    uint32_t urls_descubiertas;
    uint32_t urls_procesadas;
    uint32_t profundidad_actual;
    Estado estado;
};

/**
 * @brief Cola concurrente segura para hilos (Thread-Safe)
 */
template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;

    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

struct NoticiaExtraida {
    std::string url;
    std::string titulo;
    std::string subtitulo;
    std::string autor;
    std::string fecha_creacion;
    std::string fecha_modificacion;
    std::string imagen_principal;
    std::vector<std::string> personas_citadas;
    std::vector<std::string> personas_entrevistadas;
    std::string cuerpo_plano;
    std::vector<std::string> tags_dinamicos;
};

/**
 * @brief Motor de rastreo Araña OSINT
 */
class Spider {
public:
    Spider() = default;
    ~Spider() = default;

    // Métodos públicos para gestión del ciclo de vida
    void iniciar_bucle(size_t num_hilos = 4);
    void detener_todos();
    void cargar_semillas(const std::string& ruta);
    
    // Hooks orientados a gRPC y LLM
    std::string iniciar_rastreo(const std::string& url_rss, int max_saltos);
    EstadoSesionRastreo obtener_estado(const std::string& id_rastreo);
    
    using CallbackT = std::function<void(const ResultadoRastreo&)>;
    void set_grafo(std::shared_ptr<GraphConnector> grafo) { grafo_ = grafo; }
    void set_feed_gen(std::shared_ptr<FeedGenerator> feed_gen) { feed_gen_ = feed_gen; }
    void registrar_callback(CallbackT cb) { callback_ = cb; }

    std::string descargar_url(const std::string& url, std::stop_token stoken = std::stop_token{});
    std::string extraer_texto(const std::string& html);
    std::optional<NoticiaExtraida> analizar_noticia(const std::string& html_crudo, const std::string& url);
    std::string obtener_resultado_procesado();

    // Métricas
    uint64_t obtener_urls_procesadas() const { return urls_procesadas_.load(); }
    uint64_t obtener_descargas_exitosas() const { return descargas_exitosas_.load(); }
    uint64_t obtener_bytes_descargados() const { return bytes_descargados_.load(); }

    static std::string obtener_host(const std::string& url);

private:
    bool marcar_como_visitada(const std::string& url);
    void extraer_y_encolar_enlaces(const std::string& html_crudo, const LinkItem& padre);
    static std::string obtener_path(const std::string& url);

    // Integraciones OSINT y AI
    std::shared_ptr<GraphConnector> grafo_;
    std::shared_ptr<FeedGenerator>  feed_gen_;
    CallbackT callback_;

    // Miembros privados
    std::vector<std::jthread> hilos_trabajadores_;
    std::stop_source parada_solicitada_;
    
    ThreadSafeQueue<LinkItem> urls_pendientes_;
    ThreadSafeQueue<std::string> resultados_procesados_;

    std::unordered_set<std::string> urls_visitadas_;
    std::mutex mutex_visitadas_;

    std::atomic<uint64_t> urls_procesadas_{0};
    std::atomic<uint64_t> descargas_exitosas_{0};
    std::atomic<uint64_t> bytes_descargados_{0};
};

} // namespace sursur
