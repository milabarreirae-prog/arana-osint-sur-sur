// ============================================================================
// Araña OSINT Sur-Sur — FeedbackServer (Servidor HTTP de Retroalimentación)
// Mini-servidor HTTP embebido (cpp-httplib) que recibe feedback del usuario
// desde el lector RSS vía enlaces [👍 Validar] / [👎 Ruido Hegemónico].
// Actualiza el TrustRank en GraphDB y guarda ejemplos positivos para
// In-Context Learning sin re-entrenamiento del LLM.
// ============================================================================
#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>

namespace sursur {

// Forward declarations
class GraphConnector;

/// Servidor HTTP embebido para recibir feedback del usuario.
class FeedbackServer {
public:
    /// Constructor: recibe referencia a GraphConnector para actualizar TrustRank.
    explicit FeedbackServer(GraphConnector& grafo);
    ~FeedbackServer();

    FeedbackServer(const FeedbackServer&) = delete;
    FeedbackServer& operator=(const FeedbackServer&) = delete;

    /// Iniciar el servidor en un puerto y dirección específicos.
    /// @param bind_address Dirección de escucha (ej. 0.0.0.0 o 127.0.0.1)
    /// @param puerto Puerto de escucha (ej. 1973)
    /// @return Puerto asignado, o 0 si falló
    uint16_t iniciar(const std::string& bind_address, uint16_t puerto);

    /// Detener el servidor.
    void detener();

    /// Obtener el puerto actual.
    uint16_t puerto() const { return puerto_; }

    /// Verificar si el servidor está activo.
    bool activo() const { return activo_.load(); }

private:
    struct Impl; // Pimpl para aislar httplib
    std::unique_ptr<Impl> impl_;

    GraphConnector& grafo_;
    uint16_t puerto_ = 0;
    std::atomic<bool> activo_{false};
    std::jthread hilo_servidor_;
};

} // namespace sursur
