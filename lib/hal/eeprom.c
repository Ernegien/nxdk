/*
  Copyright and related rights waived by Mike Davis via CC0
  https://creativecommons.org/publicdomain/zero/1.0/
*/

#include <hal/eeprom.h>
#include <xboxkrnl/xboxkrnl.h>

int XEepromRead(EEPROM_DATA* data)
{
    unsigned int type;
    unsigned long bytes_read;

    return ExQueryNonVolatileSetting(0xFFFF, &type, data, sizeof(EEPROM_DATA), &bytes_read);
}

int XEepromWrite(EEPROM_DATA* data)
{
    // update checksums
    XEepromCalculateChecksum(&data->factory_checksum, (unsigned char*)&data->serial, 0x2C);
    XEepromCalculateChecksum(&data->user_checksum, (unsigned char*)&data->time_zone_bias, 0x5C);

    return ExSaveNonVolatileSetting(0xFFFF, 0, data, sizeof(EEPROM_DATA));
}

void XEepromCalculateChecksum(unsigned int* outSum, unsigned char* data, unsigned int length)
{
    unsigned int high = 0, low = 0;

    for (unsigned int i = 0; i < length / sizeof(unsigned int); i++)
    {
        unsigned int val = ((unsigned int*)data)[i];
        unsigned long long sum = ((unsigned long long)high << 32) | low;

        high = (unsigned int)((sum + val) >> 32);
        low += val;
    }

    *outSum = ~(high + low);
}