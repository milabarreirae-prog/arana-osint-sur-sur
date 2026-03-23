// ============================================================================
// Araña OSINT Sur-Sur — GraphConnector (Conector de Base de Datos de Grafos)
// Almacenamiento estructural de relaciones entre entidades OSINT.
// Nodos: [Hablante], [Fuente], [RecorteAudio], [ConceptoAnalitico],
//        [IdiomaOriginal], [Documento], [Organizacion].
// Incluye subrutina de archivado local anti-censura.
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

#include <sqlite3.h>

namespace sursur {

/// Tipos de nodo en el grafo OSINT.
enum class TipoNodo {
  HABLANTE,           // Persona que habla/escribe
  FUENTE,             // Medio o publicación
  RECORTE_AUDIO,      // Fragmento de audio extraído
  CONCEPTO_TEORICO,   // Concepto académico/teórico identificado (legacy)
  CONCEPTO_ANALITICO, // Concepto dinámico extraído por LLM
  IDIOMA_ORIGINAL,    // Idioma del contenido original
  DOCUMENTO,          // Documento o artículo completo
  ORGANIZACION        // Institución o entidad
};

/// Tipos de relación entre nodos.
enum class TipoRelacion {
  CITA_A,       // Un hablante cita a otro
  HABLA_EN,     // Un hablante habla en un idioma
  PERTENECE_A,  // Entidad pertenece a organización
  PUBLICA_EN,   // Hablante publica en fuente
  CONTIENE,     // Documento contiene recorte de audio
  TRATA_SOBRE,  // Documento/recorte trata sobre concepto
  DERIVADO_DE,  // Contenido derivado de otro
  TRADUCIDO_DE, // Contenido traducido de otro idioma
  ARCHIVADO_EN, // Contenido archivado localmente
  ANALIZA,      // Relación: RecorteAudio ANALIZA ConceptoAnalitico con marco teórico
  EJEMPLO_DE    // Relación de un extracto siendo ejemplo positivo de un nodo
};

/// Representación de un nodo en el grafo.
struct NodoGrafo {
  std::string id;
  TipoNodo tipo;
  std::string etiqueta; // Nombre legible del nodo
  std::unordered_map<std::string, std::string> propiedades;

  /// Obtener el nombre del tipo de nodo como cadena.
  std::string tipo_cadena() const;
};

/// Representación de una relación entre nodos.
struct RelacionGrafo {
  std::string id_origen;
  std::string id_destino;
  TipoRelacion tipo;
  std::unordered_map<std::string, std::string> propiedades;

  /// Obtener el nombre del tipo de relación como cadena.
  std::string tipo_cadena() const;
};

/// Resultado de una consulta al grafo.
struct ResultadoConsultaGrafo {
  std::vector<NodoGrafo> nodos;
  std::vector<RelacionGrafo> relaciones;
  int32_t total_resultados = 0;
  bool exitoso = false;
  std::string mensaje_error;
};

// ============================================================================
// Conector de Base de Datos de Grafos
// ============================================================================
class GraphConnector {
public:
  GraphConnector();
  ~GraphConnector();

  /// Prohibir copia.
  GraphConnector(const GraphConnector &) = delete;
  GraphConnector &operator=(const GraphConnector &) = delete;

  /// Conectar a la base de datos de grafos (Neo4j vía Bolt).
  /// @return true si la conexión fue exitosa.
  bool conectar();

  std::optional<std::string> buscar_similitud_vectorial(const std::vector<float>& embedding, float distancia_max = 0.15f);
  void vincular_fuente_duplicada(const std::string& id_noticia_existente, const std::string& url_nueva);
  void guardar_embedding(const std::string& id_nodo, const std::vector<float>& embedding);

  /// Verificar si la conexión está activa.
  bool esta_conectado() const;

  /// Inicializar el esquema SQLite.
  bool inicializar_bd();

  /// Desconectar de la base de datos.
  void desconectar();

  // ── Operaciones con nodos ──────────────────────────────────────────────

  /// Agregar un nodo nuevo al grafo.
  /// @param tipo Tipo de nodo (Hablante, Fuente, etc.)
  /// @param etiqueta Nombre legible del nodo
  /// @param propiedades Mapa de propiedades clave-valor
  /// @return ID del nodo creado, o std::nullopt en caso de error
  std::optional<std::string> agregar_nodo(
      TipoNodo tipo, const std::string &etiqueta,
      const std::unordered_map<std::string, std::string> &propiedades = {});

  /// Actualizar propiedades de un nodo existente (upsert).
  bool actualizar_nodo(
      const std::string &id_nodo,
      const std::unordered_map<std::string, std::string> &propiedades);

  /// Buscar nodo por ID.
  std::optional<NodoGrafo> buscar_nodo(const std::string &id_nodo);

  /// Buscar nodos por tipo.
  std::vector<NodoGrafo> buscar_nodos_por_tipo(TipoNodo tipo);

  // ── Operaciones con relaciones ─────────────────────────────────────────

  /// Crear una relación entre dos nodos.
  /// @param id_origen ID del nodo origen
  /// @param id_destino ID del nodo destino
  /// @param tipo Tipo de relación
  /// @param propiedades Propiedades adicionales de la relación
  /// @return true si la relación fue creada exitosamente
  bool crear_relacion(
      const std::string &id_origen, const std::string &id_destino,
      TipoRelacion tipo,
      const std::unordered_map<std::string, std::string> &propiedades = {});

