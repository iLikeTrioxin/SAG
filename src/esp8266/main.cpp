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
    if(!server.args() || !server.hasArg('method')) return;

    String method = server.arg('method');

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
    case "setvoltagedividerr1":
        if(server.hasArg('r1')) Serial.send("setvoltagedividerr1 " + server.arg('r1'));
        break;
    case "activate":
        Serial.send("activate");
        break;
    case "shutdown":
        Serial.send("shutdown");
        break;
    case "debug":
        Serial.send("debug");
        break;
    default:
        break;
    }

    digitalWrite(led, 1);
    char temp[400];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf(temp, 400,

                     "<html>\
    <head>\
        <meta http-equiv='refresh' content='5'/>\
        <title>ESP8266 Demo</title>\
        <style>\
            body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
    </head>\
    <body>\
        <h1>Hello from ESP8266!</h1>\
        <p>Uptime: %02d:%02d:%02d</p>\
        <img src=\"/test.svg\" />\
    </body>\
</html>",

                     hr, min % 60, sec % 60
                    );
    server.send(200, "text/html", temp);
    digitalWrite(led, 0);
}

void handleNotFound() {
    digitalWrite(led, 1);
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
    digitalWrite(led, 0);
}

void setup(void) {
    pinMode(led, OUTPUT);
    digitalWrite(led, 0);
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
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