#include <SPI.h>
#include <mcp_can.h>

#define CAN_CS_PIN 10
MCP_CAN canController(CAN_CS_PIN); // Create MCP_CAN instance

// Define your EPOS4 Node ID (default is 1)
const uint8_t NODE_ID = 1;
const long unsigned int CAN_ID_SDO = 0x600 + NODE_ID;       // SDO write COB-ID
const long unsigned int CAN_ID_HEARTBEAT = 0x700 + NODE_ID; // Heartbeat COB-ID
const long unsigned int CAN_ID_STATUS = 0x580 + NODE_ID;    // Statusword response COB-ID

long unsigned int rxId;
unsigned char len = 0;
unsigned char buf[8]; // Buffer for received CAN data

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // Wait for Serial to initialize

    // Initialize MCP2515 at 125 kbps
    if (canController.begin(MCP_ANY, CAN_125KBPS, MCP_8MHZ) == CAN_OK)
    {
        Serial.println("CAN Bus Initialized Successfully!");
    }
    else
    {
        Serial.println("Error Initializing CAN Bus...");
        while (1)
            ; // Halt if CAN initialization fails
    }

    canController.setMode(MCP_NORMAL); // Set MCP2515 to Normal Mode
    Serial.println("Enter Controlword in HEX format to control the motor (e.g., 0x000F for start, 0x0006 for stop):");

    // Set mode of operation to Profile Velocity Mode (PVM)
    setModeOfOperation(3);

    // Enable operation by sending the necessary Controlwords
    enableOperation();
}

void loop()
{
    // Handle Serial Input for Commands
    if (Serial.available())
    {
        String input = Serial.readStringUntil('\n');
        input.trim(); // Remove any extra spaces or line breaks

        // Check if the input is a valid hex value for Controlword
        if (input.startsWith("0x") || input.startsWith("0X"))
        {
            uint16_t controlWord = (uint16_t)strtol(input.c_str(), NULL, 16); // Convert input from hex to integer
            sendControlWord(controlWord);                                     // Send the control word to the motor
        }
        else if (input.startsWith("speed"))
        {
            // Set motor speed based on the input
            String speedHex = input.substring(6);                             // Get the hex string after 'speed'
            int32_t speedValue = (int32_t)strtol(speedHex.c_str(), NULL, 16); // Convert hex string to int32_t
            setTargetVelocity(speedValue);                                    // Send the speed value
            Serial.print("Motor Speed Set to: ");
            Serial.print(speedValue, HEX); // Print the speed in hex
            Serial.println(" (Hexadecimal)");
        }
        else if (input.equalsIgnoreCase("readstatus"))
        {
            // Request and read the Statusword
            requestStatusWord();
        }
        else
        {
            Serial.println("Unknown command. Use HEX values for Controlword or 'speed <hex_value>' for speed.");
        }
    }

    // Listen for incoming CAN messages
    if (canController.checkReceive() == CAN_MSGAVAIL)
    {
        if (canController.readMsgBuf(&rxId, &len, buf) == CAN_OK)
        {
            // Check if it's a Statusword message
            if (rxId == CAN_ID_STATUS)
            {
                uint16_t statusword = (buf[1] << 8) | buf[0]; // Combine first two bytes
                Serial.print("Statusword: ");
                Serial.println(statusword, HEX);
                interpretStatusWord(statusword); // Interpret and display the drive state
            }
        }
    }

    delay(100); // Short delay to prevent flooding
}

// Function to set the Mode of Operation (0x6060)
void setModeOfOperation(uint8_t mode)
{
    unsigned char data[8];
    data[0] = 0x2F; // SDO write, expedited, 1 byte
    data[1] = 0x60; // Index low byte
    data[2] = 0x60; // Index high byte
    data[3] = 0x00; // Subindex
    data[4] = mode; // Mode of Operation (3 for Profile Velocity Mode)
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;

    Serial.print("Setting Mode of Operation to: ");
    Serial.println(mode);

    if (canController.sendMsgBuf(CAN_ID_SDO, 0, 8, data) == CAN_OK)
    {
        Serial.println("Mode of Operation set successfully.");
    }
    else
    {
        Serial.println("Error setting Mode of Operation");
    }
}

