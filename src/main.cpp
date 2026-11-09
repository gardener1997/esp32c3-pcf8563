#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

constexpr uint8_t SDA_PIN = 6;
constexpr uint8_t SCL_PIN = 7;
constexpr uint8_t RTC_ADDRESS = 0x51;

char inputBuffer[64];
size_t inputLength = 0;
bool isInputting = false;

uint8_t decToBcd(uint8_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

uint8_t bcdToDec(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

bool writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

// 计算星期：星期日 = 0，星期一 = 1
uint8_t calculateWeekday(int year, int month, int day)
{
    static const int offsets[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };

    if (month < 3)
    {
        year--;
    }

    return (year + year / 4 - year / 100 + year / 400
            + offsets[month - 1] + day) % 7;
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0)
        || year % 400 == 0;
}

bool isValidDateTime(
    int year, int month, int day,
    int hour, int minute, int second)
{
    if (year < 2000 || year > 2099)
        return false;

    if (month < 1 || month > 12)
        return false;

    if (hour < 0 || hour > 23
        || minute < 0 || minute > 59
        || second < 0 || second > 59)
        return false;

    const int daysInMonth[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    int maxDay = daysInMonth[month - 1];

    if (month == 2 && isLeapYear(year))
    {
        maxDay = 29;
    }

    return day >= 1 && day <= maxDay;
}

bool setTime(
    int year, int month, int day,
    int hour, int minute, int second)
{
    uint8_t weekday = calculateWeekday(year, month, day);

    // 停止 RTC 计时器
    if (!writeRegister(0x00, 0x20))
    {
        Serial.println("Error: Failed to stop RTC.");
        return false;
    }

    // 从秒寄存器 0x02 开始写入时间
    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(0x02);

    Wire.write(decToBcd(second));       // 秒，同时清除 VL
    Wire.write(decToBcd(minute));       // 分
    Wire.write(decToBcd(hour));         // 时
    Wire.write(decToBcd(day));          // 日
    Wire.write(weekday);                // 星期
    Wire.write(decToBcd(month));        // 月
    Wire.write(decToBcd(year - 2000));  // 年

    if (Wire.endTransmission() != 0)
    {
        Serial.println("Error: Failed to write time.");
        return false;
    }

    // 启动 RTC 计时器
    if (!writeRegister(0x00, 0x00))
    {
        Serial.println("Error: Failed to start RTC.");
        return false;
    }

    return true;
}

void readTime()
{
    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(0x02);

    if (Wire.endTransmission(false) != 0)
    {
        Serial.println("RTC communication error");
        return;
    }

    if (Wire.requestFrom(RTC_ADDRESS, (uint8_t)7) != 7)
    {
        Serial.println("RTC read error");
        return;
    }

    uint8_t seconds = Wire.read();
    uint8_t minutes = Wire.read();
    uint8_t hours = Wire.read();
    uint8_t days = Wire.read();
    uint8_t weekday = Wire.read();
    uint8_t months = Wire.read();
    uint8_t years = Wire.read();

    bool voltageLow = seconds & 0x80;

    seconds = bcdToDec(seconds & 0x7F);
    minutes = bcdToDec(minutes & 0x7F);
    hours = bcdToDec(hours & 0x3F);
    days = bcdToDec(days & 0x3F);
    weekday &= 0x07;
    months = bcdToDec(months & 0x1F);
    years = bcdToDec(years);

    const char* weekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };

    Serial.printf(
        "%04u-%02u-%02u %02u:%02u:%02u  %s",
        2000 + years, months, days,
        hours, minutes, seconds,
        weekday < 7 ? weekdays[weekday] : "Unknown"
    );

    if (voltageLow)
    {
        Serial.print("  [WARNING: VL=1]");
    }

    Serial.println();
}

void processCommand(char* command)
{
    int year, month, day;
    int hour, minute, second;
    int consumed = 0;

    // 解析命令
    if (sscanf(
        command,
        "set %4d-%2d-%2d %2d:%2d:%2d%n",
        &year, &month, &day,
        &hour, &minute, &second,
        &consumed
    ) != 6)
    {
        Serial.println("Invalid command.");
        Serial.println("Usage: set YYYY-MM-DD HH:MM:SS");
        return;
    }

    // 检查命令末尾是否只有空白字符
    for (size_t i = consumed; command[i] != '\0'; i++)
    {
        if (!isspace((unsigned char)command[i]))
        {
            Serial.println("Invalid command format.");
            return;
        }
    }

    // 检查日期和时间是否合法
    if (!isValidDateTime(
        year, month, day,
        hour, minute, second))
    {
        Serial.println("Invalid date or time.");
        return;
    }

    if (setTime(
        year, month, day,
        hour, minute, second))
    {
        Serial.println("RTC time set successfully!");
        readTime();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Wire.begin(SDA_PIN, SCL_PIN);

    Serial.println();
    Serial.println("PCF8563 RTC");
    Serial.println("--------------------------------");
    Serial.println("Commands:");
    Serial.println("set YYYY-MM-DD HH:MM:SS");
    Serial.println("Example: set 2026-11-09 20:45:00");
    Serial.println("--------------------------------");
}

void loop()
{
    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            Serial.println();

            inputBuffer[inputLength] = '\0';

            if (inputLength > 0)
            {
                processCommand(inputBuffer);
                inputLength = 0;
            }

            isInputting = false;
        }
        else
        {
            isInputting = true;
            Serial.write(c);

            if (inputLength < sizeof(inputBuffer) - 1)
            {
                inputBuffer[inputLength++] = c;
            }
            else
            {
                inputLength = 0;
                isInputting = false;
                Serial.println();
                Serial.println("Error: Command too long.");
            }
        }
    }

    static unsigned long lastReadTime = 0;

    if (!isInputting && millis() - lastReadTime >= 1000)
    {
        lastReadTime = millis();
        readTime();
    }
}
