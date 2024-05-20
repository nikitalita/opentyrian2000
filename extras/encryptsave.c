#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <memory.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef SAVE_FILES_SIZE
#define SAVE_FILES_SIZE 2398
#endif

#ifndef SIZEOF_SAVEGAMETEMP
#define SIZEOF_SAVEGAMETEMP SAVE_FILES_SIZE + 4 + 100
#endif

#ifndef SAVE_FILE_SIZE
#define SAVE_FILE_SIZE (SIZEOF_SAVEGAMETEMP - 4)
#endif

#ifndef SAVE_FILES_NUM
#define SAVE_FILES_NUM (11 * 2)
#endif /*  SAVE_FILES_NUM */

typedef uint8_t  JE_byte;
typedef uint16_t JE_word;
typedef int32_t JE_longint;
typedef int16_t JE_integer;
typedef int8_t  JE_shortint;
typedef bool   JE_boolean;
typedef char   JE_char;
typedef float  JE_real;

typedef JE_byte JE_PItemsType[12]; /* [1..12] */
typedef JE_byte JE_SaveGameTemp[SAVE_FILES_SIZE + 4 + 100];

typedef struct
{
	JE_word       encode;
	JE_word       level;
	JE_PItemsType items;
	JE_longint    score;
	JE_longint    score2;
	char          levelName[11];
	JE_char       name[15];
	JE_byte       cubes;
	JE_byte       power[2];
	JE_byte       episode;
	JE_PItemsType lastItems;
	JE_byte       difficulty;
	JE_byte       secretHint;
	JE_byte       input1;
	JE_byte       input2;
	JE_boolean    gameHasRepeated;
	JE_byte       initialDifficulty;

	JE_longint    highScore1;
	JE_longint    highScore2;
	char          highScoreName[30];
	JE_byte       highScoreDiff;
} JE_SaveFileType;

typedef JE_SaveFileType JE_SaveFilesType[SAVE_FILES_NUM]; /* [1..savefilesnum] */
JE_SaveGameTemp saveTemp;
JE_SaveFilesType saveFiles; /*array[1..saveLevelnum] of savefiletype;*/

void Usage(char *name)
{
	printf("%s <decrypted.sav> <tyrian.sav>\n", name);
	printf("Copyright (C) 2024 AnV Software\n");
}

const JE_byte cryptKey[10] = /* [1..10] */
{
	15, 50, 89, 240, 147, 34, 86, 9, 32, 208
};

void JE_encryptSaveTemp(void)
{
	JE_SaveGameTemp s3;
	JE_word x;
	JE_byte y;

	memcpy(&s3, &saveTemp, sizeof(s3));

	y = 0;
	for (x = 0; x < SAVE_FILE_SIZE; x++)
	{
		y += s3[x];
	}
	saveTemp[SAVE_FILE_SIZE] = y;

	y = 0;
	for (x = 0; x < SAVE_FILE_SIZE; x++)
	{
		y -= s3[x];
	}
	saveTemp[SAVE_FILE_SIZE+1] = y;

	y = 1;
	for (x = 0; x < SAVE_FILE_SIZE; x++)
	{
		y = (y * s3[x]) + 1;
	}
	saveTemp[SAVE_FILE_SIZE+2] = y;

	y = 0;
	for (x = 0; x < SAVE_FILE_SIZE; x++)
	{
		y = y ^ s3[x];
	}
	saveTemp[SAVE_FILE_SIZE+3] = y;

	for (x = 0; x < SAVE_FILE_SIZE; x++)
	{
		saveTemp[x] = saveTemp[x] ^ cryptKey[(x+1) % 10];
		if (x > 0)
		{
			saveTemp[x] = saveTemp[x] ^ saveTemp[x - 1];
		}
	}
}

int main(int argc, char **argv)
{
	FILE *f = NULL;
	size_t fsz = 0;
	int x = 0;
	JE_byte y = 0;

	if (argc != 3)
	{
		Usage(argv[0]);
	}

#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	fopen_s(&f, argv[1], "rb");
#else
	f = fopen(argv[1], "rb");
#endif

	if (f == NULL)
	{
		printf("Couldn't open %s for reading\n", argv[1]);
		return -1;
	}

	fseek(f, 0, SEEK_END);
	fsz = ftell(f);
	fseek(f, 0, SEEK_SET);

	fread(saveTemp, 1, sizeof(saveTemp), f);
	fclose(f);

	JE_encryptSaveTemp();

	f = fopen(argv[2], "wb");

	if (f == NULL)
	{
		printf("Couldn't open %s for writing\n", argv[2]);
		return -1;
	}

	fwrite(saveTemp, 1, sizeof(saveFiles), f);
	fclose(f);

	return 0;
}

