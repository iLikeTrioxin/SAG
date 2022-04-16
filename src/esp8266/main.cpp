/* 
 * This is arduino nano based project using modules esp8266 and PWM regulators
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// File with defines for SSID and PASS of wifi network
#include "credentials.h"

ESP8266WebServer server(80);

void handleApi() {
    digitalWrite(LED_BUILTIN, HIGH);

    if(!server.args() || !server.hasArg('method')) return;

    String method   = server.arg('method');
    String response = "";

    switch (method) {
    case "setcriticaltemp":
        if(server.hasArg('temp')) Serial.send("setcriticaltemp " + server.arg('temp'));
        break;
    case "settargettemp":
        if(server.hasArg('temp')) Serial.send("settargettemp " + server.arg('temp'));
        break;
    case "setpumpminrpm":
        if(server.hasArg('rpm')) Serial.send("setpumpminrpm " + server.arg('rpm'));
        break;
    case "setpumptargetrpm":
        if(server.hasArg('rpm')) Serial.send("setpumptargetrpm " + server.arg('rpm'));
        break;
    case "activate":
        Serial.send("activate");
        break;
    case "shutdown":
        Serial.send("shutdown");
        break;
    case "info":
        Serial.send("info");
        while(!Serial.available()) delay(100);
        response = Serial.readString();
        break;
    default:
        break;
    }

    server.send(200, "application/json", response);
    digitalWrite(LED_BUILTIN, LOW);
}

void handleNotFound() {
    digitalWrite(LED_BUILTIN, HIGH);
    
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
    
    digitalWrite(LED_BUILTIN, LOW);
}

void setup(void) {
    pinMode(LED_BUILTIN, OUTPUT);

    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(9600);

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }

    server.on("/api", handleApi);
    server.onNotFound(handleNotFound);
    
    server.begin();

    Serial.println("HTTP server started");
}

void loop(void) {
    server.handleClient();
    MDNS.update();
}