  // ── Consultas ──────────────────────────────────────────────────────────

  /// Ejecutar una consulta Cypher arbitraria.
  ResultadoConsultaGrafo consultar(
      const std::string &consulta_cypher,
      const std::unordered_map<std::string, std::string> &parametros = {});

  /// Obtener el grafo de vecindad de un nodo (n saltos).
  ResultadoConsultaGrafo vecindad(const std::string &id_nodo,
                                  int32_t saltos = 1);

  // ── Archivado Anti-Censura ─────────────────────────────────────────────

  /// Archivar el estado completo del grafo localmente.
  /// @param ruta_destino Directorio de destino para el archivo
  /// @return true si el archivado fue exitoso
  bool archivar_grafo(const std::filesystem::path &ruta_destino = "");

  /// Exportar un subgrafo como JSON-LD.
  /// @param consulta_cypher Consulta que delimita el subgrafo a exportar
  /// @return Cadena JSON-LD con los datos del subgrafo
  std::string exportar_jsonld(
      const std::string &consulta_cypher = "MATCH (n) RETURN n LIMIT 1000");

  /// Restaurar grafo desde un archivo local.
  bool restaurar_desde_archivo(const std::filesystem::path &ruta_archivo);

  /// Ingestar resultado de evaluación LLM-JSON al grafo.
  /// Crea nodos [ConceptoAnalitico] dinámicos y aristas [ANALIZA] con marco
  /// teórico.
  /// @param id_recorte ID del nodo RecorteAudio origen
  /// @param axiomas_detectados Lista de axiomas cumplidos (del JSON del LLM)
  /// @param justificacion Justificación teórica del análisis
  /// @param marco_teorico Marco teórico aplicado
  /// @param aprobado Si fue aprobado por el filtro
  void ingestar_evaluacion(const std::string &id_recorte,
                           const std::vector<std::string> &axiomas_detectados,
                           const std::string &justificacion,
                           const std::string &marco_teorico, bool aprobado);

  /// Registrar una URL visitada en la base de datos (atómico).
  bool registrar_url(const std::string& url);

  /// Verificar si una URL ya ha sido procesada anteriormente.
  bool url_ya_visitada(const std::string& url);

  // ── Persistencia de Visitas por Fuente ─────────────────────────────

  /// Registrar/actualizar la visita a una fuente con metadatos HTTP.
  bool registrar_visita_fuente(const std::string& dominio,
                               const std::string& etag = "",
                               const std::string& last_modified = "",
                               const std::vector<std::string>& urls_conocidas = {});

  /// Retorna true si nunca se ha visitado este dominio.
  bool es_primera_visita(const std::string& dominio);

  /// Obtiene la lista de URLs ya procesadas para este dominio.
  std::vector<std::string> obtener_noticias_conocidas(const std::string& dominio);

  // ── Feedback Loop: TrustRank e In-Context Learning ───────────────────

  /// Actualizar el TrustRank de un nodo (fuente o recorte).
  /// @param id_nodo ID del nodo a actualizar
  /// @param delta Score delta (+1 o -1, acumulativo)
  void actualizar_trust_rank(const std::string &id_nodo, int delta);

  /// Guardar un extracto de 300 palabras como Ejemplo_Positivo.
  /// @param id_nodo ID del nodo fuente
  /// @param extracto Extracto de texto (si vacío, se genera desde el nodo)
  void guardar_ejemplo_positivo(const std::string &id_nodo,
                                const std::string &extracto);

  /// Obtener los mejores ejemplos positivos por TrustRank.
  /// @param top_n Número de ejemplos a retornar
  /// @return Vector de pares {id, extracto}
  std::vector<std::pair<std::string, std::string>>
  obtener_mejores_ejemplos(int top_n = 2);

  /// Comprueba si hay validación TrustRank positiva para un archivo/texto dado
  /// @param keyword Nombre de archivo o identificador a buscar
  /// @return true si tiene validación del curador (trust_rank > 0)
  bool esta_medio_validado(const std::string &keyword);

  // Convertir TipoNodo a etiqueta Cypher
  static std::string tipo_nodo_a_etiqueta_(TipoNodo tipo);

  // Convertir TipoRelacion a tipo Cypher
  static std::string tipo_relacion_a_cypher_(TipoRelacion tipo);

private:
  // ── Implementación interna (SQLite3) ────────
  sqlite3* db_ = nullptr;
  mutable std::mutex mutex_;
  bool conectado_ = false;

  // Ejecutar una consulta Cypher genérica
  bool ejecutar_cypher_(const std::string &cypher);

  // Construir sentencia CREATE para un nodo
  std::string construir_cypher_nodo_(
      TipoNodo tipo, const std::string &etiqueta,
      const std::unordered_map<std::string, std::string> &props);

  // Construir sentencia CREATE para una relación
  std::string construir_cypher_relacion_(
      const std::string &id_origen, const std::string &id_destino,
      TipoRelacion tipo,
      const std::unordered_map<std::string, std::string> &props);

  // Generar ID único para nodos
  static std::string generar_id_nodo_();

  // ── Estado interno ─────────────────────────────────────────────────────

  // Archivo local de respaldo (anti-censura)
  std::filesystem::path directorio_archivo_;
};

} // namespace sursur
