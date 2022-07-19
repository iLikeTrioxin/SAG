#include <Arduino.h>
#include <SoftwareSerial.h>

#define DEBUG

// analog pins
#define PWM_PIN_AIR_PUMP  3 // OUT
#define PWM_PIN_PREHEATER 5 // OUT
#define PWM_PIN_HEATER    6 // OUT
#define THERMOPROBE_PIN   A5 // IN

// digital pins
#define PUMP_RPM_PIN 7
#define ESP_TX       2
#define ESP_RX       4

// voltage divider for thermoprobe
#define THERMISTOR_DIVIDER_RESISTOR 1000

// constants for Pt100 thermoprobe
#define PT100_R0 100
#define PT100_A  3.9083e-3
#define PT100_B -5.775e-7
#define SP_OK new String("\"OK\"")

#if defined(DEBUG)
#    define DS(x) x
#else
#    define DS(x)
#endif

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

typedef int8_t sbyte;
typedef int16_t sword;
typedef int32_t sdword;
typedef int64_t sqword;

// RX | TX
SoftwareSerial espSerial(ESP_RX, ESP_TX);

void activate();
void shutdown();
String* getInfo();

#include "Command.hpp"

// safety limits
const
word tempCritical  = 270;
byte pumpMinRpm    = 40;

bool isShutdown = false;
bool isCommand  = true;

// info
byte pumpTargetRpm = 100;
byte tempTarget    = 240;
byte airPumpPwm    = 0;
byte heaterPwm     = 0;
byte temp          = 0;
byte rpm           = 0;

const Command commands[] = {
    Command("info"    , 0, [](String*){             return getInfo();}),
    Command("shutdown", 0, [](String*){ shutdown(); return SP_OK;    }),
    Command("activate", 0, [](String*){ activate(); return SP_OK;    }),
    Command("set", 2, [](String* c){
        String arg = Command::getArg(c, 1); arg.toLowerCase();

             if(arg == "targettemp") { tempTarget    = Command::getArg(c, 2).toInt(); return SP_OK; }
        else if(arg == "airpumprpm") { pumpTargetRpm = Command::getArg(c, 2).toInt(); return SP_OK; }
        else if(arg == "pumpminrpm") { pumpMinRpm    = Command::getArg(c, 2).toInt(); return SP_OK; }

        return (String*) nullptr;
    }),
    Command("toggleCommands", 0, [](String*){ isCommand = !isCommand; return SP_OK; })
};

void setup() {
    espSerial.begin(9600);
       Serial.begin(9600);

    espSerial.setTimeout(50);
       Serial.setTimeout(50);
    
    pinMode(LED_BUILTIN, OUTPUT);

    pinMode(PWM_PIN_AIR_PUMP , OUTPUT);
    pinMode(PWM_PIN_PREHEATER, OUTPUT);
    pinMode(PWM_PIN_HEATER   , OUTPUT);

    pinMode(THERMOPROBE_PIN, INPUT);
    pinMode(PUMP_RPM_PIN   , INPUT);

    // initialy shut down everything
    shutdown();
}

String* getInfo() {
    String* json = new String();

    json->reserve(100);

    *json += "{";

    *json += "\"time\":"          + String(millis()     ) + ",";
    *json += "\"pumpTargetRpm\":" + String(pumpTargetRpm) + ",";
    *json += "\"tempTarget\":"    + String(tempTarget   ) + ",";
    *json += "\"airPumpPwm\":"    + String(airPumpPwm   ) + ",";
    *json += "\"heaterPwm\":"     + String(heaterPwm    ) + ",";
    *json += "\"temp\":"          + String(temp         ) + ",";
    *json += "\"rpm\":"           + String(rpm          ) + ",";
    *json += "\"shutdown\":"      + String(isShutdown   );

    *json += "}";

    return json;
}

void dispatchSerial() {
    if(!espSerial.available()) return;
    
    String  cmd = espSerial.readString();
    String* res;
    
    DS(Serial.println(cmd));
    DS(Serial.flush());

    if(!isCommand) return;

    cmd.toLowerCase();
    for(auto& command : commands ) {
        res = command.doIfValid(&cmd);

        if(res == nullptr) continue;

        espSerial.print("{\"exitCode\":0,\"result\":");
        espSerial.print(*res);
        espSerial.print("}");

        espSerial.flush();

        DS(Serial.print("{\"exitCode\":0,\"result\":"));
        DS(Serial.print(*res));
        DS(Serial.println("}"));

        DS(Serial.flush());

        delete res;

        return;
    }

    espSerial.print("{\"exitCode\":1,\"result\":\"Unknown command or invalid usage.\"}");
    espSerial.flush();

    DS(Serial.println("{\"exitCode\":1,\"result\":\"Unknown command or invalid usage.\"}"));
    DS(Serial.flush());
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

    if(r2 == 0) return 0;

    r2 *= (VIN / 1024.0);
    r2  = (VIN / r2    ) - 1;

    return R1 * r2;
}

void activate() { isShutdown = false; }
void shutdown() {
    isShutdown = true;

    analogWrite(PWM_PIN_AIR_PUMP , 0);
    analogWrite(PWM_PIN_PREHEATER, 0);
    analogWrite(PWM_PIN_HEATER   , 0);

    // make sure the pump have enough power to start
    analogWrite(PWM_PIN_AIR_PUMP , 255);
}

// returns rpm of the pump based on how often the pin is high
float measureRPM() {
    return 60.0f; // function is not good enough yet

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
    delay(200);

    dispatchSerial();
    
    if (isShutdown) return;

    // read the resistance of the thermistor
    float rt = measureOhms<THERMOPROBE_PIN, THERMISTOR_DIVIDER_RESISTOR, 5>();
    
    // convert to degrees C
    temp = measureTemp(rt);

    // read the rpm of the pump
    rpm = measureRPM(); // Needs big improvement
    
    if(temp >= tempCritical) return shutdown(); // shutdown if temp is too high
    if(rpm  <= pumpMinRpm  ) return shutdown(); // shutdown if pump is not working

    heaterPwm  = max(0, map(temp, 0, tempTarget   , 255, 0));
    airPumpPwm = max(0, map(rpm , 0, pumpTargetRpm, 255, 0));

    analogWrite(PWM_PIN_AIR_PUMP , airPumpPwm);
    analogWrite(PWM_PIN_PREHEATER, heaterPwm );
    analogWrite(PWM_PIN_HEATER   , heaterPwm );
}