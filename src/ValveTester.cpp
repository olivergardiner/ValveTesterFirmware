#include "ValveTester.h"

#include "Arduino.h"
#include "Wire.h"

#include "hardware.h"

const char *const ValveTester::errorMessages[ValveTester::ERR_COUNT] = {
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
    "Offset voltage out of range",
};

ValveTester::ValveTester()
    : parser(this, &ValveTester::infoCommand, &ValveTester::modeCommand,
             &ValveTester::getCommand, &ValveTester::setCommand, &ValveTester::commandError),
      grid1(0), grid2(0), targetHT1(0), targetHT2(0),
      measuredHT1(0), measuredHT2(0),
      currentLo1(0), currentMid1(0), currentHi1(0),
      currentLo2(0), currentMid2(0), currentHi2(0)
{
    Wire.begin(MASTER_ADDR);
    pinMode(CHARGE1_PIN,    OUTPUT); digitalWrite(CHARGE1_PIN,    LOW);
    pinMode(CHARGE2_PIN,    OUTPUT); digitalWrite(CHARGE2_PIN,    LOW);
    pinMode(DISCHARGE1_PIN, OUTPUT); digitalWrite(DISCHARGE1_PIN, LOW);
    pinMode(DISCHARGE2_PIN, OUTPUT); digitalWrite(DISCHARGE2_PIN, LOW);
    pinMode(FIRE1_PIN,      OUTPUT); digitalWrite(FIRE1_PIN,      LOW);
    pinMode(FIRE2_PIN,      OUTPUT); digitalWrite(FIRE2_PIN,      LOW);
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
    measuredHT1 = analogRead(VA1_PIN);
    measuredHT2 = analogRead(VA2_PIN);
    currentLo1  = analogRead(IA1_LO_PIN);
    currentMid1 = analogRead(IA1_MID_PIN);
    currentLo2  = analogRead(IA2_LO_PIN);
    currentMid2 = analogRead(IA2_MID_PIN);
#if HARDWARE_TYPE == MEGA2560
    currentHi1 = analogRead(IA1_HI_PIN);
    currentHi2 = analogRead(IA2_HI_PIN);
#else
    currentHi1 = 0;
    currentHi2 = 0;
#endif

    return 1;
}

int ValveTester::runTest()
{
    int status;

    status = chargeHT();

    if (status > 0)
    {
        digitalWrite(FIRE1_PIN, HIGH);
        digitalWrite(FIRE2_PIN, HIGH);

        delayMicroseconds(1); // Allow some time for voltages and currents to stabilise before measuring
        status = measureValues();

        digitalWrite(FIRE1_PIN, LOW);
        digitalWrite(FIRE2_PIN, LOW);
    }

    return status;
}

// Voltage sense: 3×470kΩ top, 2×4.7kΩ bottom, Vref=4.096V, 10-bit ADC
// V_cap = count × (4.096/1024) × (1419400/9400) = count × 0.604 V/count
// Resistor power P = (dV_count × 0.604)² / 1000 W
// 2.5W limit → dV_count ≤ √(2500) / 0.604 ≈ 82 counts (≈50V) – full drive safe below this
// Software PWM: both channels pulsed simultaneously within one HT_PERIOD_US window.
// On-time t_on (µs) = P_limit × R × HT_PERIOD_US / V² = HT_ON_NUMERATOR / dV_count²
static const uint16_t      HT_PERIOD_US         = 2000;      // PWM period (µs); << τ = RC = 100ms
static const long          HT_ON_NUMERATOR      = 13706000L; // = 2.5 × 1000 × HT_PERIOD_US / 0.604²
static const int           HT_SAFE_DV_COUNTS    = 82;        // below this dV full drive is safe
static const int           HT_DISCHARGED_COUNTS = 5;         // ≈3V – considered safely discharged
static const unsigned long HT_TIMEOUT_MS        = 30000UL;   // 30 s max per charge/discharge