// Function to send Controlword (0x6040)
void sendControlWord(uint16_t controlWord)
{
    unsigned char data[8];
    data[0] = 0x2B;                  // SDO write, expedited, 2 bytes
    data[1] = 0x40;                  // Index low byte
    data[2] = 0x60;                  // Index high byte
    data[3] = 0x00;                  // Subindex
    data[4] = lowByte(controlWord);  // Controlword low byte
    data[5] = highByte(controlWord); // Controlword high byte
    data[6] = 0x00;                  // Reserved
    data[7] = 0x00;                  // Reserved

    Serial.print("Sending Controlword: ");
    Serial.println(controlWord, HEX);

    if (canController.sendMsgBuf(CAN_ID_SDO, 0, 8, data) == CAN_OK)
    {
        Serial.println("Controlword Sent Successfully");
    }
    else
    {
        Serial.println("Error Sending Controlword...");
    }
}

// Function to set Target Velocity (0x60FF)
void setTargetVelocity(int32_t velocity)
{
    unsigned char data[8];
    data[0] = 0x23;                     // SDO write, expedited, 4 bytes
    data[1] = 0xFF;                     // Index low byte
    data[2] = 0x60;                     // Index high byte
    data[3] = 0x00;                     // Subindex
    data[4] = lowByte(velocity);        // Velocity byte 0
    data[5] = highByte(velocity);       // Velocity byte 1
    data[6] = lowByte(velocity >> 16);  // Velocity byte 2
    data[7] = highByte(velocity >> 24); // Velocity byte 3

    Serial.print("Sending Target Velocity: ");
    Serial.println(velocity);

    if (canController.sendMsgBuf(CAN_ID_SDO, 0, 8, data) == CAN_OK)
    {
        Serial.println("Target Velocity Command Sent");
    }
    else
    {
        Serial.println("Error Sending Target Velocity Command");
    }
}

// Function to transition the drive to "Operation Enabled" state
void enableOperation()
{
    // Step 1: Send 0x0006 - Ready to Switch On
    sendControlWord(0x0006);
    delay(100);

    // Step 2: Send 0x0007 - Switched On
    sendControlWord(0x0007);
    delay(100);

    // Step 3: Send 0x000F - Operation Enabled
    sendControlWord(0x000F);
    delay(100);

    Serial.println("Drive is now in Operation Enabled state.");
}

// Function to request Statusword (0x6041)
void requestStatusWord()
{
    unsigned char data[8];
    data[0] = 0x40; // SDO read command
    data[1] = 0x41; // Index low byte (0x6041)
    data[2] = 0x60; // Index high byte (0x6041)
    data[3] = 0x00; // Subindex 0
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;

    if (canController.sendMsgBuf(CAN_ID_SDO, 0, 8, data) == CAN_OK)
    {
        Serial.println("Statusword Request Sent");
    }
    else
    {
        Serial.println("Error Sending Statusword Request");
    }
}

// Function to interpret and display the Statusword (0x6041)
void interpretStatusWord(uint16_t statusword)
{
    Serial.print("Interpreted Statusword: ");
    Serial.println(statusword, BIN); // Print Statusword in binary format

    if ((statusword & 0x006F) == 0x0021)
    {
        Serial.println("State: Switch on disabled");
    }
    else if ((statusword & 0x006F) == 0x0023)
    {
        Serial.println("State: Ready to switch on");
    }
    else if ((statusword & 0x006F) == 0x0027)
    {
        Serial.println("State: Switched on");
    }
    else if ((statusword & 0x006F) == 0x0037)
    {
        Serial.println("State: Operation enabled");
    }
    else if ((statusword & 0x006F) == 0x0017)
    {
        Serial.println("State: Quick stop active");
    }
    else if ((statusword & 0x004F) == 0x000F)
    {
        Serial.println("State: Fault reaction active");
    }
    else if ((statusword & 0x004F) == 0x0008)
    {
        Serial.println("State: Fault");
    }
    else
    {
        Serial.println("State: Unknown");
    }
}
