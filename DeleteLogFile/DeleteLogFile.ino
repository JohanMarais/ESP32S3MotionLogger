#include <SPIFFS.h>
HWCDC USBSerial;
void setup() {
    USBSerial.begin(115200);

    if (!SPIFFS.begin(true)) {  // Mount SPIFFS
        USBSerial.println("SPIFFS Mount Failed!");
        return;
    }

    // Attempt to delete the log file
    if (SPIFFS.exists("/datalog.txt")) {
        if (SPIFFS.remove("/datalog.txt")) {
            USBSerial.println("Log file deleted successfully!");
        } else {
            USBSerial.println("Failed to delete log file.");
        }
    } else {
        USBSerial.println("Log file does not exist.");
    }
}

void loop() {}
