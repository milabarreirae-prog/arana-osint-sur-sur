The user wants me to analyze the conversation history and generate a comprehensive project summary in markdown format. Let me extract the key information from our interaction.

The conversation was about:
1. Setting up context for a Qwen Code session
2. Analyzing the `arana-osint-sur-sur` project directory
3. Generating a QWEN.md file for future interactions

Key facts gathered:
- Project: Araña OSINT Sur-Sur - a geopolitical OSINT radar written in C++20
- Uses Ollama for local LLM inference
- Has modules for spider, evaluator, graph connector, feed generator, feedback server, config manager, media processor
- Build system: CMake
- Target: Linux (RHEL 8+, WSL2)
- Dependencies: libcurl, pugixml, SQLite3, spdlog, nlohmann-json, CLI11, fmt, inih
- External services: Ollama, Nginx
- Configuration lives in sursur_config.json (SSOT)
- Has strict rules about local-only execution, no cloud APIs
- NEWSGATE epistemic firewall for content filtering
- Rich RSS output with CDATA blocks

Recent actions:
- Read README.md, CMakeLists.txt, sursur_config.json, .antigravityrules, deploy script, systemd service file, .gitignore
- Generated QWEN.md file

The task was essentially completed - generating the QWEN.md file.# Project Summary

## Overall Goal
Analyze the `arana-osint-sur-sur` codebase and generate a comprehensive `QWEN.md` file to serve as instructional context for future CLI interactions with the project.

## Key Knowledge

### Project Identity
- **Name:** Araña OSINT Sur-Sur — Geopolitical Radar & Counter-Hegemonic Motor
- **Version:** V2026.3.22.FINAL
- **Language:** C++20
- **Build System:** CMake 3.22+
- **Target OS:** Linux (RHEL 8+, Fedora Remix on WSL2)

### Architecture (8 Modules)
| Module | Purpose |
|---|---|
| Spider (`spider.cpp`) | Async web crawler using libcurl + pugixml; extracts clean text from HTML |
| Evaluator (`evaluator.cpp`) | Local LLM inference via Ollama REST API; strips Markdown, enforces flat JSON schema |
| Graph Connector (`graph_connector.cpp`) | SQLite3 persistence; prevents re-processing URLs |
| Feed Generator (`feed_generator.cpp`) | Builds RSS 2.0 XML with `<![CDATA[...]]>` intelligence cards |
| Main Worker (`main.cpp`) | Daemon entry point; event queue, Hard Drop logic, telemetry |
| Feedback Server (`feedback_server.cpp`) | HTTP dashboard serving real-time status |
| Config Manager (`config_manager.cpp`) | Loads SSOT from `sursur_config.json` |
| Media Processor (`media_processor.cpp`) | Media download/transcoding via ffmpeg + yt-dlp |

### Dependencies
- **Libraries:** libcurl, pugixml, SQLite3, spdlog, nlohmann-json, CLI11, fmt, inih (INIReader)
- **External:** Ollama (model: `qwen2.5-coder:7b`), Nginx (dashboard serving)

### Critical Conventions
- **SSOT Rule:** All config lives in `sursur_config.json` — no hardcoded parameters
- **No-Crash Guarantee:** Every JSON parse and SQLite call must be wrapped in `try-catch`
- **FSM for HTML Parsing:** No `std::regex` or raw pugixml on dirty HTML (segfault risk)
- **UTF-8 Strict XML:** All `feed.xml` output must purge invalid control characters
- **Local-Only Inference:** All LLM calls go to `http://localhost:11434` — no cloud APIs
- **NEWSGATE Firewall:** Master system prompt rejects frivolity (sports, entertainment) and prioritizes geopolitical sovereignty topics

### Build & Deploy Commands
```bash
# Build
cd build && cmake .. && make -j$(nproc)

# Deploy (requires root)
sudo bash deploy_y_test_shutdown.sh

# Monitor
journalctl -u sursur-daemon -f | grep -E 'Spider: Descartada|Spider: Inyectando'
```

### File Boundaries (DO NOT TOUCH)
- `.antigravityrules`, `mcp_config.json`
- `claude-context-mode/`, `venv/`, `.venv/`
- Unrelated global headers or infrastructure files

## Recent Actions

1. **[DONE]** Listed project directory — identified 28 items including 9 source files in `src/`
2. **[DONE]** Read `README.md` — understood full architecture, NEWSGATE epistemic firewall, deployment workflow, and historical resolutions (JSON flattening, dynamic API routing, pre-drop telemetry, Nginx/C++ sync)
3. **[DONE]** Read `CMakeLists.txt` — confirmed C++20 standard, 8-module main target, `test-spider` test target, and all linked dependencies
4. **[DONE]** Read `sursur_config.json` — captured SSOT structure: network settings, seed URLs (contra-hegemonic + hegemonic-contrast), LLM parameters with master system prompt, persistence path, media settings
5. **[DONE]** Read `.antigravityrules` — extracted strict operational rules: no cloud routing, MCP server usage, atomic editing, self-healing build loop, autonomous execution protocol
6. **[DONE]** Read `deploy_y_test_shutdown.sh` — documented 8-step atomic deploy process
7. **[DONE]** Read `systemd/sursur-daemon.service` — captured service unit configuration and environment variables
8. **[DONE]** Read `.gitignore` — identified excluded artifacts (build outputs, databases, telemetry files)
9. **[DONE]** Generated comprehensive `QWEN.md` with project overview, architecture table, build/run instructions, directory structure, configuration guide, development conventions, and system directory mappings

## Current Plan

1. **[DONE]** Analyze project directory structure and identify key files
2. **[DONE]** Read README, CMakeLists, config, rules, deploy script, service file, gitignore
3. **[DONE]** Generate `QWEN.md` file with comprehensive project context
4. **[TODO]** Await user's next task or code modification request

---

*Summary generated for session continuity. All critical architectural decisions, conventions, and operational constraints have been captured for future reference.*

---

## Summary Metadata
**Update time**: 2026-04-06T03:48:34.409Z 
