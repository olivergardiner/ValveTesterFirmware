#include "ValveTester.h"

#include "Arduino.h"
#include <math.h>
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
      grid1(0), grid2(0), targetHT1(0), targetHT2(0), testMode(TEST_MODE_OFF),
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

    pinMode(VA1_PIN,    INPUT);
    pinMode(VA2_PIN,    INPUT);
    pinMode(IA1_LO_PIN, INPUT);
    pinMode(IA1_MID_PIN,INPUT);
    pinMode(IA2_LO_PIN, INPUT);
    pinMode(IA2_MID_PIN,INPUT);
#if HARDWARE_TYPE == MEGA2560
    pinMode(IA1_HI_PIN, INPUT);
    pinMode(IA2_HI_PIN, INPUT);
#endif
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

// ─── Valve model helpers (equations ported from valvedesigner-web/public/designer.js) ────
// All currents in mA; voltages in V.

// Numerically stable log(1 + exp(y)); prevents float overflow for large y.
static float softplus(float y)
{
    return (y > 70.0f) ? y : logf(1.0f + expf(y));
}

// ── ECC83 "12AX7 (Cohen Helie - DS)" parameters ──────────────────────────────
static const float ECC83_KG1  = 0.899682f;
static const float ECC83_MU   = 100.0f;
static const float ECC83_X    = 1.52f;
static const float ECC83_KP   = 806.0f;
static const float ECC83_KVB  = 3.0f;
static const float ECC83_KVB1 = 12.5f;
static const float ECC83_VCT  = 0.505f;

// Cohen-Helie triode model. vg1 = actual (negative) grid voltage. Returns Ia in mA.
static float cohenHelieTriode_mA(float va, float vg1, float mu = ECC83_MU)
{
    float f  = sqrtf(ECC83_KVB + va * ECC83_KVB1 + va * va);
    float y  = ECC83_KP * (1.0f / mu + (vg1 + ECC83_VCT) / f);
    float ep = va / ECC83_KP * softplus(y);
    if (ep <= 0.0f) return 0.0f;
    // ia [A] = epk / (kg1 * 1000);  multiply by 1000 gives mA = epk / kg1
    return powf(ep, ECC83_X) / ECC83_KG1;
}

// ── EL84 "EL84 (Simple)" parameters ─────────────────────────────────────────
static const float EL84_A     = 0.00010564f;
static const float EL84_ALPHA = 0.007691f;
static const float EL84_BETA  = 0.037742f;
static const float EL84_GAMMA = 0.674046f;
static const float EL84_KG1   = 0.279261f;
static const float EL84_KG2   = 2.614896f;
static const float EL84_KP    = 145.4f;
static const float EL84_MU    = 19.0f;

// Simple pentode epk helper (shared by Ia and Ig2 calculations).
static float el84Epk(float vg2, float vg1)
{
    if (vg2 <= 0.0f) return 0.0f;
    float y  = EL84_KP * (1.0f / EL84_MU + vg1 / vg2);
    float ep = vg2 / EL84_KP * softplus(y);
    return (ep > 0.0f) ? powf(ep, 1.5f) : 0.0f;
}

// Simple pentode anode current (mA). vg1: control grid (negative); vg2: screen (positive).
static float el84Anode_mA(float va, float vg1, float vg2)
{
    float epk   = el84Epk(vg2, vg1);
    float k     = 1.0f / EL84_KG1 - 1.0f / EL84_KG2;
    float shift = EL84_BETA * (1.0f - EL84_ALPHA * vg1);
    float sv    = shift * va;
    float g     = (sv > 0.0f) ? expf(-powf(sv, EL84_GAMMA)) : 1.0f;
    float ia    = epk * (k * (1.0f - g) + EL84_A * va / EL84_KG1);
    return (ia > 0.0f) ? ia : 0.0f;
}

// Simple pentode screen current (mA).
static float el84Screen_mA(float va, float vg1, float vg2)
{
    float epk   = el84Epk(vg2, vg1);
    float shift = EL84_BETA * (1.0f - EL84_ALPHA * vg1);
    float sv    = shift * va;
    float g     = (sv > 0.0f) ? expf(-powf(sv, EL84_GAMMA)) : 1.0f;
    float psi   = EL84_KG2 / EL84_KG1 - 1.0f;
    float ig2   = epk * (1.0f + psi * g - EL84_A * va) / EL84_KG2;
    return (ig2 > 0.0f) ? ig2 : 0.0f;
}

