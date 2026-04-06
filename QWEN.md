# QWEN.md — Araña OSINT Sur-Sur

## Project Overview

**Araña OSINT Sur-Sur** is an asynchronous geopolitical intelligence radar and counter-hegemonic motor, written in **C++20**. The system operates as a daemon (`sur-sur-daemon`) that crawls, extracts, and curates OSINT (Open-Source Intelligence) from RSS feeds in real time. It leverages local LLM inference via **Ollama** and an epistemic firewall called **NEWSGATE** to filter information and produce a Rich RSS feed focused on Global South perspectives.

### Core Architecture

The system is composed of loosely coupled modules orchestrated via an event-driven, thread-safe queue architecture:

| Module | File | Responsibility |
|---|---|---|
| **Spider** | `src/spider.cpp` | Async web crawler using libcurl + pugixml. Extracts clean text from HTML by purging scripts/styles. |
| **Evaluator** | `src/evaluator.cpp` | LLM inference REST interface. Connects to Ollama (`/api/chat`), injects a flat JSON schema, and strips Markdown artifacts from responses. |
| **Graph Connector** | `src/graph_connector.cpp` | SQLite3 persistence layer. Acts as historical memory to avoid re-processing already analyzed URLs. |
| **Feed Generator** | `src/feed_generator.cpp` | Builds standard RSS 2.0 XML output with `<![CDATA[...]]>` blocks containing inline HTML/CSS "Intelligence Cards". |
| **Main Worker** | `src/main.cpp` | Daemon heart. Enqueues events, performs *Hard Drop* (in-memory elimination of irrelevant news), populates telemetry, and manages service lifecycle via systemd. |
| **Feedback Server** | `src/feedback_server.cpp` | Real-time dashboard serving memory state (`status.json`, `ollama_log.json`) for observability. |
| **Config Manager** | `src/config_manager.cpp` | Reads and manages the Single Source of Truth (SSOT) from `/etc/sursur/sursur_config.json`. |
| **Media Processor** | `src/media_processor.cpp` | Handles media (audio/video) download and transcoding via ffmpeg and yt-dlp. |

### Key Design Principles

- **NEWSGATE Epistemic Firewall**: A master system prompt filters content based on geopolitical relevance, automatically rejecting frivolity (sports, entertainment, viral content) and prioritizing topics like asymmetric conflicts, technological sovereignty, and de-dollarization.
- **Hard Drop**: If the LLM labels a news item as LOW relevance or fails structured output, the process kills the data in-memory before it reaches disk.
- **Local-Only Inference**: All LLM calls go to a local Ollama instance (`http://localhost:11434`). No cloud API usage.
- **Rich RSS Output**: The feed.xml output contains enriched intelligence cards with metadata, summaries, APA7 citations, and hashtags.

## Technologies & Dependencies

- **Language**: C++20
- **Build System**: CMake 3.22+
- **Key Libraries**: libcurl, pugixml, SQLite3, spdlog, nlohmann-json, CLI11, fmt, INIReader (inih)
- **External Services**: Ollama (model: `qwen2.5-coder:7b` or higher), Nginx (for serving the UI dashboard)
- **Target OS**: Linux (RHEL 8+, Fedora Remix on WSL2)

## Building and Running

### Prerequisites (on target Linux system)

```bash
# Development tools
sudo dnf install -y cmake make g++ pkg-config

# Dependencies
sudo dnf install -y libcurl-devel sqlite-devel pugixml-devel spdlog-devel nlohmann-json-devel CLI11-devel fmt-devel inih-devel
```

### Build

```bash
# From the project root
cd build
cmake ..
make -j$(nproc)
```

This produces two targets:
- **`sur-sur-daemon`** — the main production binary
- **`test-spider`** — a standalone test harness for the spider module

### Deploy & Run

Deployment requires root and is handled by the automated script:

```bash
sudo bash deploy_y_test_shutdown.sh
```

This script:
1. Stops the existing `sursur-daemon` systemd service
2. Compiles the C++ binary with multi-threaded make
3. Copies the binary to `/usr/sbin/sur-sur-daemon`
4. Deploys `sursur_config.json` to `/etc/sursur/`
5. Creates required directories (`/var/lib/sursur/{media,db,ui}`)
6. Resets telemetry and feed files
7. Deploys the UI dashboard to `/var/lib/sursur/ui/`
8. Restarts the systemd service

