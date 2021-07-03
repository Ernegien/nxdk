#include <hal/debug.h>
#include <hal/video.h>
#include <hal/eeprom.h>
#include <windows.h>

int main(void)
{
	XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

	EEPROM_DATA eeprom;

	debugPrint("Reading eeprom...\n");
	XEepromRead(&eeprom);

	debugPrint("Original Factory Checksum: %d\n", eeprom.factory_checksum);
	debugPrint("Original User Checksum: %d\n", eeprom.user_checksum);

	debugPrint("Performing eeprom modifications...\n");

	// clear parental control passcode
	//eeprom.parental_control_passcode = 0;

	// clear av settings
	//eeprom.video_settings = 0;
	//eeprom.audio_settings = 0;

	// clear padding
	//eeprom.padding46 = 0;
	//eeprom.padding5C = 0;

	debugPrint("Writing eeprom...\n");
	XEepromWrite(&eeprom);

	debugPrint("New Factory Checksum: %d\n", eeprom.factory_checksum);
	debugPrint("New User Checksum: %d\n", eeprom.user_checksum);
    
    debugPrint("Finished! Will reboot shortly...\n");
    Sleep(3000);

    return 0;
}