// ── ADC conversion helpers ────────────────────────────────────────────────────
// Rsense_med = 100/3 Ω, gain_hi = 4, Rsense_lo = 10/3 Ω, Vref = 4.096 V, 10-bit ADC.
// Hi:  33.33 counts/mA   Mid:  8.33 counts/mA   Lo:  0.833 counts/mA
static int mAtoHiCount(float iaMa)
{
#if HARDWARE_TYPE == NANO
    (void)iaMa;
    return 1023;  // Nano has no hi-sensitivity current sense channel
#else
    int c = (int)(iaMa * (100.0f / 3.0f) + 0.5f);
    return (c < 1023) ? c : 1023;
#endif
}
static int mAtoMidCount(float iaMa)
{
    int c = (int)(iaMa * (25.0f / 3.0f) + 0.5f);
    return (c < 1023) ? c : 1023;
}
static int mAtoLoCount(float iaMa)
{
    int c = (int)(iaMa * (5.0f / 6.0f) + 0.5f);
    return (c < 1023) ? c : 1023;
}

// HT ADC count → voltage (V): Vref/1024 × (R_top + R_bot)/R_bot
// R_top = 3×470 kΩ = 1 410 000 Ω; R_bot = 2×4.7 kΩ = 9 400 Ω
static float htCountToVolts(int count)
{
    return (float)count * (4.096f / 1024.0f) * (1419400.0f / 9400.0f);
}

// DAC code → actual (negative) grid voltage: −(code / 4095) × Vref × gain
// Vref = 4.096 V, inverting gain magnitude = 16.5
static float dacToGridVolts(int code)
{
    return -(float)code * (4.096f * 16.5f / 4095.0f);
}

int ValveTester::measureValues()
{
    if (testMode == TEST_MODE_ECC83)
    {
        // ECC83 "12AX7 (Cohen Helie - DS)": ch1 and ch2 are independent triodes.
        // Inputs: targetHT1/HT2 = anode voltage; grid1/grid2 = control grid DAC code.
        float va1 = htCountToVolts(targetHT1);
        float vg1 = dacToGridVolts(grid1);
        float va2 = htCountToVolts(targetHT2);
        float vg2 = dacToGridVolts(grid2);
        measuredHT1 = targetHT1;  measuredHT2 = targetHT2;
        float ia1 = cohenHelieTriode_mA(va1, vg1);
        float ia2 = cohenHelieTriode_mA(va2, vg2, ECC83_MU - 2.0f);
        currentHi1 = mAtoHiCount(ia1);   currentMid1 = mAtoMidCount(ia1);  currentLo1 = mAtoLoCount(ia1);
        currentHi2 = mAtoHiCount(ia2);   currentMid2 = mAtoMidCount(ia2);  currentLo2 = mAtoLoCount(ia2);
        return 1;
    }
    if (testMode == TEST_MODE_EL84)
    {
        // EL84 "EL84 (Simple)": anode on ch1, screen grid current on ch2.
        // Inputs: targetHT1 = anode voltage; targetHT2 = screen voltage; grid1 = control grid DAC code.
        float va  = htCountToVolts(targetHT1);
        float vg2 = htCountToVolts(targetHT2);  // screen voltage (positive)
        float vg1 = dacToGridVolts(grid1);       // control grid voltage (negative)
        measuredHT1 = targetHT1;  measuredHT2 = targetHT2;
        float ia  = el84Anode_mA(va, vg1, vg2);
        float ig2 = el84Screen_mA(va, vg1, vg2);
        currentHi1 = mAtoHiCount(ia);    currentMid1 = mAtoMidCount(ia);   currentLo1 = mAtoLoCount(ia);
        currentHi2 = mAtoHiCount(ig2);   currentMid2 = mAtoMidCount(ig2);  currentLo2 = mAtoLoCount(ig2);
        return 1;
    }

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
    currentHi1 = 1023; // On Nano, IA1_HI and IA2_HI are not connected; return max count to indicate 'over-range'
    currentHi2 = 1023;
#endif

    return 1;
}

