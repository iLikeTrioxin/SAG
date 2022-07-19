class Command {
public:
    const char* name;
    byte args;
    String* (*handler)(String*);

    Command(const char* name, int args, String* (*handler)(String*)) {
        this->name = name;
        this->args = args;
        this->handler = handler;
    }

    static String getArg(String* command, byte index = 0) {
        int start = 0;
        int end = 0;

        for(byte i = 0; i < index; i++)
            start = command->indexOf(' ', start) + 1;

        end = command->indexOf(' ', start);

        if(end == -1) end = command->length();

        return command->substring(start, end);
    }

    String* doIfValid(String* command) {
        if(!command->startsWith(name)) return nullptr;
        
        byte args = 0;

        for(char c : *command)
            if(c == ' ') args++;

        if(args != this->args) return nullptr;

        String* response = handler(command);

        return response;
    }
};