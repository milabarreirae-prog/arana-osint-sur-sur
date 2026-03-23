#include <iostream>
#include <cassert>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include "spider.hpp"

// Símbolo global requerido por el enlazador (referenciado por otros módulos)
std::atomic<bool> g_apagar(false);

/**
 * @brief Test de unidad para la extracción de texto del DOM.
 * Verifica que se extraiga el contenido útil y se ignore el ruido (scripts/styles).
 */
void test_extraccion_dom() {
    sursur::Spider spider;

    // HTML de prueba sucio
    std::string html_sucio = 
        "<html><body>"
        "<h1>Título</h1>"
        "<script>alert('malicioso');</script>"
        "<p>Texto limpio</p>"
        "<style>body{color:red;}</style>"
        "</body></html>";

    // Procesamiento
    std::string resultado = spider.extraer_texto(html_sucio);

    // Verificaciones
    assert(resultado.find("Título ") != std::string::npos);
    assert(resultado.find("Texto limpio ") != std::string::npos);
    
    // Verificar que el ruido fue eliminado
    assert(resultado.find("alert") == std::string::npos);
    assert(resultado.find("color:red") == std::string::npos);
}

/**
 * @brief Test de integración para la descarga de URLs.
 */
void test_descarga_red() {
    sursur::Spider spider;

    // Caso exitoso
    std::string html = spider.descargar_url("http://example.com");
    assert(!html.empty());
    assert(html.find("Example Domain") != std::string::npos);

    // Caso fallido (dominio inexistente)
    std::string html_error = spider.descargar_url("http://dominio-inexistente-12345.com");
    assert(html_error.empty());
}

/**
 * @brief Test de concurrencia y apagado elegante (Graceful Shutdown).
 */
void test_concurrencia_apagado() {
    sursur::Spider spider;

    // Iniciar 4 hilos trabajadores
    spider.iniciar_bucle(4);

    // Dejar que los hilos corran un breve periodo
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Solicitar apagado ordenado
    spider.detener_todos();
    
    // Si llegamos aquí sin quedar bloqueados, el test es exitoso.
}

int main() {
    std::cout << "Iniciando pruebas de Spider..." << std::endl;
    
    test_extraccion_dom();
    std::cout << "TEST DOM PASADO" << std::endl;
    
    test_descarga_red();
    std::cout << "TEST RED PASADO" << std::endl;
    
    test_concurrencia_apagado();
    std::cout << "TEST CONCURRENCIA PASADO" << std::endl;
    
    return 0;
}
