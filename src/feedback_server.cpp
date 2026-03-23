// Servidor HTTP embebido con cpp-httplib.
// Endpoints:
// - GET /feedback?id=X&score=Y
// - GET / (Static file server para RSS y media)
// Actualiza TrustRank en GraphDB y guarda ejemplos positivos.
// ============================================================================
#include "feedback_server.hpp"
#include "graph_connector.hpp"
#include "config_manager.hpp"
#include "monitor.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sstream>
#include <httplib.h>
#include <random>
#include <queue>

using json = nlohmann::json;

namespace sursur {

// ── Pimpl para aislar httplib ──────────────────────────────────────────────
struct FeedbackServer::Impl {
    httplib::Server servidor;
};

// ── Constructor / Destructor ──────────────────────────────────────────────
FeedbackServer::FeedbackServer(GraphConnector& grafo)
    : impl_(std::make_unique<Impl>()), grafo_(grafo) {
}

FeedbackServer::~FeedbackServer() {
    detener();
}

// ── Iniciar servidor ──────────────────────────────────────────────────────
uint16_t FeedbackServer::iniciar(const std::string& bind_address, uint16_t puerto) {
    const auto& config_media = ConfigManager::instancia().media();
    std::string output_dir = config_media.output_dir.string();

    spdlog::info("FeedbackServer: montando directorio estático en '{}'", output_dir);
    if (!impl_->servidor.set_mount_point("/media", output_dir)) {
        spdlog::error("FeedbackServer: no se pudo montar el directorio estático");
    }

    // Endpoint raíz: Redirigir al Monitor
    impl_->servidor.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/monitor");
    });

    // Feed RSS
    impl_->servidor.Get("/feed.xml", [output_dir](const httplib::Request&, httplib::Response& res) {
        std::filesystem::path ruta = std::filesystem::path(output_dir) / "feed.xml";
        if (std::filesystem::exists(ruta)) {
            std::ifstream file(ruta);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                res.set_content(buffer.str(), "application/rss+xml; charset=utf-8");
                return;
            }
        }
        res.status = 404;
        res.set_content("Feed RSS no encontrado", "text/plain");
    });

    // Feedback
    impl_->servidor.Get("/feedback", [this](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.get_param_value("id");
        std::string score_str = req.get_param_value("score");
        if (id.empty() || score_str.empty()) {
            res.status = 400;
            res.set_content("Parámetros faltantes", "text/plain");
            return;
        }
        int score = 0;
        try { score = std::stoi(score_str); } catch (...) { res.status = 400; return; }
        
        grafo_.actualizar_trust_rank(id, score);
        if (score > 0) grafo_.guardar_ejemplo_positivo(id, "");

        std::string emoji = score > 0 ? "👍" : "👎";
        std::string msg = score > 0 ? "Criterio validado." : "Marcado como ruido.";
        res.set_content("<html><body style='background:#1a1a2e;color:#eee;display:flex;justify-content:center;align-items:center;height:100vh;font-family:sans-serif;'>"
                        "<div style='background:#16213e;padding:2rem;border-radius:1rem;text-align:center;'><h1>" + emoji + "</h1><h2>Feedback Registrado</h2><p>" + msg + "</p></div></body></html>", "text/html");
    });

    // Health
    impl_->servidor.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // Dashboard Monitor
    impl_->servidor.Get("/monitor", [](const httplib::Request&, httplib::Response& res) {
        std::string html = R"monitor_html(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sur-Sur | Centro de Mando</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg: #09090b; --card: #18181b; --border: #27272a;
            --text: #fafafa; --dim: #a1a1aa;
            --primary: #10b981; --accent: #6366f1; --err: #f43f5e;
            --accent-glow: rgba(99, 102, 241, 0.15);
        }
        * { box-sizing: border-box; transition: all 0.2s ease; }
        body { 
            background: var(--bg); color: var(--text); 
            font-family: 'Outfit', sans-serif; margin: 0; 
            overflow-x: hidden; display: flex; flex-direction: column; height: 100vh;
        }
        header { 
            padding: 1rem 2rem; background: rgba(24, 24, 27, 0.8); 
            backdrop-filter: blur(12px); border-bottom: 1px solid var(--border);
            display: flex; justify-content: space-between; align-items: center;
            position: sticky; top: 0; z-index: 100;
        }
        .logo { font-weight: 800; font-size: 1.25rem; letter-spacing: -0.02em; display: flex; align-items: center; gap: 10px; }
        .logo span { color: var(--primary); }
        .status-badge { 
            background: var(--accent-glow); color: var(--accent); 
            padding: 4px 12px; border-radius: 99px; font-size: 0.75rem; 
            font-weight: 600; border: 1px solid var(--accent);
            animation: pulse 2s infinite;
        }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.6; } 100% { opacity: 1; } }
        main { flex: 1; display: grid; grid-template-columns: 350px 1fr 400px; gap: 1rem; padding: 1rem; overflow: hidden; }
        .panel { 
            background: var(--card); border: 1px solid var(--border); 
            border-radius: 16px; display: flex; flex-direction: column; overflow: hidden;
            box-shadow: 0 4px 24px rgba(0,0,0,0.4);
        }
        .panel-header { 
            padding: 12px 16px; border-bottom: 1px solid var(--border); 
            display: flex; justify-content: space-between; align-items: center;
            background: rgba(255,255,255,0.02);
        }
        .panel-header h3 { margin: 0; font-size: 0.85rem; text-transform: uppercase; letter-spacing: 0.1em; color: var(--dim); }
        .stats-grid { grid-column: 1 / -1; display: grid; grid-template-columns: repeat(5, 1fr); gap: 1rem; margin-bottom: 1rem; }
        .stat-card { background: var(--card); border: 1px solid var(--border); padding: 16px; border-radius: 16px; text-align: center; }
        .stat-val { font-size: 1.5rem; font-weight: 800; display: block; }
        .stat-label { font-size: 0.7rem; color: var(--dim); text-transform: uppercase; }
        #net-log { list-style: none; padding: 0; margin: 0; overflow-y: auto; flex: 1; font-family: 'JetBrains Mono', monospace; font-size: 11px; }
        #net-log li { padding: 8px 16px; border-bottom: 1px solid var(--border); color: var(--dim); display: flex; gap: 8px; }
        .ts { color: #555; }
        .ok { color: var(--primary); }
        .wait { color: var(--accent); }
        .llm-content { padding: 16px; font-family: 'JetBrains Mono', monospace; font-size: 12px; height: 100%; display: flex; flex-direction: column; gap: 1rem; }
        textarea { 
            background: #000; color: #a5f3fc; border: 1px solid var(--border); 
            border-radius: 8px; padding: 12px; width: 100%; height: 200px; resize: none;
            overflow-y: auto; scrollbar-width: thin;
        }
        .json-out { color: #f9a8d4 !important; flex: 1; }
        .metric-item { padding: 12px 16px; display: flex; justify-content: space-between; border-bottom: 1px solid var(--border); }
        .metric-item:last-child { border: 0; }
    </style>
</head>
<body>
    <header>
        <div class="logo">🕸️ ARAÑA <span>SUR-SUR</span></div>
        <div style="display: flex; gap: 20px; align-items: center;">
            <div class="status-badge" id="bot-status">SISTEMA ACTIVO</div>
            <div id="model-badge" style="color: var(--dim); font-size: 0.8rem; font-family: 'JetBrains Mono';">LLM: Cargando...</div>
        </div>
    </header>
    <div style="padding: 1rem 1rem 0 1rem;">
        <div class="stats-grid">
            <div class="stat-card"><span class="stat-val" id="count-total">0</span><span class="stat-label">Analizados</span></div>
            <div class="stat-card"><span class="stat-val" style="color:var(--primary)" id="count-ok">0</span><span class="stat-label">Aprobados</span></div>
            <div class="stat-card"><span class="stat-val" style="color:var(--err)" id="count-fail">0</span><span class="stat-label">Rechazados</span></div>
            <div class="stat-card"><span class="stat-val" style="color:var(--accent)" id="q-size">0</span><span class="stat-label">En Cola</span></div>
            <div class="stat-card"><span class="stat-val" id="tps-val">0.0</span><span class="stat-label">Tokens/s</span></div>
        </div>
    </div>
    <main>
        <div class="panel">
            <div class="panel-header"><h3>Actividad del Spider</h3></div>
            <ul id="net-log"></ul>
        </div>
        <div class="panel">
            <div class="panel-header"><h3>Geopolitical Radar Context</h3><span id="eval-ms" style="font-size:10px; color:var(--dim)">0ms</span></div>
            <div class="llm-content">
                <div><b>Cuerpo Analizado:</b><textarea id="prompt" readonly></textarea></div>
                <div><b>Salida Radar (JSON):</b><textarea id="json" class="json-out" readonly></textarea></div>
            </div>
        </div>
        <div class="panel">
            <div class="panel-header"><h3>Métricas de Host</h3></div>
            <div class="metric-item"><span>Threads Activos</span><b id="threads">0</b></div>
            <div class="metric-item"><span>Consumo RAM</span><b id="ram">0 MB</b></div>
            <div class="metric-item"><span>Estado Spider</span><b id="spider-state">REPOSO</b></div>
            <div style="padding:16px; flex:1; display:flex; flex-direction:column; justify-content:flex-end;">
                 <div style="background:rgba(255,255,255,0.05); padding:12px; border-radius:8px; font-size:11px; color:#777;">
                    Araña OSINT Sur-Sur v0.1.0<br>
                    Engine: C++20 / SQLite3 / Ollama
                 </div>
            </div>
        </div>
    </main>
    <script>
        const es = new EventSource('/api/stream');
        const netLog = document.getElementById('net-log');
        let total = 0, ok = 0, fail = 0;
        es.onmessage = e => {
            if(e.data.startsWith(':')) return;
            try {
                const data = JSON.parse(e.data);
                if(data.model_name) document.getElementById('model-badge').innerText = `MODEL: ${data.model_name}`;
                if(data.type === 'host') {
                    document.getElementById('ram').innerText = data.ram_mb + ' MB';
                    document.getElementById('threads').innerText = data.threads;
                    document.getElementById('q-size').innerText = data.bfs_queue;
                    const states = ["REPOSO", "RASTRREANDO", "ESP. RED"];
                    document.getElementById('spider-state').innerText = states[data.crawler_status] || "UNK";
                } else if(data.type === 'net') {
                    total++; document.getElementById('count-total').innerText = total;
                    const li = document.createElement('li');
                    const isOk = data.status === 200;
                    li.innerHTML = `<span class="ts">${new Date().toLocaleTimeString()}</span> <span class="${isOk?'ok':'wait'}">[${data.status}]</span> ${data.url}`;
                    netLog.prepend(li); if(netLog.children.length > 50) netLog.lastChild.remove();
                } else if(data.type === 'llm') {
                    document.getElementById('tps-val').innerText = data.tps;
                    document.getElementById('eval-ms').innerText = data.eval_ms + 'ms';
                    document.getElementById('prompt').value = data.prompt;
                    document.getElementById('json').value = data.json;
                    try {
                        const out = JSON.parse(data.json);
                        if(out.aprobado) ok++; else fail++;
                        document.getElementById('count-ok').innerText = ok;
                        document.getElementById('count-fail').innerText = fail;
                    } catch(e){}
                }
            } catch(e) {}
        };
        setInterval(() => fetch('/api/metrics_poll'), 2000);
    </script>
</body>
</html>)monitor_html";
        res.set_content(html, "text/html");
    });

    // Metrics Poll
    impl_->servidor.Get("/api/metrics_poll", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json ev;
        ev["type"] = "host";
        ev["ram_mb"] = MonitorState::get().ram_usage_mb();
        ev["threads"] = MonitorState::get().active_threads.load();
        ev["bfs_queue"] = MonitorState::get().bfs_queue_size.load();
        MonitorState::get().push_event(ev);
        res.set_content("ok", "text/plain");
    });

    // SSE Stream
    impl_->servidor.Get("/api/stream", [&](const httplib::Request&, httplib::Response& res) {
        res.set_chunked_content_provider("text/event-stream", [](size_t, httplib::DataSink &sink) {
            auto q = std::make_shared<std::queue<std::string>>();
            auto q_mtx = std::make_shared<std::mutex>();
            auto cb = [q, q_mtx](const std::string& payload) {
                std::lock_guard<std::mutex> lock(*q_mtx);
                q->push(payload);
            };
            {
                std::lock_guard<std::mutex> lock(MonitorState::get().mtx);
                MonitorState::get().clientes_sse.push_back(cb);
            }
            while(true) {
                std::string ev;
                {
                    std::lock_guard<std::mutex> lock(*q_mtx);
                    if (!q->empty()) { ev = q->front(); q->pop(); }
                }
                if (!ev.empty()) { if (!sink.write(ev.c_str(), ev.size())) return false; }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!sink.write(": keepalive\n\n", 13)) return false;
            }
            return true;
        });
    });

    puerto_ = puerto;
    activo_.store(true);
    spdlog::info("FeedbackServer: iniciando en http://{}:{}...", bind_address, puerto);

    hilo_servidor_ = std::jthread([this, bind_address, puerto](std::stop_token) {
        if (!impl_->servidor.listen(bind_address, puerto)) {
            spdlog::error("FeedbackServer: falló escucha en {}:{}", bind_address, puerto);
            activo_.store(false);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return activo_.load() ? puerto_ : 0;
}

// ── Detener servidor ──────────────────────────────────────────────────────
void FeedbackServer::detener() {
    if (activo_.load()) {
        spdlog::info("FeedbackServer: deteniendo servidor en puerto {}", puerto_);
        impl_->servidor.stop();
        activo_.store(false);
    }
}

} // namespace sursur
