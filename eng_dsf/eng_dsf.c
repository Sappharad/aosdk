//
// Audio Overload
// Emulated music player
//
// (C) 2000-2008 Richard F. Bannister
//

//
// eng_dsf.c
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ao.h"
#include "eng_protos.h"
#include "corlett.h"
#include "dc_hw.h"
#include "aica.h"

#define DK_CORE	(1)

#if DK_CORE
#include "arm7.h"
#else
#include "arm7core.h"
#endif

static corlett_t	c = {0};
int SND_SUBTRACK = 0;

int dsf_lib(int libnum, uint8 *lib, uint64 size, corlett_t *c)
{
	// patch the file into ram
	uint32 offset = lib[0] | lib[1]<<8 | lib[2]<<16 | lib[3]<<24;
	memcpy(&dc_ram[offset], lib+4, size-4);

	return AO_SUCCESS;
}

int32 dsf_start(uint8 *buffer, uint32 length)
{
	// clear Dreamcast work RAM before we start scribbling in it
	memset(dc_ram, 0, 8*1024*1024);

	// Decode the current SSF
	if (corlett_decode(buffer, length, &c, dsf_lib) != AO_SUCCESS)
	{
		return AO_FAIL;
	}

	#ifdef DEBUG
	{
		FILE *f;

		f = ao_fopen("dcram.bin", "wb");
		fwrite(dc_ram, 2*1024*1024, 1, f);
		fclose(f);
	}
	#endif

	#if DK_CORE
	ARM7_Init();
	#else
	arm7_init(0, 45000000, NULL, NULL);
	arm7_reset();
	#endif
	dc_hw_init();

	return AO_SUCCESS;
}

int32 am2snd_start(uint8 *buffer, uint32 length)
{
    // clear Dreamcast work RAM before we start scribbling in it
    memset(dc_ram, 0, 8*1024*1024);

    //Load AICADRV.BIN starting at 0
    FILE *drvFile = ao_fopen("AICADRV.BIN", "rb");
    if (!drvFile)
    {
        printf("ERROR: could not open file AICADRV.BIN\n");
        return AO_FAIL;
    }
    else{
        fseek(drvFile, 0, SEEK_END);
        int driverLen = ftell(drvFile);
        fseek(drvFile, 0, SEEK_SET);
        fread(dc_ram, driverLen, 1, drvFile);
        fclose(drvFile);
        
        //Copy SND file into memory
        memcpy(dc_ram+0x10000, buffer, length);
        
        //Copy DTPK ID from SND file
        dc_ram[0x60] = buffer[0x4];
        dc_ram[0x61] = buffer[0x5];
        dc_ram[0x62] = buffer[0x6];
        dc_ram[0x63] = buffer[0x7];
        
        //Bank 0 command
        dc_ram[0x400] = 0xA0;
        dc_ram[0x401] = 0x00;
        dc_ram[0x402] = 0x11;
        dc_ram[0x403] = 0x00;
        
        //Find Sequence offset
        int sequenceOffset = buffer[0x2C] + (buffer[0x2D] << 8) + (buffer[0x2E] << 16);
        if(sequenceOffset == 0){
            printf("SND file does not contain any tracks");
            return AO_FAIL;
        }
        int sequences = buffer[sequenceOffset]; //Should always be 0 for what we want.
        //It's a 32-bit number, but if there are > 10 sequences in a file that would be odd.
        if(sequences == 0 && buffer[sequenceOffset+8] != 0){
            sequences = buffer[sequenceOffset+8]; //Last subtrack index?
        }
        int trackToPlay = SND_SUBTRACK;
        if(trackToPlay > sequences){
            printf("The selected track number is greater than the number of tracks in this file");
            return AO_FAIL;
        }
        
        //Play sequence (track) command
        dc_ram[0x404] = buffer[sequenceOffset+4+3];
        dc_ram[0x405] = buffer[sequenceOffset+4+2];
        dc_ram[0x406] = trackToPlay; //Track
        dc_ram[0x407] = 0;
        
        //Set length to endless
        corlett_length_set(0,0);
    }
    
    #if DK_CORE
    ARM7_Init();
    #else
    arm7_init(0, 45000000, NULL, NULL);
    arm7_reset();
    #endif
    dc_hw_init();

    /*{
        FILE *f;

        f = ao_fopen("dcram.bin", "wb");
        fwrite(dc_ram, 2*1024*1024, 1, f);
        fclose(f);
    }*/
    
    return AO_SUCCESS;
}

int32 dsf_sample(stereo_sample_t *sample)
{
	#if DK_CORE
	ARM7_Execute((33000000 / 60 / 4) / 735);
	#else
	arm7_execute((33000000 / 60 / 4) / 735);
	#endif
	AICA_Update(NULL, NULL, sample);
	corlett_sample_fade(sample);

	return AO_SUCCESS;
}

int32 dsf_frame(void)
{
	return AO_SUCCESS;
}

int32 dsf_stop(void)
{
	return AO_SUCCESS;
}

int32 dsf_command(int32 command, int32 parameter)
{
	switch (command)
	{
		case COMMAND_RESTART:
			return AO_SUCCESS;

	}
	return AO_FAIL;
}

int32 dsf_fill_info(ao_display_info *info)
{
	info->title[1] = "Name: ";
	info->info[1] = corlett_tag_lookup(&c, "title");

	info->title[2] = "Game: ";
	info->info[2] = corlett_tag_lookup(&c, "game");

	info->title[3] = "Artist: ";
	info->info[3] = corlett_tag_lookup(&c, "artist");

	info->title[4] = "Copyright: ";
	info->info[4] = corlett_tag_lookup(&c, "copyright");

	info->title[5] = "Year: ";
	info->info[5] = corlett_tag_lookup(&c, "year");

	info->title[6] = "Length: ";
	info->info[6] = corlett_tag_lookup(&c, "length");

	info->title[7] = "Fade: ";
	info->info[7] = corlett_tag_lookup(&c, "fade");

	return AO_SUCCESS;
}

int32 am2snd_fill_info(ao_display_info *info)
{
    info->title[1] = "Name: ";
    info->info[1] = "Unknown";

    info->title[2] = "Game: ";
    info->info[2] = "Unknown";

    info->title[3] = "Artist: ";
    info->info[3] = "Unknown";

    info->title[4] = "Copyright: ";
    info->info[4] = "Unknown";

    info->title[5] = "Year: ";
    info->info[5] = "Unknown";

    info->title[6] = "Length: ";
    info->info[6] = "Unknown";

    info->title[7] = "Fade: ";
    info->info[7] = "Unknown";

    return AO_SUCCESS;
}
