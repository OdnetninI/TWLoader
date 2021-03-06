/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <nds/fifocommon.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>
#include <nds/disc_io.h>

#include <string.h>
#include <unistd.h>

#include "nds_loader_arm9.h"

#include "inifile.h"
#include "log.h"

using namespace std;

bool logEnabled = false;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

//---------------------------------------------------------------------------------
void doPause() {
//---------------------------------------------------------------------------------
	iprintf("Press start...\n");
	while(1) {
		scanKeys();
		if(keysDown() & KEY_START)
			break;
		swiWaitForVBlank();
	}
	scanKeys();
}

void runFile(string filename) {
	vector<char*> argarray;
	
	if ( strcasecmp (filename.c_str() + filename.size() - 5, ".argv") == 0) {
		FILE *argfile = fopen(filename.c_str(),"rb");
		char str[PATH_MAX], *pstr;
		const char seps[]= "\n\r\t ";

		while( fgets(str, PATH_MAX, argfile) ) {
			// Find comment and end string there
			if( (pstr = strchr(str, '#')) )
				*pstr= '\0';

			// Tokenize arguments
			pstr= strtok(str, seps);

			while( pstr != NULL ) {
				argarray.push_back(strdup(pstr));
				pstr= strtok(NULL, seps);
			}
		}
		fclose(argfile);
		filename = argarray.at(0);
	} else {
		argarray.push_back(strdup(filename.c_str()));
	}

	if ( strcasecmp (filename.c_str() + filename.size() - 4, ".nds") != 0 || argarray.size() == 0 ) {
		iprintf("no nds file specified\n");
	} else {
		iprintf("Running %s with %d parameters\n", argarray[0], argarray.size());
		int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
		iprintf("Start failed. Error %i\n", err);
		// doPause();
	}
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	// REG_SCFG_CLK = 0x80;
	REG_SCFG_CLK = 0x85;

	bool TWLCLK = true;
	bool TWLVRAM = false;
	bool TriggerExit = false;
	std::string	gamesettingsPath = "";

	bool consoleOn = false;

	/* Log file is dissabled by default. If _nds/twloader/log exist, we turn log file on, else, log is dissabled */
	struct stat logBuf;
	logEnabled = stat("sd:/_nds/twloader/log", &logBuf) == 0;
	/* Log configuration file end */
	
	/* scanKeys();
	int pressed = keysDown(); */

	if (fatInitDefault()) {
		CIniFile twloaderini( "sd:/_nds/twloader/settings.ini" );
		CIniFile bootstrapini( "sd:/_nds/nds-bootstrap.ini" );
		
		// Didn't seem to work, aside from the language of the H&S screen changing.
		// if(twloaderini.GetInt("TWL-MODE","USE_SYSLANG",0) == 0) {
		// 	PersonalData->language = twloaderini.GetInt("TWL-MODE", "LANGUAGE", 0);
		// }
		
		char *p = (char*)PersonalData->name;
		
		// text
		for (int i = 0; i < 10; i++) {
			if (p[i*2] == 0x00)
				p[i*2/2] = 0;
			else
				p[i*2/2] = p[i*2];
		}
		
		if (logEnabled)	LogFMA("TWL.Main", "Got username", p);
		
		twloaderini.SetString("FRONTEND","NAME", p);
		twloaderini.SaveIniFile( "sd:/_nds/twloader/settings.ini" );
		if (logEnabled)	LogFMA("TWL.Main", "Saved username to GUI", p);
		
		gamesettingsPath = twloaderini.GetString( "TWL-MODE", "GAMESETTINGS_PATH", "");
		
		if(!access(gamesettingsPath.c_str(), F_OK)) {
			CIniFile gamesettingsini( gamesettingsPath );
			if(gamesettingsini.GetInt("GAME-SETTINGS","TWL_CLOCK",0) == -1) {
				if(twloaderini.GetInt("TWL-MODE","TWL_CLOCK",0) == 0) { TWLCLK = false; }
			} else {
				if(gamesettingsini.GetInt("GAME-SETTINGS","TWL_CLOCK",0) == 0) {
					TWLCLK = false;
					bootstrapini.SetInt("NDS-BOOTSTRAP","BOOST_CPU", 0);
				} else {
					bootstrapini.SetInt("NDS-BOOTSTRAP","BOOST_CPU", 1);
				}
			}
			if(gamesettingsini.GetInt("GAME-SETTINGS","TWL_VRAM",0) == -1) {
				if(twloaderini.GetInt("TWL-MODE","TWL_VRAM",0) == 1) { TWLVRAM = true; }
			} else {
				if(gamesettingsini.GetInt("GAME-SETTINGS","TWL_VRAM",0) == 1) { TWLVRAM = true; }
			}
			if(gamesettingsini.GetInt("GAME-SETTINGS","USE_ARM7_DONOR",0) == 1) {
				bootstrapini.SetInt("NDS-BOOTSTRAP","USE_ARM7_DONOR", 1);
			} else {
				bootstrapini.SetInt("NDS-BOOTSTRAP","USE_ARM7_DONOR", 0);
			}
			bootstrapini.SaveIniFile( "sd:/_nds/nds-bootstrap.ini" );
		} else {
			if(twloaderini.GetInt("TWL-MODE","TWL_CLOCK",0) == 0) { TWLCLK = false; }
			if(twloaderini.GetInt("TWL-MODE","TWL_VRAM",0) == 1) { TWLVRAM = true; }
			bootstrapini.SetInt("NDS-BOOTSTRAP","USE_ARM7_DONOR", 1);
			bootstrapini.SaveIniFile( "sd:/_nds/nds-bootstrap.ini" );
		}
		
		if(twloaderini.GetInt("TWL-MODE","DEBUG",0) != -1) {
			consoleDemoInit();
			consoleOn = true;
		}
		if(TWLCLK) {
			if (logEnabled)	LogFM("TWL.Main", "ARM9 CPU Speed set to 133mhz(TWL)");
			if(twloaderini.GetInt("TWL-MODE","DEBUG",0) == 1) {
				printf("TWL_CLOCK ON\n");		
			}
		} else {
			REG_SCFG_CLK = 0x80;
			fifoSendValue32(FIFO_USER_04, 1);
			if (logEnabled)	LogFM("TWL.Main", "ARM9 CPU Speed set to 67mhz(NTR)");
		}

		if(twloaderini.GetInt("TWL-MODE","LAUNCH_SLOT1",0) == 1) {
			if(REG_SCFG_MC == 0x11) { 
				if (consoleOn == false) {
					consoleDemoInit(); }
				printf("Please insert a cartridge...\n");
				do { swiWaitForVBlank(); } 
				while (REG_SCFG_MC == 0x11);
			}
		}
		
		if(twloaderini.GetInt("TWL-MODE","LAUNCH_SLOT1",0) == 1) {
			REG_SCFG_EXT = 0x83000000; // NAND/SD Access
			if(twloaderini.GetInt("TWL-MODE","FORWARDER",0) == 1) {
				if(twloaderini.GetInt("TWL-MODE","FLASHCARD",0) == 0) {
					fifoSendValue32(FIFO_USER_05, 1);
					if(twloaderini.GetInt("TWL-MODE","DEBUG",0) == 1)
						printf("ARM7 REG_SCFG_ROM = 0x703\n");
				}
			}
			if(twloaderini.GetInt("TWL-MODE","DEBUG",0) == 1)
				printf("Switched to NTR mode\n");		
		}
		
		if(TWLVRAM) {
			REG_SCFG_EXT |= 0x2000;
			if (logEnabled)	LogFM("TWL.Main", "VRAM boost on");
			if(twloaderini.GetInt("TWL-MODE","DEBUG",0) == 1) {
				printf("TWL_VRAM ON\n");		
			}
		}
		
		// Tell Arm7 to apply changes.
		fifoSendValue32(FIFO_USER_07, 1);

		if(twloaderini.GetInt("TWL-MODE","DEBUG",0) == 1)
			doPause();

		for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }
				
		if(twloaderini.GetInt("TWL-MODE","LAUNCH_SLOT1",0) == 1) {
			if(twloaderini.GetInt("TWL-MODE","DEBUG",0) != -1) {
				printf("Now booting Slot-1 card\n");					
				if (logEnabled)	LogFM("TWL.Main", "Now booting Slot-1 card");
			}
		}
		if(twloaderini.GetInt("TWL-MODE","FORWARDER",0) == 1) {
			if(twloaderini.GetInt("TWL-MODE","FLASHCARD",0) == 0) {
				runFile("sd:/_nds/loadcard_dstt.nds");
			} else if(twloaderini.GetInt("TWL-MODE","FLASHCARD",0) == 1) {
				runFile("sd:/_nds/twloader/loadflashcard/r4.nds");
			} else if(twloaderini.GetInt("TWL-MODE","FLASHCARD",0) == 4) {
				runFile("sd:/_nds/twloader/loadflashcard/ace_rpg.nds");
			}
		}
		
		for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }

		if(twloaderini.GetInt("TWL-MODE","LAUNCH_SLOT1",0) == 0) {
			if (logEnabled)	LogFM("TWL.Main", "Now booting bootstrap");
			if(twloaderini.GetInt("TWL-MODE","DEBUG",0) != -1) {
				printf("Now booting bootstrap\n");					
			}
		}
	}

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	std::string filename;

	if (!fatInitDefault()) {
		// Subscreen as a console
		videoSetModeSub(MODE_0_2D);
		vramSetBankH(VRAM_H_SUB_BG);
		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	

		iprintf ("fatinitDefault failed!\n");		
		iprintf ("\n");		
		iprintf ("Press B to return to\n");		
		iprintf ("HOME Menu.\n");		
			
		while (1) {
			scanKeys();
			if (keysHeld() & KEY_B) fifoSendValue32(FIFO_USER_06, 1);	// Tell ARM7 to reboot into 3DS HOME Menu (power-off/sleep mode screen skipped)
		}
	}

	vector<string> extensionList;
	extensionList.push_back(".nds");
	extensionList.push_back(".argv");

	while(1) {

		if(TriggerExit) { 
		do { swiWaitForVBlank(); scanKeys(); } while (!keysDown());
		break;
		}
		
		CIniFile bootstrapini( "sd:/_nds/nds-bootstrap.ini" );
		filename = bootstrapini.GetString("NDS-BOOTSTRAP", "BOOTSTRAP_PATH","");
		// filename = ReplaceAll( filename, "fat:/", "sd:/");
		runFile(filename);
		
		// Subscreen as a console
		videoSetModeSub(MODE_0_2D);
		vramSetBankH(VRAM_H_SUB_BG);
		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);	
		
		iprintf ("bootstrap not found.\n");		
		iprintf ("\n");		
		iprintf ("Press B to return to\n");		
		iprintf ("HOME Menu.\n");

		while (1) {
			scanKeys();
			if (keysHeld() & KEY_B) fifoSendValue32(FIFO_USER_06, 1);	// Tell ARM7 to reboot into 3DS HOME Menu (power-off/sleep mode screen skipped)
		}
	}

	return 0;
}
