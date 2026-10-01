/*
 * credenciales.broker_publico.h - Alternativa sin cuenta: broker MQTT publico
 * ============================================================================
 *
 * PARA QUE SIRVE
 *   Para hacer la practica sin cuenta de ThingSpeak (MathWorks). Copie este
 *   archivo como "credenciales.h". El codigo del ESP32 no cambia: usa los
 *   mismos topicos y formatos que ThingSpeak, y el tablero web
 *   (tablero/tablero.html) hace el papel de la pagina del canal:
 *
 *     - dibuja la temperatura que envia el publicador
 *         topico  channels/<CANAL>/publish    mensaje  field1=31.25&status=...
 *     - sus botones envian al suscriptor el JSON que enviaria ThingSpeak
 *         topico  channels/<CANAL>/subscribe  mensaje  {"field1":"1", ...}
 *
 * CUIDADO: EL BROKER ES PUBLICO
 *   broker.hivemq.com no pide usuario ni contrasena, y cualquiera puede leer
 *   o escribir en cualquier topico. Por eso el "canal" es un texto unico
 *   (aqui, la direccion MAC de la placa) y no se envia nada privado. Si copia
 *   esta practica para otra placa, cambie TS_CHANNEL_ID y TS_CLIENT_ID por un
 *   texto suyo (por ejemplo, con su matricula) y escriba el mismo canal en el
 *   tablero.
 *
 *   Este archivo no tiene secretos, asi que si se sube al repositorio.
 * ============================================================================
 */
#pragma once

#define TS_BROKER_URI   "mqtt://broker.hivemq.com"     // puerto 1883, sin cifrar
#define TS_CHANNEL_ID   "esp32c6-e8f60afc9114"
#define TS_CLIENT_ID    "esp32c6-e8f60afc9114"         // unico en el broker
#define TS_USERNAME     ""                             // vacio: no se envia
#define TS_PASSWORD     ""