int ValveTester::runTest()
{
    int status;

    if (testMode != TEST_MODE_OFF)
    {
        // Test mode: simulate charge time then return simulated measurements.
        // Approximation: τ = RC = 100 ms; at full duty each 2 ms period closes ~2% of
        // remaining gap, giving a charge time roughly proportional to the target voltage.
        // delay_ms ≈ max(targetHT1, targetHT2) / 4  →  ~100 ms at 250 V (414 counts).
        int maxHT = (targetHT1 > targetHT2) ? targetHT1 : targetHT2;
        delay(maxHT / 4);
        return measureValues();
    }

    status = chargeHT();

    if (status > 0)
    {
        digitalWrite(FIRE1_PIN, HIGH);
        digitalWrite(FIRE2_PIN, HIGH);

        delayMicroseconds(10); // Allow some time for voltages and currents to stabilise before measuring
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
#if ORIGINAL_CHARGE_MODE
   // Charges channels alternately; no resistor power limiting.
    digitalWrite(CHARGE1_PIN, LOW);
    digitalWrite(CHARGE2_PIN, LOW);
    digitalWrite(DISCHARGE1_PIN, LOW);
    digitalWrite(DISCHARGE2_PIN, LOW);

    int m1 = analogRead(VA1_PIN);
    int m2 = analogRead(VA2_PIN);
    // TODO: consider changing != to >= (as in the PWM path) to handle overshoot — if the
    // capacitor charges past targetHT due to MOSFET turn-off latency, m1/m2 will exceed
    // the target on the first read after the inner while exits, making the != condition
    // immediately false. That is actually the desired behaviour, but if a subsequent read
    // returns below target (e.g. due to droop or ADC noise) the outer loop re-enters
    // unexpectedly. Using >= would make the termination condition consistent with the PWM
    // path and immune to that edge case.
    while (m1 != targetHT1 || m2 != targetHT2)
    {
        while (m1 < targetHT1)
        {
            digitalWrite(CHARGE1_PIN, HIGH);
            m1 = analogRead(VA1_PIN);
        }
        digitalWrite(CHARGE1_PIN, LOW);

        m2 = analogRead(VA2_PIN);
        while (m2 < targetHT2)
        {
            digitalWrite(CHARGE2_PIN, HIGH);
            m2 = analogRead(VA2_PIN);
        }
        digitalWrite(CHARGE2_PIN, LOW);

        m1 = analogRead(VA1_PIN);
    }
    return 1;
#else
    // PWM-based routine: limits average resistor power to 2.5 W.
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
#endif
}

int ValveTester::dischargeHT()
{
#if ORIGINAL_CHARGE_MODE
    // Turns discharge MOSFETs fully on and waits until current sense reads zero.
    // TODO: this is a potential infinite loop. ADC noise alone (±1–2 counts) or a tiny
    // residual leakage current through the 3.3 Ω LO sense resistor can keep the ADC
    // reading non-zero even when the capacitor is effectively discharged. Consider adding
    // a timeout (e.g. 30 s as in the PWM path) or replacing the current-zero condition
    // with a voltage-zero condition using VA1_PIN / VA2_PIN and a small threshold (like
    // HT_DISCHARGED_COUNTS) so the loop exits reliably.
    digitalWrite(FIRE1_PIN,      LOW);
    digitalWrite(FIRE2_PIN,      LOW);
    digitalWrite(CHARGE1_PIN,    LOW);
    digitalWrite(CHARGE2_PIN,    LOW);
    digitalWrite(DISCHARGE1_PIN, HIGH);
    digitalWrite(DISCHARGE2_PIN, HIGH);
    while (analogRead(IA1_LO_PIN)) {}
    while (analogRead(IA2_LO_PIN)) {}
    digitalWrite(DISCHARGE1_PIN, LOW);
    digitalWrite(DISCHARGE2_PIN, LOW);
    return 1;
#else
    // PWM-based routine: limits average resistor power to 2.5 W.
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
#endif
}

int ValveTester::applyGridVoltage(int value, uint8_t address)
{
    byte buf[3];

    if (value > 4095)
    {
        value = 4095;
    }

    Wire.beginTransmission(address);
    buf[0] = 0x40;             // MCP4725 "Write DAC Register" command
    buf[1] = value >> 4;       // D11–D4
    buf[2] = (value & 0x0F) << 4; // D3–D0 in upper nibble (lower nibble don't-care)
    Wire.write(buf, 3);
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
#if HARDWARE_TYPE == MEGA2560
        Serial.println("OK: Info(0) = Rev 5 (Mega Pro)");
#elif HARDWARE_TYPE == NANO
        Serial.println("OK: Info(0) = Rev 6 (Valve Wizard Nano MkII)");
#endif
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
        if (testMode != TEST_MODE_OFF)
        {
            Serial.println("OK: Mode(0)");
            break;
        }
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
        if (testMode != TEST_MODE_OFF)
        {
            Serial.println("OK: Mode(2)");
            break;
        }
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
            Serial.println("OK: Mode(3)");
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
    case SET_TEST_MODE:
        if (value < TEST_MODE_OFF || value > TEST_MODE_EL84)
        {
            success = -ERR_INVALID_SET;
        }
        else
        {
            testMode = value;
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
