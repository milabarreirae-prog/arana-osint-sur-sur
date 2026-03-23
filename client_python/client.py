#!/usr/bin/env python3
import sys
import grpc
import sursur_service_pb2
import sursur_service_pb2_grpc

def main():
    if len(sys.argv) < 2:
        print("Uso: python client.py [comando] [argumentos]")
        print("Comandos:")
        print("  semilla <url>      - Envia una URL RSS para rastreo")
        print("  evaluar <texto>    - Evalúa un texto usando el LLM")
        sys.exit(1)

    comando = sys.argv[1]
    
    # Conectar al demonio OSINT en localhost:50051
    canal = grpc.insecure_channel('localhost:50051')
    
    if comando == "semilla":
        if len(sys.argv) < 3:
            print("Error: falta la URL")
            sys.exit(1)
        url = sys.argv[2]
        cliente = sursur_service_pb2_grpc.SpiderServiceStub(canal)
        peticion = sursur_service_pb2.SemillaRequest(url_rss=url, saltos_maximos=2)
        try:
            respuesta = cliente.EnviarSemilla(peticion)
            print(f"Respuesta del Demonio: {respuesta.mensaje} (Aceptada: {respuesta.aceptada})")
        except grpc.RpcError as e:
            print(f"GPRC Error: {e.details()}")

    elif comando == "evaluar":
        if len(sys.argv) < 3:
            print("Error: falta el texto a evaluar")
            sys.exit(1)
        texto = sys.argv[2]
        cliente = sursur_service_pb2_grpc.EvaluatorServiceStub(canal)
        peticion = sursur_service_pb2.DocumentoRequest(
            contenido_texto=texto, 
            idioma_origen="es",
            url_fuente="http://test.local"
        )
        try:
            respuesta = cliente.EvaluarDocumento(peticion)
            print(f"--- Resultado de Evaluación ---")
            print(f"Puntuación de Rigor: {respuesta.puntuacion_rigor}")
            print(f"Resumen: {respuesta.resumen_evaluacion}")
        except grpc.RpcError as e:
            print(f"GPRC Error: {e.details()}")

    else:
        print(f"Comando desconocido: {comando}")

if __name__ == "__main__":
    main()
