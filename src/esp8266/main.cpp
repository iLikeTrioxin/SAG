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

void handleSerial() {
    digitalWrite(LED_BUILTIN, HIGH);

    if(server.hasArg("msg")) {
        Serial.println(server.arg("msg"));

        while(!Serial.available()) delay(50);
        
        server.send(200, "application/json", Serial.readString());
    }else{
        server.send(200, "application/json", R"({"error": "no 'msg' argument"})");
    }
    
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
    Serial.println(SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }

    server.on("/", []() {
        server.send(200, "text/html", R"~(Theres nothing here. If you want to monitor this project consider using SAGA (https://github.com/iLikeTrioxin/SAGA).)~");
    });
    server.on("/serial", handleSerial);
    server.onNotFound(handleNotFound);
    
    server.begin();

    Serial.println("HTTP server started");
}

void loop(void) {
    server.handleClient();
    MDNS.update();
}