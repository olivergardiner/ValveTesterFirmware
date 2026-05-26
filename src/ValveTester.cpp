#include "ValveTester.h"

#include "Arduino.h"
#include "Wire.h"

#include "hardware.h"

enum
{
    ERR_NO_ERROR,
    ERR_INVALID_INFO,
    ERR_INVALID_MODE,
    ERR_INVALID_SET,
    ERR_INVALID_GET,
    ERR_I2C,
    ERR_GRID_RANGE,
    ERR_HT_RANGE,
    ERR_HT_TIMEOUT,
    ERR_UNSAFE,
    ERR_OFFSET_RANGE
};

const char *errorMessages[] = {
    "",
    "Invalid info command",
    "Invalid mode command",
    "Invalid set command",
    "Invalid get command",
    "I2C transmission failed",
    "Grid voltage out of range",
    "HT voltage out of range",
    "Timeout setting HT voltage",
    "Unsafe to test",
    "Offset voltage out of range"};

ValveTester::ValveTester()
    : parser(this, &ValveTester::infoCommand, &ValveTester::modeCommand,
             &ValveTester::getCommand, &ValveTester::setCommand, &ValveTester::commandError),
      grid1(0), grid2(0), targetHT1(0), targetHT2(0),
      measuredHT1(0), measuredHT2(0),
      currentLo1(0), currentMid1(0), currentHi1(0),
      currentLo2(0), currentMid2(0), currentHi2(0)
{
    Wire.begin(MASTER_ADDR);
}

void ValveTester::parseInput(char c)
{
    parser.parseInput(c);
}

void ValveTester::printValues()
{
    // Targets
    Serial.print(grid1);
    Serial.print(", ");
    Serial.print(grid2);
    Serial.print(", ");
    Serial.print(targetHT1);
    Serial.print(", ");
    Serial.print(targetHT2);
    Serial.print(", ");
    // Measured
    Serial.print(measuredHT1);
    Serial.print(", ");
    Serial.print(measuredHT2);
    Serial.print(", ");
    Serial.print(currentLo1);
    Serial.print(", ");
    Serial.print(currentMid1);
    Serial.print(", ");
    Serial.print(currentHi1);
    Serial.print(", ");
    Serial.print(currentLo2);
    Serial.print(", ");
    Serial.print(currentMid2);
    Serial.print(", ");
    Serial.println(currentHi2);
}

int ValveTester::measureValues()
{
    return 1;
}

int ValveTester::runTest()
{
    int status;

    applyGridVoltage(grid1, DAC1_ADDR);
    applyGridVoltage(grid2, DAC2_ADDR);

    status = chargeHT();
    if (status > 0)
    {
        status = measureValues();
    }

    return status;
}

int ValveTester::chargeHT()
{
    return 1;
}

int ValveTester::dischargeHT()
{
    return 1;
}

int ValveTester::applyGridVoltage(int value, int address)
{
    byte buf[3];

    if (value > 4095)
    {
        value = 4095;
    }

    Wire.beginTransmission(address);
    buf[0] = value >> 8;
    buf[1] = value & 255;
    Wire.write(buf, 2);
    if (Wire.endTransmission() != 0)
    { // If I2C tramission failed, return error
        return -ERR_I2C;
    }
    return 1;
}

void ValveTester::infoCommand(int index)
{
    int success = 1;

    switch (index)
    {
    case INFO_HW_VERSION:
        Serial.println("OK: Info(0) = Rev 5 (Mega Pro)");
        break;
    case INFO_SW_VERSION:
        Serial.println("OK: Info(1) = 2.0.0");
        break;
    default:
        success = -ERR_INVALID_INFO;
        break;
    }

    if (success < 0)
    {
        Serial.print("ERR: ");
        Serial.println(errorMessages[-success]);
    }
}

void ValveTester::modeCommand(int index)
{
    int success = 1;

    switch (index)
    {
    case MODE_SAFE:
        success = dischargeHT();
        if (success > 0)
        {
            success = applyGridVoltage(0, DAC1_ADDR);
            setGrid1(0);
            if (success > 0)
            {
                success = applyGridVoltage(0, DAC2_ADDR);
                setGrid2(0);
            }
        }
        if (success > 0)
        {
            Serial.println("OK: Mode(0)");
        }
        break;
    case MODE_RUN_TEST:
        success = runTest();
        if (success > 0)
        {
            Serial.print("OK: Mode(1) ");
            printValues();
        }
        break;
    case MODE_DISCHARGE:
        success = dischargeHT();
        if (success > 0)
        {
            Serial.println("OK: Mode(2)");
        }
        break;
    case MODE_CHARGE:
        success = chargeHT();
        if (success > 0)
        {
            Serial.println("OK: Mode(3) ");
        }
        break;
    default:
        success = -ERR_INVALID_MODE;
        break;
    }

    if (success < 0)
    {
        Serial.print("ERR: ");
        Serial.println(errorMessages[-success]);
    }
}

void ValveTester::getCommand(int index)
{
    int success = 1;

    switch (index)
    {
    case GET_ALL_VALUES:
        success = measureValues();
        if (success > 0)
        {
            Serial.print("OK: Get(0) ");
            printValues();
        }
        break;
    default:
        success = -ERR_INVALID_GET;
        break;
    }

    if (success < 0)
    {
        Serial.print("ERR: ");
        Serial.println(errorMessages[-success]);
    }
}

void ValveTester::setCommand(int index, int value)
{
    int success = 1;

    switch (index)
    {
    case SET_GRID1:
        if (value < 0 || value > 4095)
        {
            success = -ERR_GRID_RANGE;
        }
        else
        {
            grid1 = value;
            success = applyGridVoltage(grid1, DAC1_ADDR);
        }
        break;
    case SET_GRID2:
        if (value < 0 || value > 4095)
        {
            success = -ERR_GRID_RANGE;
        }
        else
        {
            grid2 = value;
            success = applyGridVoltage(grid2, DAC2_ADDR);
        }
        break;
    case SET_TARGET_HT1:
        if (value < 0 || value > 1023)
        {
            success = -ERR_HT_RANGE;
        }
        else
        {
            targetHT1 = value;
        }
        break;
    case SET_TARGET_HT2:
        if (value < 0 || value > 1023)
        {
            success = -ERR_HT_RANGE;
        }
        else
        {
            targetHT2 = value;
        }
        break;
    default:
        success = -ERR_INVALID_SET;
        break;
    }

    if (success > 0)
    {
        Serial.print("OK: Set(");
        Serial.print(index);
        Serial.print(") = ");
        Serial.println(value);
    }
    else
    {
        Serial.print("ERR: Set(");
        Serial.print(index);
        Serial.print(") = ");
        Serial.print(value);
        Serial.print(" - ");
        Serial.println(errorMessages[-success]);
    }
}

void ValveTester::commandError(const char *command)
{
    Serial.print("ERR: Unrecognised command - ");
    Serial.println(command);
}