### Manual Run (for development/debugging)

```bash
sudo /usr/sbin/sur-sur-daemon --config=/etc/sursur/sursur_config.json
```

### Monitoring

```bash
# Follow service logs
journalctl -u sursur-daemon -f

# Filter for acceptance/rejection decisions
journalctl -u sursur-daemon -f | grep -E 'Spider: Descartada|Spider: Inyectando'
```

## Project Structure

```
arana-osint-sur-sur/
├── src/                     # C++ source files
│   ├── main.cpp             # Daemon entry point & worker orchestration
│   ├── spider.cpp           # Web crawler (libcurl + pugixml)
│   ├── evaluator.cpp        # Ollama LLM inference client
│   ├── config_manager.cpp   # JSON config loader
│   ├── feed_generator.cpp   # RSS 2.0 XML builder
│   ├── feedback_server.cpp  # HTTP dashboard server
│   ├── graph_connector.cpp  # SQLite3 persistence
│   ├── media_processor.cpp  # Media download/transcoding
│   └── test_spider.cpp      # Spider test harness
├── include/                 # C++ headers
├── conf/                    # Additional configuration files
├── config/                  # Configuration templates
├── ui/                      # Dashboard frontend (HTML/JS/CSS)
├── proto/                   # Protocol buffer definitions (if any)
├── docs/                    # Documentation
├── frontend/                # Frontend assets
├── systemd/                 # systemd service unit files
├── build/                   # CMake out-of-tree build directory
├── CMakeLists.txt           # Main build configuration
├── deploy_y_test_shutdown.sh # Automated deploy script
├── sursur_config.json       # SSOT configuration (seeds, LLM params, prompts)
└── INIReader.h              # Vendored INI parsing header
```

## Configuration

All runtime configuration lives in **`sursur_config.json`** (deployed to `/etc/sursur/sursur_config.json`). Key sections:

- **`network`**: Timeout, user agent, strict filtering toggle
- **`sources`**: RSS seed URLs (contra-hegemonic and hegemonic-contrast), target keywords
- **`llm`**: Ollama model name, endpoint, temperature, context window, and the master system prompt
- **`persistence`**: SQLite database path
- **`media`**: Output directory, ffmpeg/yt-dlp paths, file size/resolution limits

## Development Conventions

### Coding Standards

- **C++20** is the minimum standard. Use modern C++ idioms (smart pointers, ranges, concepts where applicable).
- **Thread safety**: All shared data structures must use thread-safe queues and proper synchronization.
- **Error handling**: Every JSON parse call and SQLite operation **must** be wrapped in `try-catch` blocks. The daemon must **never** crash with SIGABRT.
- **Logging**: Use `spdlog` for all logging (`spdlog::info`, `spdlog::warn`, `spdlog::error`). Logs are consumed via `journalctl`.
- **No hardcoded parameters**: All configuration must come from `sursur_config.json`.

### DOM Parsing

- **Do not** use `std::regex` or pugixml for raw HTML text extraction (segfault risk, queue starvation).
- Use **Finite State Machines (FSM)** for single-pass text extraction.

### XML Output

- All content written to `feed.xml` must be UTF-8 sanitized (invalid control characters removed).
- Rich content (images, metadata, quotes) must be wrapped in `<![CDATA[...]]>` blocks.

### Git & File Boundaries

- **DO NOT MODIFY**: `.antigravityrules`, `mcp_config.json`, `claude-context-mode/`, `venv/`, or global headers unrelated to the current task.
- **Atomic edits**: Use targeted string replacements instead of rewriting entire files unless structural corruption demands it.
- **Self-healing**: If a build fails, analyze the error, apply fixes, and retry up to 3 times before asking for help.

## System Directories (Post-Deploy)

| Path | Purpose |
|---|---|
| `/usr/sbin/sur-sur-daemon` | Main executable |
| `/etc/sursur/sursur_config.json` | Configuration SSOT |
| `/var/lib/sursur/db/graph.db` | SQLite historical database |
| `/var/lib/sursur/ui/` | Nginx document root (dashboard, feed.xml, telemetry) |
| `/var/lib/sursur/media/` | Downloaded/transcoded media files |

## Version

Current version tag: **V2026.3.22.FINAL**
