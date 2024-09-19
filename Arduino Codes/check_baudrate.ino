#include <SPI.h>
#include <mcp_can.h>

#define CAN_CS_PIN 10
MCP_CAN canController(CAN_CS_PIN); // Create MCP_CAN instance

// List of common CAN baud rates and their readable values
unsigned long baudRates[] = {CAN_125KBPS, CAN_250KBPS, CAN_500KBPS, CAN_1000KBPS};
const char *baudRateNames[] = {"125 kbps", "250 kbps", "500 kbps", "1000 kbps"};

unsigned int currentBaudRateIndex = 0;

bool detectBaudRate()
{
    for (int i = 0; i < sizeof(baudRates) / sizeof(baudRates[0]); i++)
    {
        currentBaudRateIndex = i;
        Serial.print("Trying baud rate: ");
        Serial.println(baudRateNames[i]); // Print the human-readable baud rate

        // Try to initialize MCP2515 with the current baud rate
        if (canController.begin(MCP_ANY, baudRates[i], MCP_8MHZ) == CAN_OK)
        {
            Serial.println("CAN Bus Initialized. Trying communication...");

            // Try sending a test message (Controlword or Node ID request)
            unsigned char data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            if (canController.sendMsgBuf(0x601, 0, 8, data) == CAN_OK)
            {
                Serial.println("CAN communication successful at this baud rate!");
                return true; // Baud rate found, exit the loop
            }
            else
            {
                Serial.println("Failed to send message. Trying next baud rate...");
            }
        }
        else
        {
            Serial.println("Failed to initialize CAN bus at this baud rate.");
        }

        delay(1000); // Small delay before trying the next baud rate
    }
    return false; // If no baud rate was successful
}

void setup()
{
    Serial.begin(115200);

    if (detectBaudRate())
    {
        Serial.print("Baud rate detected: ");
        Serial.println(baudRateNames[currentBaudRateIndex]); // Print detected baud rate
    }
    else
    {
        Serial.println("Failed to detect baud rate.");
    }
}

void loop()
{
    // Empty loop for baud rate detection
}
