#include <SoftwareSerial.h>

// pins
#define PWM_PIN_AIR_PUMP  5
#define PWM_PIN_PREHEATER 4
#define PWM_PIN_HEATER    3
#define THERMOPROBE_PIN   2
#define PUMP_RPM_PIN      14

// voltage divider for thermoprobe
#define THERMISTOR_DIVIDER_RESISTOR 1000

// constants for Pt100 thermoprobe
#define PT100_R0 100
#define PT100_A  3.9083e-3
#define PT100_B -5.775e-7

bool isShutdown        = true;
bool debug             = false;
int pump_min_rpm       = 40;
int temp_target        = 240;
int temp_critical      = 270;
int voltage_divider_r1 = 1000;

// RX | TX
SoftwareSerial espSerial(2, 3);

void setup() {

}

void dispatchSerial() {
    if(!espSerial.available()) return;
    
    String command = espSerial.readString().toLowerCase(); 

    if     (command.startsWith("setcriticaltemp"    )) temp_critical      = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("settargettemp"      )) temp_target        = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("setpumpminrpm"      )) pump_min_rpm       = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("setvoltagedividerr1")) voltage_divider_r1 = command.substring(command.indexOf(" ") + 1).toInt();
    else if(command.startsWith("activate"           )) isShutdown = false;
    else if(command.startsWith("shutdown"           )) isShutdown = true;
    else if(command.startsWith("debug"              )) debug      = !debug;
}

// returns degrees C based on the resistance of the thermistor (Pt100)
float measureTemp(float r) {
    double t = (PT100_R0 * PT100_R0) * (PT100_A * PT100_A);

    t -= PT100_R0 * PT100_B * 4 * (PT100_R0 - r);
    t -= PT100_R0 * PT100_A - sqrt(t);
    t /= PT100_R0 * PT100_B * 2;
    
    return t;
}

// calculates resistance in ohms from a voltage divider
template<int PIN, int R1, int VIN>
float measureOhms() {
    double v = analogRead(PIN) * VIN / 1024.0;
    double vDrop = (VIN / v) - 1;
    return R1 * vDrop;
}


void shutDown() {
    isShutdown = true;

    digitalWrite(PWM_PIN_AIR_PUMP , LOW);
    digitalWrite(PWM_PIN_PREHEATER, LOW);
    digitalWrite(PWM_PIN_HEATER   , LOW);

    while(isShutdown) {
        delay(1000);
        dispatchSerial();
        
    }
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
    float temp = measureTemp(rt);
    float rpm  = measureRPM();
    
    if(temp >= TEMP_CRITICAL) shutdown();
    if(rpm  <= PUMP_RPM_MIN ) shutdown();
    
    float heaterPwm = min(0, map(t, 0, TEMP_TARGET, 255, 0));

    analogWrite(PWM_PIN_PREHEATER, heaterPwm);
    analogWrite(PWM_PIN_HEATER   , heaterPwm);
    
    delay(200);
}
