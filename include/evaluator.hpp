// ============================================================================
// Araña OSINT Sur-Sur — Evaluator (Evaluador Semántico + RAG)
// Integración con llama.cpp para inferencia LLM local.
// Soporte multilingüe amplio: lenguas occidentales, dialectos árabes,
// lenguas originarias andinas/sudamericanas.
// ============================================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace sursur {

/// Resultado de evaluación semántica de un documento.
struct ResultadoEvaluacion {
  float puntuacion_rigor = 0.0f; // 0.0 — 1.0
  std::string resumen;
  std::vector<std::string> conceptos_clave;
  std::string idioma_detectado;

  // Campos JSON dinámicos
  bool aprobado = false;
  std::string rechazo_motivo;
  std::string justificacion_teorica;
  std::vector<std::string> axiomas_detectados;

  struct EntidadAcademica {
    std::string nombre;
    std::string grado_academico; // "Magíster", "Doctor", etc.
    std::string orcid;
    std::string afiliacion;
  };
  std::vector<EntidadAcademica> entidades;
};

/// Fragmento RAG recuperado del corpus local.
struct FragmentoRAG {
  std::string contenido;
  std::string fuente_pdf;
  float similitud = 0.0f;
  int32_t pagina = 0;
};

/// Resultado de traducción.
struct ResultadoTraduccion {
  std::string texto_traducido;
  float confianza = 0.0f;
};

/// Cita en formato APA 7.
struct CitaAPA7 {
  std::vector<std::string> autores; // "Apellido, I."
  int32_t anio = 0;
  std::string titulo;
  std::string fuente; // Nombre del medio/revista
  std::string url;
  std::string doi;
  std::string fecha_acceso; // ISO 8601

  /// Generar la cadena formateada APA 7 completa.
  std::string formatear() const;
};

// ============================================================================
// Evaluador Semántico con RAG Bidireccional
// ============================================================================
class Evaluator {
public:
  Evaluator();
  ~Evaluator();

  /// Prohibir copia.
  Evaluator(const Evaluator &) = delete;
  Evaluator &operator=(const Evaluator &) = delete;

  /// Inicializar el modelo LLM desde la ruta configurada.
  /// @return true si la carga del modelo fue exitosa.
  bool inicializar_modelo();

  /// Evaluar el rigor analítico de un documento.
  /// @param url_fuente URL de origen del documento
  /// @param texto Contenido textual del documento
  /// @param system_prompt Prompt de sistema para el LLM
  /// @return Resultado de la evaluación semántica (JSON string)
  std::string evaluar_documento(const std::string &url_fuente,
                               const std::string &texto,
                               const std::string &system_prompt);

  std::vector<float> generar_embedding(const std::string& texto);
  std::vector<std::string> segmentar_texto(const std::string& texto, size_t max_chars = 1500);


  /// Búsqueda RAG contra el corpus local de PDFs.
  /// @param consulta Texto de la consulta
  /// @param top_k Número de fragmentos a retornar
  /// @param idioma_preferido Idioma preferido para los resultados
  /// @return Vector de fragmentos RAG ordenados por similitud
  std::vector<FragmentoRAG>
  busqueda_rag(const std::string &consulta, int32_t top_k = 5,
               const std::string &idioma_preferido = "es");

  /// Traducir texto de un idioma a otro usando el LLM.
  /// @param texto Texto a traducir
  /// @param idioma_origen Código ISO 639 del idioma de origen
  /// @param idioma_destino Código ISO 639 del idioma de destino (defecto: "es")
  /// @return Resultado de la traducción
  ResultadoTraduccion traducir(const std::string &texto,
                               const std::string &idioma_origen,
                               const std::string &idioma_destino = "es");

  /// Vectorizar un PDF del corpus local.
  /// @param ruta_pdf Ruta al archivo PDF
  /// @return true si la vectorización fue exitosa
  bool vectorizar_pdf(const std::filesystem::path &ruta_pdf);

  /// Indexar todo el directorio de corpus PDF.
  /// @return Número de PDFs indexados exitosamente
  int32_t indexar_corpus();

  /// Generar cita APA 7 a partir de metadatos.
  CitaAPA7 generar_cita_apa7(const std::string &titulo,
                             const std::vector<std::string> &autores,
                             int32_t anio, const std::string &fuente,
                             const std::string &url = "",
                             const std::string &doi = "");

  /// Verificar si el modelo está cargado y listo.
  bool modelo_listo() const;

  /// Obtener el nombre del modelo cargado.
  std::string obtener_nombre_modelo() const;

  /// Conectar referencia al GraphConnector para In-Context Learning.
  /// Debe llamarse después de la construcción.
  void conectar_grafo_icl(class GraphConnector* grafo) { grafo_icl_ = grafo; }

private:

  // ── Inferencia LLM ─────────────────────────────────────────────────────
  std::string inferir_(const std::string &prompt_sistema,
                       const std::string &prompt_usuario);

  // Construir prompt del sistema multilingüe según el idioma de entrada
  std::string construir_prompt_sistema_(const std::string &idioma,
                                        const std::string &tarea);

  // ── RAG interno ────────────────────────────────────────────────────────
  // Vectorización simple (TF-IDF o embeddings del LLM)
  std::vector<float> vectorizar_texto_(const std::string &texto);

  // Similitud coseno entre dos vectores
  static float similitud_coseno_(const std::vector<float> &a,
                                 const std::vector<float> &b);

  // ── Verificación académica ─────────────────────────────────────────────
  // Consultar ORCID para verificar grado académico
  std::optional<std::string> verificar_orcid_(const std::string &nombre);

  // Consultar Google Scholar
  std::optional<std::string>
  verificar_google_scholar_(const std::string &nombre);

  // ── Estado interno ─────────────────────────────────────────────────────
  struct ImplLLM; // Pimpl para aislar dependencias de llama.cpp
  std::unique_ptr<ImplLLM> impl_llm_;

  mutable std::mutex mutex_;
  bool modelo_cargado_ = false;

  // Índice RAG en memoria: ruta_pdf -> [(contenido, vector)]
  struct EntradaCorpus {
    std::string contenido;
    std::vector<float> vector_embedding;
    int32_t pagina;
  };
  std::unordered_map<std::string, std::vector<EntradaCorpus>> indice_rag_;

  // Mapa de nombres de idiomas para prompts del sistema
  static const std::unordered_map<std::string, std::string> NOMBRES_IDIOMAS_;

  // Referencia a GraphConnector para In-Context Learning
  class GraphConnector* grafo_icl_ = nullptr;
};

} // namespace sursur
