/*
 * cliente_mqtt.js - Cliente MQTT de prueba en el PC (broker publico).
 *
 * Sirve para comprobar el firmware sin el tablero: escucha lo que publica la
 * placa y envia al suscriptor el JSON que enviaria ThingSpeak.
 *
 * Uso (desde esta carpeta, tras "npm install"):
 *   node cliente_mqtt.js observar 90 [archivo]      escucha channels/<canal>/# durante 90 s
 *   node cliente_mqtt.js enviar 1                    publica {"field1":"1", ...} al suscriptor
 *   node cliente_mqtt.js secuencia "38:1,58:0" [archivo]
 *                                                    field1=1 a los 38 s y field1=0 a los 58 s
 *
 * Variables de entorno: BROKER (por defecto mqtt://broker.hivemq.com:1883) y
 * CANAL (por defecto el de firmware/credenciales.broker_publico.h).
 */
const fs = require("fs");
const mqtt = require("mqtt");

const BROKER = process.env.BROKER || "mqtt://broker.hivemq.com:1883";
const CANAL = process.env.CANAL || "esp32c6-e8f60afc9114";
const [, , modo, arg, archivo] = process.argv;

const salida = archivo ? fs.createWriteStream(archivo) : null;
const t0 = Date.now();
const log = (texto) => {
  const linea = `[${new Date().toISOString()}] +${((Date.now() - t0) / 1000).toFixed(1)} s  ${texto}`;
  console.log(linea);
  if (salida) salida.write(linea + "\n");
};
const terminar = () => { cliente.end(); if (salida) salida.end(); };

// El mismo JSON que ThingSpeak envia en channels/<canal>/subscribe.
//   valor "null"     -> "field1":null (entrada sin field1)
//   valor "raw:..."  -> el texto tal cual, sin JSON (mensaje malformado)
let entrada = 0;
function mensajeCanal(valor) {
  if (String(valor).startsWith("raw:")) return String(valor).slice(4);
  entrada += 1;
  return JSON.stringify({
    channel_id: CANAL, created_at: new Date().toISOString().replace(/\.\d+Z$/, "Z"),
    entry_id: entrada, field1: valor === "null" ? null : String(valor),
    field2: null, field3: null, field4: null, field5: null, field6: null, field7: null, field8: null,
    latitude: null, longitude: null, elevation: null, status: null,
  });
}
function enviar(valor, alTerminar) {
  const topico = `channels/${CANAL}/subscribe`;
  cliente.publish(topico, mensajeCanal(valor), { qos: 0, retain: false }, (err) => {
    log(err ? "ERROR " + err.message : `enviado field1=${valor} a ${topico}`);
    if (alTerminar) alTerminar();
  });
}

if (!["observar", "enviar", "secuencia"].includes(modo)) {
  console.log("modos: observar <segundos> | enviar <valor> | secuencia \"seg:valor,...\"");
  process.exit(1);
}

const cliente = mqtt.connect(BROKER, { clientId: "pc-" + Math.random().toString(16).slice(2, 10), clean: true });
cliente.on("error", (e) => { log("ERROR " + e.message); process.exit(1); });
cliente.on("message", (topico, datos) => log(`${topico}  ${datos.toString()}`));

cliente.on("connect", () => {
  log(`conectado a ${BROKER}`);
  if (modo === "observar") {
    const topico = `channels/${CANAL}/#`;
    cliente.subscribe(topico, { qos: 0 }, (err) => log(err ? "ERROR " + err.message : `suscrito a ${topico}`));
    setTimeout(() => { log("fin"); terminar(); }, Number(arg || 60) * 1000);
  } else if (modo === "enviar") {
    enviar(arg ?? "1", terminar);
  } else {
    // "segundos:valor"; se corta solo en el primer ":" (el valor puede ser "raw:texto")
    const pasos = String(arg || "38:1,58:0").split(",").map((p) => [p.slice(0, p.indexOf(":")), p.slice(p.indexOf(":") + 1)]);
    for (const [seg, valor] of pasos) {
      setTimeout(() => enviar(valor), Number(seg) * 1000 - (Date.now() - t0));
    }
    const fin = Math.max(...pasos.map(([s]) => Number(s))) + 3;
    setTimeout(() => { log("fin"); terminar(); }, fin * 1000 - (Date.now() - t0));
  }
});
