#include <Arduino.h>
#include <SoftwareSerial.h>

// analog pins
#define PWM_PIN_AIR_PUMP  A1
#define PWM_PIN_PREHEATER A3
#define PWM_PIN_HEATER    A5
#define THERMOPROBE_PIN   A5

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
#define JSON_OK R"({"status":"ok"})"

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

typedef int8_t sbyte;
typedef int16_t sword;
typedef int32_t sdword;
typedef int64_t sqword;

class Command {
public:
    const char* name;
    int args;
    String (*handler)(String&);

    Command(const char* name, int args, String (*handler)(String&)) {
        this->name = name;
        this->args = args;
        this->handler = handler;
    }

    static bool doIfValid(String& command) {
        if(!command.startswith(name)) return false;
        
        byte args = 0;

        for(char c : command)
            if(c == ' ') args++;

        if(args != this->args) return false;

        espSerial.println(handler(command));

        return true;
    }
};

String getArg(String& command, byte index = 0) {
    int start = 0;
    int end = 0;

    for(int i = 0; i < index; i++) {
        start = command.indexOf(' ', start) + 1;
    }

    end = command.indexOf(' ', start);

    if(end == -1) end = command.length();

    return command.substring(start, end);
}

const auto commands = {
    Command("info"    , 0, [](String&){             return getInfo();}),
    Command("shutdown", 0, [](String&){ shutdown(); return JSON_OK;  }),
    Command("activate", 0, [](String&){ activate(); return JSON_OK;  }),
    Command("set", 2, [](String& c){
        String arg = getArg(c, 0);

             if(arg == "targettemp") tempTarget    = getArg(c, 1).toInt();
        else if(arg == "airpumprpm") pumpTargetRpm = getArg(c, 1).toInt();
        else if(arg == "pumpminrpm") pumpMinRpm    = getArg(c, 1).toInt();

        return JSON_OK;
    })
}

// safety limits
const
word tempCritical  = 270;
byte pumpMinRpm    = 40;

bool isShutdown = true;

// info
byte pumpTargetRpm = 100;
byte tempTarget    = 240;
byte airPumpPwm    = 0;
byte heaterPwm     = 0;
byte temp          = 0;
byte rpm           = 0;

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

String getInfo() {
    String json;

    json.reserve(100);

    json += "{";

    json += "\"time\":"          + String(millis()     ) + ",";
    json += "\"pumpTargetRpm\":" + String(pumpTargetRpm) + ",";
    json += "\"tempTarget\":"    + String(tempTarget   ) + ",";
    json += "\"airPumpPwm\":"    + String(airPumpPwm   ) + ",";
    json += "\"heaterPwm\":"     + String(heaterPwm    ) + ",";
    json += "\"temp\":"          + String(temp         ) + ",";
    json += "\"rpm\":"           + String(rpm          ) + ",";
    json += "\"shutdown\":"      + String(isShutdown   );

    json += "}";

    return json;
}

void dispatchSerial() {
    if(!espSerial.available()) return;
    
    String c = espSerial.readString(); c.toLowerCase();
    
    Serial.println("esp: " + c);
    
    for(auto& command : commands )
        if(command.doIfValid(c)) return;
    
    espSerial.println("Unknown command or invalid usage.");
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
