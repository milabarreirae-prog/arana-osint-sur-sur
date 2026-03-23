#pragma once
#include <string>
#include <mutex>
#include <vector>
#include <atomic>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <stdio.h>

namespace sursur {

struct MonitorState {
    static MonitorState& get() {
        static MonitorState instance;
        return instance;
    }

    std::mutex mtx;
    std::string last_prompt;
    std::string last_json;
    float tps = 0.0f;
    float eval_time_ms = 0.0f;
    
    std::atomic<int> active_threads{0};
    std::atomic<int> bfs_queue_size{0};
    std::atomic<int> crawler_status{0}; // 0=DURMIENDO, 1=EJECUTANDO, 2=ESP_RED
    std::string model_name;

    // Observers para SSE
    std::vector<std::function<void(const std::string&)>> clientes_sse;

    void push_event(const nlohmann::json& j) {
        nlohmann::json event = j;
        event["crawler_status"] = crawler_status.load();
        
        std::lock_guard<std::mutex> lock(mtx);
        if (!model_name.empty()) {
            event["model_name"] = model_name;
        }

        std::string payload = "data: " + event.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\n\n";
        for(auto& cli : clientes_sse) {
            cli(payload);
        }
    }

    int64_t ram_usage_mb() {
#ifdef __linux__
        FILE* f = fopen("/proc/self/statm", "r");
        if(f) {
            long rss = 0;
            if (fscanf(f, "%*s %ld", &rss) == 1) {
                fclose(f);
                return (rss * 4096) / (1024 * 1024);
            }
            fclose(f);
        }
#endif
        return 0;
    }
};

} // namespace sursur
