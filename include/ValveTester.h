#ifndef VALVE_TESTER_H
#define VALVE_TESTER_H

#include "CommandParser.h"

#define MASTER_ADDR 0x0A // I2C Address of the Arduino Mega Pro
#define DAC1_ADDR 0x60   // I2C Address of one MCP4725, by default pin A0 is pulled to GND.
#define DAC2_ADDR 0x61   // I2C Address of other MCP4725; its pin A0 must be pulled HIGH to make address 0x61 (also remove pull-up resistors). Note that the breakout board uses the MCP4725A0.
// NB: I2C can send 32 ints per transmission.

class ValveTester
{
public:
    ValveTester();
    void parseInput(char c);

    enum SetIndex  { SET_GRID1 = 0, SET_GRID2, SET_TARGET_HT1, SET_TARGET_HT2, SET_TEST_MODE };
    enum GetIndex  { GET_ALL_VALUES = 0 };
    enum InfoIndex { INFO_HW_VERSION = 0, INFO_SW_VERSION };
    enum ModeIndex { MODE_SAFE = 0, MODE_RUN_TEST, MODE_DISCHARGE, MODE_CHARGE };
    enum TestModeIndex { TEST_MODE_OFF = 0, TEST_MODE_ECC83 = 1, TEST_MODE_EL84 = 2 };

    // Setters for target values
    void setGrid1(int value)     { grid1 = value; }
    void setGrid2(int value)     { grid2 = value; }
    void setTargetHT1(int value) { targetHT1 = value; }
    void setTargetHT2(int value) { targetHT2 = value; }
    void setTestMode(int value)  { testMode = value; }

    // Getters for target values
    int getGrid1() const    { return grid1; }
    int getGrid2() const    { return grid2; }
    int getTargetHT1() const { return targetHT1; }
    int getTargetHT2() const { return targetHT2; }
    int getTestMode() const  { return testMode; }

    // Getters for measured values
    int getMeasuredHT1() const      { return measuredHT1; }
    int getMeasuredHT2() const      { return measuredHT2; }
    int getCurrentLo1() const       { return currentLo1; }
    int getCurrentMid1() const      { return currentMid1; }
    int getCurrentHi1() const       { return currentHi1; }
    int getCurrentLo2() const       { return currentLo2; }
    int getCurrentMid2() const      { return currentMid2; }
    int getCurrentHi2() const       { return currentHi2; }

protected:
    enum ErrorCode {
        ERR_NO_ERROR = 0,
        ERR_INVALID_INFO,
        ERR_INVALID_MODE,
        ERR_INVALID_SET,
        ERR_INVALID_GET,
        ERR_I2C,
        ERR_GRID_RANGE,
        ERR_HT_RANGE,
        ERR_HT_TIMEOUT,
        ERR_UNSAFE,
        ERR_OFFSET_RANGE,
        ERR_COUNT  // must remain last
    };

    static const char *const errorMessages[ERR_COUNT];

    CommandParser<ValveTester> parser;

    // Target values
    int grid1;
    int grid2;
    int targetHT1;
    int targetHT2;
    int testMode;

    // Measured values
    int measuredHT1;
    int measuredHT2;
    int currentLo1;
    int currentMid1;
    int currentHi1;
    int currentLo2;
    int currentMid2;
    int currentHi2;

    virtual void infoCommand(int index);
    virtual void modeCommand(int index);
    virtual void getCommand(int index);
    virtual void setCommand(int index, int value);
    virtual void commandError(const char *command);

    // Column order: grid1, grid2, targetHT1, targetHT2,
    //               measuredHT1, measuredHT2,
    //               currentLo1, currentMid1, currentHi1,
    //               currentLo2, currentMid2, currentHi2
    void printValues();
    virtual int runTest();
    virtual int measureValues();
    virtual int chargeHT();
    virtual int dischargeHT();
    virtual int applyGridVoltage(int value, uint8_t address);
};

#endif // VALVE_TESTER_H
