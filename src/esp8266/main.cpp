#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// File with defines for SSID and PASS of wifi network
#include "credentials.h"

ESP8266WebServer server(80);

#define DEBUG

#if defined(DEBUG)
#    define DS(x) x
#else
#    define DS(x)
#endif

void handleSerial() {
    digitalWrite(LED_BUILTIN, HIGH);

    if(server.hasArg("msg")) {
        // clear input buffer
        while(Serial.available()) Serial.read();

        Serial.print(server.arg("msg"));
        Serial.flush();

        while(!Serial.available()) delay(100);
        
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
    Serial.setTimeout(50);

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) delay(500);

    DS(Serial.println("toggleCommands")); // redirect following serial messages to serial monitor
    DS(Serial.print  ("Connected to " ));
    DS(Serial.println(SSID            ));
    DS(Serial.print  ("IP address: "  ));
    DS(Serial.println(WiFi.localIP()  ));

    if (MDNS.begin("esp8266")) {
        DS(Serial.println("MDNS responder started"));
    }

    server.on("/", []() {
        server.send(200, "text/html", R"~(Theres nothing here. If you want to monitor this project consider using SAGA (https://github.com/iLikeTrioxin/SAGA).)~");
    });
    server.on("/serial", handleSerial);
    server.onNotFound(handleNotFound);
    
    server.begin();

    DS(Serial.println("HTTP server started"));
    DS(Serial.println("toggleCommands"     ));

    // clear serial
    while(Serial.available()) Serial.read();
}

void loop(void) {
    server.handleClient();
    MDNS.update();
}