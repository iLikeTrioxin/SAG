#include <Arduino.h>
#include <SoftwareSerial.h>

// analog pins
#define PWM_PIN_AIR_PUMP  1
#define PWM_PIN_PREHEATER 3
#define PWM_PIN_HEATER    5
#define THERMOPROBE_PIN   7

// digital pins
#define PUMP_RPM_PIN 1
#define ESP_TX       2
#define ESP_RX       4

// voltage divider for thermoprobe
#define THERMISTOR_DIVIDER_RESISTOR 1000

// constants for Pt100 thermoprobe
#define PT100_R0 100
#define PT100_A  3.9083e-3
#define PT100_B -5.775e-7


// safety limits
unsigned short tempCritical  = 270;
unsigned short pumpMinRpm    = 40;

bool isShutdown = true;

// info
unsigned short pumpTargetRpm = 100;
unsigned short tempTarget    = 240;
unsigned short airPumpPwm    = 0;
unsigned short heaterPwm     = 0;
unsigned short temp          = 0;
unsigned short rpm           = 0;

// RX | TX
SoftwareSerial espSerial(ESP_RX, ESP_TX);

void setup() {
    espSerial.begin(9600);
    Serial.begin(9600);

    pinMode(PWM_PIN_AIR_PUMP , OUTPUT);
    pinMode(PWM_PIN_PREHEATER, OUTPUT);
    pinMode(PWM_PIN_HEATER   , OUTPUT);

    pinMode(THERMOPROBE_PIN, INPUT);
    pinMode(PUMP_RPM_PIN   , INPUT);
}

void sendInfo() {
    String json = "{";

    json += "\"time\":"          + String(millis()     ) + ",";
    json += "\"pumpTargetRpm\":" + String(pumpTargetRpm) + ",";
    json += "\"tempTarget\":"    + String(tempTarget   ) + ",";
    json += "\"airPumpPwm\":"    + String(airPumpPwm   ) + ",";
    json += "\"heaterPwm\":"     + String(heaterPwm    ) + ",";
    json += "\"temp\":"          + String(temp         ) + ",";
    json += "\"rpm\":"           + String(rpm          ) + ",";
    json += "\"shutdown\":"      + String(isShutdown   );

    json += "}";

    espSerial.println(json);
}

void dispatchSerial() {
    if(!espSerial.available()) return;
    
    String command = espSerial.readString();
    
    Serial.println("esp: " + command);
    
    command.toLowerCase(); 

    if     (command.startsWith("setcriticaltemp" )) tempCritical  = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("settargettemp"   )) tempTarget    = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("setpumpminrpm"   )) pumpMinRpm    = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("setpumptargetrpm")) pumpTargetRpm = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("activate"        )) isShutdown = false;
    else if(command.startsWith("shutdown"        )) isShutdown = true;
    else if(command.startsWith("info"            )) sendInfo();
}

// returns degrees C based on the resistance of the thermistor (Pt100)
float measureTemp(float r) {
    double t = (PT100_R0 * PT100_R0) * (PT100_A * PT100_A);

    t -=  PT100_R0 * PT100_B * 4 * (PT100_R0 - r);
    t  = -PT100_R0 * PT100_A + sqrt(t);
    t /=  PT100_R0 * PT100_B * 2;

    return t;
}

// calculates resistance in ohms from a voltage divider
template<int PIN, int R1, int VIN>
float measureOhms() {
    double r2 = analogRead(PIN);
    r2 *= (VIN / 1024.0);
    r2  = (VIN / r2    ) - 1;
    return R1 * r2;
}


void shutdown() {
    isShutdown = true;

    analogWrite(PWM_PIN_AIR_PUMP , LOW);
    analogWrite(PWM_PIN_PREHEATER, LOW);
    analogWrite(PWM_PIN_HEATER   , LOW);

    while(isShutdown) {
        delay(1000);
        dispatchSerial();
    }

    // make sure the pump have enough power to start
    analogWrite(PWM_PIN_AIR_PUMP , 255);
}

// returns rpm of the pump based on how often the pin is high
float measureRPM() {
    unsigned long start = millis();
    unsigned long end   = millis();
    unsigned long count = 0;

    while(end - start < 1000) {
        if(digitalRead(PUMP_RPM_PIN) == HIGH) count++;

        end = millis();
    }

    return count * 60.0 / 1000.0;
}

void loop() {
    // read the resistance of the thermistor
    float rt = measureOhms<THERMOPROBE_PIN, THERMISTOR_DIVIDER_RESISTOR, 5>();
    
    // convert to degrees C
    temp = measureTemp(rt);

    // read the rpm of the pump
    rpm = measureRPM();
    
    if(temp >= tempCritical) return shutdown(); // shutdown if temp is too high
    if(rpm  <= pumpMinRpm  ) return shutdown(); // shutdown if pump is not working

    heaterPwm  = max(0, map(temp, 0, tempTarget   , 255, 0));
    airPumpPwm = max(0, map(rpm , 0, pumpTargetRpm, 255, 0));

    analogWrite(PWM_PIN_AIR_PUMP , airPumpPwm);
    analogWrite(PWM_PIN_PREHEATER, heaterPwm );
    analogWrite(PWM_PIN_HEATER   , heaterPwm );
    
    dispatchSerial();

    delay(200);
}