// Return on-time (µs) that keeps average resistor power ≤ 2.5 W over one PWM period
static uint16_t htOnTime(int dv)
{
    if (dv <= HT_SAFE_DV_COUNTS)
        return HT_PERIOD_US;
    long t = HT_ON_NUMERATOR / ((long)dv * dv);
    return (t >= HT_PERIOD_US) ? HT_PERIOD_US : (uint16_t)t;
}

// Pulse two pins simultaneously with independent on-times inside one PWM period.
// The pin with the shorter on-time is turned off first; both are LOW by the end of the period.
static void htPulse(uint8_t pin1, uint16_t on1, uint8_t pin2, uint16_t on2)
{
    if (on1) digitalWrite(pin1, HIGH);
    if (on2) digitalWrite(pin2, HIGH);

    uint16_t t_a = (on1 <= on2) ? on1 : on2;   // earlier turn-off time
    uint16_t t_b = (on1 <= on2) ? on2 : on1;   // later  turn-off time
    uint8_t  pa  = (on1 <= on2) ? pin1 : pin2;  // pin that turns off first
    uint8_t  pb  = (on1 <= on2) ? pin2 : pin1;  // pin that turns off second

    if (t_a)                 delayMicroseconds(t_a);
    digitalWrite(pa, LOW);
    if (t_b > t_a)           delayMicroseconds(t_b - t_a);
    digitalWrite(pb, LOW);
    if (HT_PERIOD_US > t_b)  delayMicroseconds(HT_PERIOD_US - t_b);
}

int ValveTester::chargeHT()
{
    unsigned long start = millis();
    bool done1 = (targetHT1 == 0);
    bool done2 = (targetHT2 == 0);

    digitalWrite(CHARGE1_PIN, LOW);
    digitalWrite(CHARGE2_PIN, LOW);

    while (!done1 || !done2)
    {
        if (millis() - start > HT_TIMEOUT_MS)
        {
            digitalWrite(CHARGE1_PIN, LOW);
            digitalWrite(CHARGE2_PIN, LOW);
            return -ERR_HT_TIMEOUT;
        }

        uint16_t on1 = 0, on2 = 0;

        if (!done1)
        {
            int m = analogRead(VA1_PIN);
            if (m >= targetHT1) done1 = true;
            else                on1 = htOnTime(targetHT1 - m);
        }

        if (!done2)
        {
            int m = analogRead(VA2_PIN);
            if (m >= targetHT2) done2 = true;
            else                on2 = htOnTime(targetHT2 - m);
        }

        if (on1 || on2)
            htPulse(CHARGE1_PIN, on1, CHARGE2_PIN, on2);
    }

    return 1;
}

int ValveTester::dischargeHT()
{
    unsigned long start = millis();

    digitalWrite(DISCHARGE1_PIN, LOW);
    digitalWrite(DISCHARGE2_PIN, LOW);

    for (;;)
    {
        if (millis() - start > HT_TIMEOUT_MS)
        {
            digitalWrite(DISCHARGE1_PIN, LOW);
            digitalWrite(DISCHARGE2_PIN, LOW);
            return -ERR_HT_TIMEOUT;
        }

        int  m1    = analogRead(VA1_PIN);
        int  m2    = analogRead(VA2_PIN);
        bool done1 = (m1 <= HT_DISCHARGED_COUNTS);
        bool done2 = (m2 <= HT_DISCHARGED_COUNTS);

        if (done1 && done2)
        {
            digitalWrite(DISCHARGE1_PIN, LOW);
            digitalWrite(DISCHARGE2_PIN, LOW);
            return 1;
        }

        uint16_t on1 = done1 ? 0 : htOnTime(m1);
        uint16_t on2 = done2 ? 0 : htOnTime(m2);

        htPulse(DISCHARGE1_PIN, on1, DISCHARGE2_PIN, on2);
    }
}

int ValveTester::applyGridVoltage(int value, uint8_t address)
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
