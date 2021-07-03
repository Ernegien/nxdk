#ifndef HAL_EEPROM_H
#define HAL_EEPROM_H

#define EEPROM_WRITE_SMBUS_ADDR		0xA8
#define EEPROM_READ_SMBUS_ADDR		0xA9


typedef struct _EEPROM_DATA
{
	// security section (can't change these yet!)
	unsigned char hmac_sha1_hash[20];
	unsigned char confounder[8];	// RC4-encrypted at rest
	unsigned char hdd_key[16];		// RC4-encrypted at rest
	unsigned int region_flags;		// RC4-encrypted at rest TODO: enum

	// factory section
	unsigned int factory_checksum;	// factory section data from 0x34 to 0x60
	char serial[12];
	unsigned char mac_address[6];
	unsigned short padding46;
	unsigned char online_key[16];
	unsigned int video_standard_flags;	// TODO: enum
	unsigned int padding5C;

	// user section
	unsigned int user_checksum;		// user section data from 0x64 to 0xC0
	unsigned int time_zone_bias;
	char time_zone_standard_name[4];
	char time_zone_daylight_name[4];
	unsigned char padding70[8];
	unsigned int time_zone_standard_starts;
	unsigned int time_zone_daylight_starts;
	unsigned char padding80[8];
	unsigned int time_zone_standard_bias;
	unsigned int time_zone_daylight_bias;
	unsigned int language;					// TODO: enum
	unsigned int video_settings;			// TODO: enum
	unsigned int audio_settings;			// TODO: enum
	unsigned int parental_control_game;		// TODO: enum
	unsigned int parental_control_passcode;	// TODO: enum
	unsigned int parental_control_movie;	// TODO: enum
	unsigned int live_ip;
	unsigned int live_dns;
	unsigned int live_gateway;
	unsigned int live_subnet;
	unsigned int unknownB8;
	unsigned int dvd_zone;					// TODO: enum

	// history section?
	unsigned char history[64];
} EEPROM_DATA;

void XEepromCalculateChecksum(unsigned int* outSum, unsigned char* data, unsigned int length);

int XEepromRead(EEPROM_DATA* data);
int XEepromWrite(EEPROM_DATA* data);

#endif
