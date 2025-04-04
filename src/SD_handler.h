#include "FS.h"
#include "SD.h"
#include "SPI.h"

void createDir(fs::FS &fs, const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else
    {
        Serial.println("mkdir failed");
    }
}

void readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while (file.available())
    {
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message, bool newline = true)
{
    Serial.print("Appending to file: ");
    Serial.println(path);
    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        Serial.println("Message appended");
        if (newline)
        {
            file.println();
        }
    }
    else
    {
        Serial.println("Append failed");
    }
    file.close();
}

bool SDattached()
{
    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("No SD card attached");
        return false;
    }

    Serial.print("SD Card Type: ");
    switch (SD.cardType())
    {
    case CARD_MMC:
        Serial.println("MMC");
        break;
    case CARD_SD:
        Serial.println("SDSC");
        break;
    case CARD_SDHC:
        Serial.println("SDHC");
        break;
    default:
        Serial.println("UNKNOWN");
        break;
    }

    Serial.printf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
    return true;
}