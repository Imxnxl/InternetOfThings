/*
 * credenciales.ejemplo.h - Plantilla de las credenciales de ThingSpeak
 * ============================================================================
 *
 * COMO USARLA
 *   1. Copie este archivo como "credenciales.h" en esta misma carpeta
 *      (firmware/). Lo usan los dos programas: el publicador y el suscriptor.
 *   2. Reemplace los textos TU_... por los datos de su cuenta de ThingSpeak.
 *
 *   credenciales.h esta en .gitignore: las contrasenas no se suben al
 *   repositorio, que es publico. La red Wi-Fi no va aqui: se configura con
 *   "idf.py menuconfig" (ver el README).
 *
 *   Sin cuenta de ThingSpeak, use credenciales.broker_publico.h en su lugar.
 *
 * DE DONDE SALE CADA DATO
 *   TS_CHANNEL_ID   Channels -> My Channels -> su canal: el numero
 *                   "Channel ID" que aparece debajo del nombre.
 *   TS_CLIENT_ID    Devices -> MQTT -> Add a new device. Al crear el
 *   TS_USERNAME     dispositivo, ThingSpeak muestra las tres cadenas una sola
 *   TS_PASSWORD     vez (boton "Download Credentials"). No son la Write API
 *                   Key del canal. En ThingSpeak, el Client ID y el Username
 *                   suelen ser la misma cadena.
 * ============================================================================
 */
#pragma once

#define TS_CHANNEL_ID   "TU_CANAL_ID"
#define TS_CLIENT_ID    "TU_THINGSPEAK_MQTT_CLIENT_ID"
#define TS_USERNAME     "TU_THINGSPEAK_MQTT_USERNAME"
#define TS_PASSWORD     "TU_THINGSPEAK_MQTT_PASSWORD"
