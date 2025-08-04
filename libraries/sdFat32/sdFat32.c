/*
 * MIT License
 *
 * Copyright (c) 2025 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sdFat32.h"
#include "usb_serial.h"

#if SDFAT32_SD_BACKEND == SDFAT32_SD_BACKEND_SDMMC
#include "sdmmc.h"
#elif SDFAT32_SD_BACKEND == SDFAT32_SD_BACKEND_SPI
#include "sd_spi.h"
#else
#error "Invalid SDFAT32_SD_BACKEND selection"
#endif

#define MBR_SIGNATURE_OFFSET 510
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_ENTRY_SIZE 16

#define PRINTF(...) printf(__VA_ARGS__)

#if SDFAT32_SD_BACKEND == SDFAT32_SD_BACKEND_SDMMC
#define SDFAT32_SD_INIT() (sdmmc_init() == SDMMC_OK)
#define SDFAT32_SD_READ_SECTOR(lba, buf) (sdmmc_read_sector((lba), (buf)) == SDMMC_OK)
#define SDFAT32_SD_WRITE_SECTOR(lba, buf) (sdmmc_write_sector((lba), (buf)) == SDMMC_OK)
#define SDFAT32_SD_SECTOR_SIZE() (512U)
#elif SDFAT32_SD_BACKEND == SDFAT32_SD_BACKEND_SPI
#define SDFAT32_SD_INIT() (sd_init() == SD_INIT_SUCCESS)
#define SDFAT32_SD_READ_SECTOR(lba, buf) (sd_read_sector((lba), (buf)) == SD_READ_SUCCESS)
#define SDFAT32_SD_WRITE_SECTOR(lba, buf) (sd_write_sector((lba), (uint8_t *)(buf)) == SD_WRITE_SUCCESS)
#define SDFAT32_SD_SECTOR_SIZE() (512U)
#endif

static uint32_t bootSecStartLba;
static uint32_t fsInfoSecLba;
static bootSecParams_t params;
static uint8_t sdBuffer[512] __attribute__((aligned(4)));

static uint32_t FatStartSector;
static uint32_t FatSectorsCnt;

static uint32_t RootDirStartSector;
static uint32_t RootDirSectors;

static uint32_t DataStartSector;
static uint32_t DataSectorsCnt;

static char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static uint8_t fileNameLength(const char *filename);
static bool mixedLetters(const char *filename);
char *fileGetExtension(const char *file_name);

static uint16_t readLe16(const void *ptr)
{
	const uint8_t *p = (const uint8_t *)ptr;
	return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t *p)
{
	return ((uint32_t)p[0]) |
		   ((uint32_t)p[1] << 8) |
		   ((uint32_t)p[2] << 16) |
		   ((uint32_t)p[3] << 24);
}

static void writeLe16(void *ptr, uint16_t value)
{
	uint8_t *p = (uint8_t *)ptr;
	p[0] = (uint8_t)(value & 0xFF);
	p[1] = (uint8_t)(value >> 8);
}

static bool utf8DecodeNext(const char **src, uint32_t *codepoint)
{
	const uint8_t *s = (const uint8_t *)*src;

	if (s[0] == 0)
	{
		return false;
	}

	if (s[0] < 0x80)
	{
		*codepoint = s[0];
		*src += 1;
		return true;
	}

	if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80)
	{
		uint32_t cp = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
		if (cp >= 0x80)
		{
			*codepoint = cp;
			*src += 2;
			return true;
		}
	}

	if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)
	{
		uint32_t cp = ((uint32_t)(s[0] & 0x0F) << 12) |
					  ((uint32_t)(s[1] & 0x3F) << 6) |
					  (uint32_t)(s[2] & 0x3F);
		if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF))
		{
			*codepoint = cp;
			*src += 3;
			return true;
		}
	}

	if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80)
	{
		uint32_t cp = ((uint32_t)(s[0] & 0x07) << 18) |
					  ((uint32_t)(s[1] & 0x3F) << 12) |
					  ((uint32_t)(s[2] & 0x3F) << 6) |
					  (uint32_t)(s[3] & 0x3F);
		if (cp >= 0x10000 && cp <= 0x10FFFF)
		{
			*codepoint = cp;
			*src += 4;
			return true;
		}
	}

	*codepoint = '?';
	*src += 1;
	return true;
}

static size_t utf8Encode(char *dst, size_t dstSize, uint32_t codepoint)
{
	if (dstSize == 0)
	{
		return 0;
	}

	if (codepoint <= 0x7F)
	{
		if (dstSize < 1)
		{
			return 0;
		}
		dst[0] = (char)codepoint;
		return 1;
	}

	if (codepoint <= 0x7FF)
	{
		if (dstSize < 2)
		{
			return 0;
		}
		dst[0] = (char)(0xC0 | (codepoint >> 6));
		dst[1] = (char)(0x80 | (codepoint & 0x3F));
		return 2;
	}

	if (codepoint <= 0xFFFF)
	{
		if (dstSize < 3)
		{
			return 0;
		}
		dst[0] = (char)(0xE0 | (codepoint >> 12));
		dst[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
		dst[2] = (char)(0x80 | (codepoint & 0x3F));
		return 3;
	}

	if (dstSize < 4)
	{
		return 0;
	}
	dst[0] = (char)(0xF0 | (codepoint >> 18));
	dst[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
	dst[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
	dst[3] = (char)(0x80 | (codepoint & 0x3F));
	return 4;
}

static uint8_t *lfnEntryNameSlot(LFN_entry_t *entry, uint8_t index)
{
	if (index < 5)
	{
		return (uint8_t *)&entry->LDIR_Name1[index * 2];
	}
	if (index < 11)
	{
		return (uint8_t *)&entry->LDIR_Name2[(index - 5) * 2];
	}
	return (uint8_t *)&entry->LDIR_Name3[(index - 11) * 2];
}

static uint16_t lfnReadNameSlot(LFN_entry_t *entry, uint8_t index)
{
	return readLe16(lfnEntryNameSlot(entry, index));
}

static void lfnWriteNameSlot(LFN_entry_t *entry, uint8_t index, uint16_t value)
{
	writeLe16(lfnEntryNameSlot(entry, index), value);
}

static uint16_t utf8ToUtf16(const char *src, uint16_t *dst, uint16_t dstUnits)
{
	uint16_t units = 0;
	uint32_t cp;

	while (utf8DecodeNext(&src, &cp) && units < dstUnits)
	{
		if (cp > 0xFFFF)
		{
			if ((uint16_t)(units + 1) >= dstUnits)
			{
				break;
			}
			cp -= 0x10000;
			dst[units++] = (uint16_t)(0xD800 | (cp >> 10));
			dst[units++] = (uint16_t)(0xDC00 | (cp & 0x3FF));
		}
		else
		{
			dst[units++] = (uint16_t)cp;
		}
	}

	return units;
}

static bool filenameNeedsLfn(const char *filename)
{
	for (const uint8_t *p = (const uint8_t *)filename; *p != 0; p++)
	{
		if (*p >= 0x80)
		{
			return true;
		}
	}

	return mixedLetters(filename) || (fileNameLength(filename) > 8) || (strlen(fileGetExtension(filename)) > 3);
}


bool mbrGetPartitionStartLba(uint8_t *mbr, int partIndex, uint32_t *startLba)
{
	if (mbr == 0 || startLba == 0)
	{
		return false;
	}

	if (partIndex < 0 || partIndex > 3)
	{
		return false;
	}

	/* Check MBR signature 0x55AA */
	if (mbr[MBR_SIGNATURE_OFFSET] != 0x55 ||
		mbr[MBR_SIGNATURE_OFFSET + 1] != 0xAA)
	{
		return false;
	}

	const uint8_t *entry = &mbr[PARTITION_TABLE_OFFSET + (partIndex * PARTITION_ENTRY_SIZE)];

	/* Optional: check partition type is non-zero */
	if (entry[4] == 0x00)
	{
		return false; /* empty partition entry */
	}

	*startLba = readLe32(&entry[8]);
	return true;
}

/**
 * @brief Reads boot sector parameters from SD card and stores them in params global variable
 *
 * @return true if success, false otherwise
 */
static bool getBootSecParams(void)
{
	if (SDFAT32_SD_READ_SECTOR(bootSecStartLba, sdBuffer))
	{
		params.BPB_BytesPerSec = sdBuffer[11] | (sdBuffer[12] << 8);
		params.BPB_SecPerClus = sdBuffer[13];
		params.BPB_RsvdSecCnt = sdBuffer[14] | (sdBuffer[15] << 8);
		params.BPB_TotSec32 = sdBuffer[32] | (sdBuffer[33] << 8) | (sdBuffer[34] << 16) | (sdBuffer[35] << 24);
		params.BPB_FATSz32 = sdBuffer[36] | (sdBuffer[37] << 8) | (sdBuffer[38] << 16) | (sdBuffer[39] << 24);
		params.BPB_RootEntCnt = sdBuffer[17] | (sdBuffer[18] << 8);
		params.BPB_NumFATs = sdBuffer[16];
		params.BPB_RootClus = sdBuffer[44] | (sdBuffer[45] << 8) | (sdBuffer[46] << 16) | (sdBuffer[47] << 24);
		params.BPB_FSInfo = sdBuffer[48] | (sdBuffer[49] << 8);
		memcpy(params.BS_VolLab, &sdBuffer[71], 11);
		params.BS_VolLab[11] = '\0';
		return true;
	}
	return false;
}

/**
 * @brief  get FAT entry location strcuture(Sector number and offset)
 *
 * @param[in] fat_entry_index  entry index of FAT table
 *
 */
static fatEntLoc_t fatEntLocation(uint32_t fat_entry_index)
{
	fatEntLoc_t fatEntLoc;
	fatEntLoc.fatSecNum = FatStartSector + (fat_entry_index * 4 / params.BPB_BytesPerSec);
	fatEntLoc.fatEntOffset = (fat_entry_index * 4) % params.BPB_BytesPerSec;
	return fatEntLoc;
}

/**
 * @brief  Function to get the next cluster
 *
 * @param[in] fatThisClus Current cluster index
 * @return next cluster
 */
static uint32_t fatNextCluster(uint32_t fatThisClus)
{
	uint32_t temp;
	fatEntLoc_t fatEntLoc = fatEntLocation(fatThisClus);
	SDFAT32_SD_READ_SECTOR(fatEntLoc.fatSecNum, sdBuffer);
	temp = *((uint32_t *)&sdBuffer[fatEntLoc.fatEntOffset]);

	return temp;
}

/**
 * @brief Set the next cluster for a given cluster index
 *
 * @param[in] fatThisClus Current cluster index
 * @param[in] fatNextCluster Next cluster index
 *
 * @details Reads the sector containing the FAT entry for the given cluster index,
 *          updates the FAT entry with the given next cluster index, and writes the
 *          updated sector back to the SD card.
 */
static void fatSetNextClus(uint32_t fatThisClus, uint32_t fatNextCluster)
{
	uint32_t *p_temp;										  // pointer to FAT entry
	fatEntLoc_t fatEntLoc = fatEntLocation(fatThisClus);	  // get sector and offset of FAT entry
	SDFAT32_SD_READ_SECTOR(fatEntLoc.fatSecNum, sdBuffer);		  // read sector containing FAT entry
	p_temp = ((uint32_t *)&sdBuffer[fatEntLoc.fatEntOffset]); // get pointer to FAT entry
	memcpy(p_temp, &fatNextCluster, 4);						  // update FAT entry with next cluster index
	SDFAT32_SD_WRITE_SECTOR(fatEntLoc.fatSecNum, sdBuffer);		  // write updated sector back to SD card
}

/**
 * @brief  Calculates the starting sector of a cluster
 *
 * @param[in] cluster_index Cluster index
 * @return Starting sector of the cluster
 *
 * @details This function calculates the starting sector of a cluster given the
 *          cluster index. The formula used is:
 *          DataStartSector + (cluster_index - 2) * params.BPB_SecPerClus
 */
static inline uint32_t startSectorOfCluster(uint32_t cluster_index)
{
	return (DataStartSector + (cluster_index - 2) * params.BPB_SecPerClus);
}

/**
 * @brief Check if a file is a long file name (LFN) entry
 *
 * This function checks if a file is a long file name (LFN) entry by verifying
 * that the first character of the file name is 0x40 and the file attribute is
 * set to ATTR_LONG_FILE_NAME.
 *
 * @param pFile Pointer to file structure
 * @return true if the file is an LFN entry, false otherwise
 */
static inline bool fileIsLfnEntry(file *pFile)
{
	return (((pFile->DIR_attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_FILE_NAME) && (((uint8_t)pFile->DIR_Name[0] & 0xF0) == 0x40));
}

/**
 * @brief Check if the given file is a free entry
 *
 * This function checks the first character of the file name to determine if the
 * file is a free entry by verifying that the first character is 0xE5.
 *
 * @param pFile Pointer to file structure
 * @return true if the file is a free entry, false otherwise
 */
static inline bool fileIsFreeEntry(file *pFile)
{
	return ((uint8_t)(pFile->DIR_Name[0]) == 0xE5);
}

/**
 * @brief Get the Long File Name (LFN) entry count for a file
 * @param pFile Pointer to file structure
 * @return LFN entry count
 *
 * This function returns the LFN entry count for a file by accessing the
 * LFN_EntCnt member of the file structure.
 */
static inline uint8_t fileLfnEntryCnt(file *pFile)
{
	return pFile->fileEntInf.LFN_EntCnt;
}

/**
 * @brief Get the start cluster of a file
 * @param pFile Pointer to file structure
 * @return Start cluster of the file
 */
static inline uint32_t fileStartCluster(file *pFile)
{
	uint32_t startClus = (uint32_t)pFile->DIR_FstClusLO;

	startClus |= ((uint32_t)(pFile->DIR_FstClusHI)) << 16;

	return startClus;
}

/**
 * @brief Check if the given file is valid
 * @param pFile Pointer to file structure
 * @return true if the file is valid, false otherwise
 *
 * This function checks if the file is valid by verifying that the start
 * cluster is not zero and the file is not end of directory and file name doesnot start with  '.' and '_'.
 *
 */
bool fileIsValid(file *pFile)
{
	const char *name = fileGetName(pFile);

	return ((fileStartCluster(pFile) != 0) && !(fileIsEndOfDirectory(pFile) || (name[0] == '.' && name[1] == '_')));
}

/**
 * @brief Check if the next entry in the directory exists
 *
 * This function checks if the next entry in the directory exists. If the next
 * entry exists, it increments the entry index and returns true. Otherwise, it
 * returns false.
 *
 * @param[in,out] entryIndex entry index of the directory
 * @param[in,out] sectorIndex sector index of the next entry
 * @param[in,out] currentCluster current cluster index
 * @return true if the next entry exists, false otherwise
 */
static bool dirNextEntryExists(uint32_t *entryIndex, uint8_t *sectorIndex, uint32_t *currentCluster)
{
	(*entryIndex)++; // Increment the entry index

	if (((*entryIndex) % 16) == 0) // If reached the end of the sector, read the next sector
	{
		// Increment the sector index
		(*sectorIndex)++;

		if (*sectorIndex == params.BPB_SecPerClus) // If the sector index is equal to the number of sectors per cluster
		{
			// Reset the sector index
			*sectorIndex = 0;
			// Move to the next cluster in the FAT
			*currentCluster = fatNextCluster(*currentCluster);
			if (*currentCluster >= FAT_EOC) // If the end of the FAT is reached, return false
			{
				*entryIndex = 2;
				return false;
			}
		}
		// Read the next sector
		SDFAT32_SD_READ_SECTOR(startSectorOfCluster(*currentCluster) + *sectorIndex, sdBuffer);
	}

	return true;
}

/**
 * @brief Get the file name from a given file structure.
 *
 * This function gets the file name from a given file structure. If the file has
 * long file name entries, it concatenates the long file name entries to form
 * the file name. Otherwise, it copies the file name body and extension from the
 * directory entry.
 *
 * @param[in] pFile Pointer to the file structure.
 * @return A pointer to a string containing the file name.
 */
char *fileGetName(file *pFile)
{
	static char fileName[512];
	size_t nameIndx = 0;

	memset(fileName, 0, sizeof(fileName)); // Clear the file name buffer

	if (pFile->fileEntInf.LFN_EntCnt > 0)
	{
		uint8_t lfnEntryCnt = pFile->fileEntInf.LFN_EntCnt;
		uint32_t currentCluster = pFile->fileEntInf.lfnEntryCluster;
		uint8_t sectorIndex = pFile->fileEntInf.lfnEntrySectorIndex;
		uint32_t entryIndex = pFile->fileEntInf.lfnEntryIndex;

		uint16_t nameUtf16[255];
		memset(nameUtf16, 0, sizeof(nameUtf16));

		SDFAT32_SD_READ_SECTOR(startSectorOfCluster(currentCluster) + sectorIndex, sdBuffer);

		while (lfnEntryCnt) // Loop until all long filename entries are processed
		{
			LFN_entry_t *entry = (LFN_entry_t *)(sdBuffer + (entryIndex % 16) * 32); // Get the long filename entry at the current index
			uint8_t ordinal = entry->LDIR_Ord & 0x1F;

			if (ordinal > 0)
			{
				uint16_t baseIndex = (uint16_t)(ordinal - 1) * 13;
				for (uint8_t i = 0; i < 13 && (baseIndex + i) < 255; i++)
				{
					uint16_t ch = lfnReadNameSlot(entry, i);
					if (ch == 0x0000 || ch == 0xFFFF)
					{
						break;
					}
					nameUtf16[baseIndex + i] = ch;
				}
			}

			lfnEntryCnt--;

			if (!dirNextEntryExists(&entryIndex, &sectorIndex, &currentCluster))
			{
				break;
			}
		}

		for (uint16_t i = 0; i < 255 && nameUtf16[i] != 0; i++)
		{
			uint32_t cp = nameUtf16[i];
			if (cp >= 0xD800 && cp <= 0xDBFF && (i + 1) < 255 && nameUtf16[i + 1] >= 0xDC00 && nameUtf16[i + 1] <= 0xDFFF)
			{
				cp = 0x10000 + (((cp - 0xD800) << 10) | (nameUtf16[i + 1] - 0xDC00));
				i++;
			}
			else if (cp >= 0xD800 && cp <= 0xDFFF)
			{
				cp = '?';
			}

			size_t written = utf8Encode(&fileName[nameIndx], sizeof(fileName) - nameIndx - 1, cp);
			if (written == 0)
			{
				break;
			}
			nameIndx += written;
		}
	}
	else
	{
		for (uint8_t index = 0; index < 8; index++)
		{
			// If the character is a space, break out of the loop
			if (pFile->DIR_Name[index] == ' ')
			{
				break;
			}

			// Every character in the file name body is lower case
			if (pFile->DIR_NTRes & 0x08)
			{
				if ((pFile->DIR_Name[index] >= 'A') && (pFile->DIR_Name[index] <= 'Z'))
				{
					fileName[nameIndx++] = pFile->DIR_Name[index] + 32;
				}
				else
					fileName[nameIndx++] = pFile->DIR_Name[index];
			}
			else
				fileName[nameIndx++] = pFile->DIR_Name[index];
		}

		// If the file is not a directory, append a dot (.) to the end of the name
		if (!fileIsDirectory(pFile))
		{
			if (pFile->DIR_ext[0] != ' ')
			{
				fileName[nameIndx++] = '.';
				for (uint8_t index = 0; index < 3; index++)
				{
					fileName[nameIndx++] = (pFile->DIR_ext[index] == ' ') ? '\0' : pFile->DIR_ext[index] + 32;
				}
			}
		}
	}

	return fileName;
}

/**
 * @brief Extracts the file extension from a given file name.
 *
 * @param[in] file_name The file name from which to extract the extension.
 * @return A pointer to a string containing the file extension.
 */
char *fileGetExtension(const char *file_name)
{
	static char ext[256] = ""; /**< Buffer to store the file extension. */

	memset(ext, 0, sizeof(ext)); /**< Clear the buffer. */

	size_t idx = 0; /**< Index of the current character in the file name. */

	// Loop until we reach the dot (.) or the end of the string.
	while (file_name[idx] != '.')
	{
		idx++;

		// If we have reached the end of the string without finding a dot, return an empty string.
		if (file_name[idx] == '\0')
		{
			return ext;
		}
	}

	// Copy the characters after the dot to the ext buffer.
	strncpy(ext, &file_name[idx + 1], sizeof(ext) - 1);

	return ext;
}

/**
 * @brief Get the root directory entry
 *
 * @return root directory entry
 */
static file getRootDir()
{
	// Read the first sector of the root directory
	SDFAT32_SD_READ_SECTOR(startSectorOfCluster(params.BPB_RootClus), sdBuffer);

	// Get the root directory entry from the sector
	file root = *((file *)&sdBuffer[0]);

	// Initialize the root directory's cluster index and entry index
	root.DIR_FstClusLO = 2;
	root.entryIndex = 1;

	return root;
}

/**
 * @brief Get the next file in the folder
 *
 * @param[in] pDir pointer to the folder
 * @return next file in the folder
 */
file fileGetNext(file *pDir)
{
	file temp = {0}; // Initialize a temporary file structure

	if (!fileIsDirectory(pDir)) // If the folder is not a directory, return the empty file
	{
		PRINTF("Not a Dir\n");
		memset(&temp, 0, sizeof(file));
		return temp;
	}

	uint8_t sectorIndex = (pDir->entryIndex / 16) % params.BPB_SecPerClus; // Calculate the sector index

	uint32_t currentCluster = fileStartCluster(pDir); // Get the current cluster

	uint32_t currentClusterIndex = (pDir->entryIndex / (16 * params.BPB_SecPerClus)); // Calculate the current cluster index

	// Check if we have reached the end of the FAT
	for (uint8_t i = 0; i < currentClusterIndex; i++)
	{
		currentCluster = fatNextCluster(currentCluster); // Move to the next cluster in the FAT
		if (currentCluster >= FAT_EOC)					 // If the end of the FAT is reached, return the empty file
		{
			pDir->entryIndex = 2;
			memset(&temp, 0, sizeof(file));
			return temp;
		}
	}

	if (pDir->entryIndex <= 2) // If the entry index is less than or equal to 2, reset the sector index and cluster index
	{
		sectorIndex = 0;
		currentCluster = fileStartCluster(pDir);
	}

	SDFAT32_SD_READ_SECTOR(startSectorOfCluster(currentCluster) + sectorIndex, sdBuffer); // Read the sector containing the file entries

	while (1) // Loop until a file is found
	{
		temp = *((file *)(sdBuffer + (pDir->entryIndex % 16) * 32)); // Get the file entry at the current index

		if (!fileIsFreeEntry(&temp)) // If the entry is not a free entry
		{
			if (fileIsEndOfDirectory(&temp)) // If the entry is the end of the directory
			{
				memset(&temp, 0, sizeof(file));
				return temp;
			}

			if (fileIsLfnEntry(&temp)) // If the entry is a long filename entry
			{

				uint8_t lfnEntCnt = ((((LFN_entry_t *)&temp)->LDIR_Ord) & 0x0F); // Get the number of long filename entries
				uint8_t lfnEntCntTemp = lfnEntCnt;
				uint32_t currentClusterTemp = currentCluster;
				uint8_t sectorIndexTemp = sectorIndex;
				uint8_t entryIndexTemp = pDir->entryIndex;

				while (lfnEntCnt > 0)
				{
					lfnEntCnt--;

					// Check if next entry in the directory exists
					if (!dirNextEntryExists(&pDir->entryIndex, &sectorIndex, &currentCluster))
					{
						memset(&temp, 0, sizeof(file));
						return temp;
					}
				}

				temp = *((file *)(sdBuffer + (pDir->entryIndex % 16) * 32)); // Get the file entry at the current index
				temp.fileEntInf.LFN_EntCnt = lfnEntCntTemp;					 // Set the number of long filename entries
				temp.fileEntInf.lfnEntryCluster = currentClusterTemp;		 // Set the cluster index
				temp.fileEntInf.lfnEntrySectorIndex = sectorIndexTemp;		 // Set the sector index
				temp.fileEntInf.lfnEntryIndex = entryIndexTemp % 16;		 // Set the entry index
			}
			else
			{

				temp.fileEntInf.LFN_EntCnt = 0;			 // Set the number of long filename entries to 0
				temp.fileEntInf.lfnEntryCluster = 0;	 // Set the cluster index
				temp.fileEntInf.lfnEntrySectorIndex = 0; // Set the sector index
				temp.fileEntInf.lfnEntryIndex = 0;		 // Set the entry index
			}
			temp.fileEntInf.entryIndex = pDir->entryIndex % 16; // Set the entry index
			temp.fileEntInf.Cluster = currentCluster;			// Set the cluster index
			temp.fileEntInf.sectorIndex = sectorIndex;			// Set the sector index
			pDir->entryIndex++;									// Increment the entry index
			break;
		}
		else
		{
			if (!dirNextEntryExists(&pDir->entryIndex, &sectorIndex, &currentCluster))
			{
				memset(&temp, 0, sizeof(file));
				return temp;
			}
		}
	}
	temp.entryIndex = fileIsDirectory(&temp) ? 2 : 0; // Set the entry index of the file

	return temp;
}

/**
 *
 * @brief Checks if a file exists in the given directory.
 * @param[in] file The name of the file to search for.
 * @param[in] pDir The directory to search in.
 * @return The file if it exists, otherwise an empty file.
 */
static file fileExists(file *pDir, const char *filename)
{
	file tempFile = {0};

	do
	{
		tempFile = fileGetNext(pDir);
		if (fileIsValid(&tempFile))
		{
			char *tempFileName = fileGetName(&tempFile);

			if (strcmp(filename, tempFileName) == 0)
			{
				return tempFile;
			}
		}
	} while (!fileIsEndOfDirectory(&tempFile));
	// If the file is not found, return an empty file
	memset(&tempFile, 0, sizeof(file));
	return tempFile;
}

/**
 * @brief Finds a file in the given path.
 * @param[in] path The path to find the file in.
 * @return The file if it exists, otherwise an empty file.
 */
static file pathExists(const char *path)
{
	file tempFile = getRootDir();

	// If the path is the root directory, return the root directory
	if (strlen(path) == 1 && path[0] == '/')
	{
		goto exit;
	}

	// Iterate through the path and find the file
	uint8_t index = 0;
	uint8_t charCnt = 0;
	while (path[charCnt] != '\0')
	{
		char pathFragment[36] = "";
		for (uint8_t i = index + 1; i < index + 36; i++)
		{
			charCnt++;

			// If the end of the path is reached or a '/' is encountered, break
			if (path[i] == '/' || path[i] == '\0')
			{
				break;
			}
			pathFragment[charCnt - index - 1] = path[i]; // Store the current directory name
		}
		index = charCnt;								// Move the index to the next directory
		tempFile = fileExists(&tempFile, pathFragment); // Check if the file exists
	}
exit:
	return tempFile; // Return the file if it is found
}

/**
 * @brief Displays the time in a human-readable format
 *
 * @param[in] time Time value in FAT format
 *
 * @details This function takes a time value in FAT format, which is a
 *          16-bit value containing the time in the format:
 *          HH:MM:SS, where the values are:
 *          HH: hours (0-23)
 *          MM: minutes (0-59)
 *          SS: seconds (0-29, in 2-second increments)
 *          This function converts the FAT time format to a human-readable
 *          string in the format HH:MM:SS.
 */
static void displayTime(uint16_t time)
{
	uint8_t hours = (time & 0xF800) >> 11;	// extract hours (0-23)
	uint8_t minutes = (time & 0x07E0) >> 5; // extract minutes (0-59)
	uint8_t seconds = (time & 0x001F) * 2;	// extract seconds (0-29, in 2-second increments)

	PRINTF(hours >= 10 ? "%d" : "0%d", hours); // print hours
	PRINTF(":");
	PRINTF(minutes >= 10 ? "%d" : "0%d", minutes); // print minutes
	PRINTF(":");
	PRINTF(seconds >= 10 ? "%d" : "0%d", seconds); // print seconds
}

/**
 *
 * @brief Displays the date in a human-readable format
 *
 * @param[in] date Date value in FAT format
 *
 * @details This function takes a date value in FAT format, which is a
 *          16-bit value containing the date in the format:
 *          YYYYYYYMMMMDDDDD, where the values are:
 *          YYYYYYY: year since 1980 (0-127)
 *          MMMM: month (1-12)
 *          DDDDD: day (1-31)
 *          The function converts the FAT date format to a human-readable
 *          string in the format YYYY-MM-DD.
 *
 */
static void displayDate(uint16_t date)
{
	uint16_t year = ((date >> 9) & 0x7F) + 1980; // extract year and adjust to actual year
	uint8_t month = (date >> 5) & 0x0F;			 // extract month (1-12)
	uint8_t day = date & 0x1F;					 // extract day (1-31)

	PRINTF("%d", year); // print year
	PRINTF("-");
	PRINTF(month < 10 ? "0%d" : "%d", month); // print month with leading zero if needed
	PRINTF("-");
	PRINTF(day < 10 ? "0%d" : "%d", day); // print day with leading zero if needed
}

/**
 * @brief Display details of a file or directory
 *
 * This function prints the name, date, time, and size of a file or directory.
 * If the file is a directory, a slash '/' is appended to its name.
 * The output is indented by a specified number of tab spaces.
 *
 * @param[in] pFile Pointer to the file structure
 * @param[in] tab Number of tab spaces for indentation
 */
static void displayFile(file *pFile, uint8_t tab)
{
	// Indent the output by the specified number of tab spaces
	for (uint8_t i = 0; i < tab; i++)
	{
		PRINTF("    ");
	}

	// Print the file or directory name
	PRINTF("%s", fileGetName(pFile));

	// Append a slash if the file is a directory
	if (fileIsDirectory(pFile))
	{
		PRINTF("/");
	}

	PRINTF("     ");

	// Display the date the file was last written to
	displayDate(pFile->DIR_WrtDate);

	PRINTF(" || ");

	// Display the time the file was last written to
	displayTime(pFile->DIR_WrtTime);

	PRINTF(" || ");

	// Display the size of the file in bytes
	PRINTF("%d Bytes\n", pFile->DIR_FileSize);
}

/**
 * @brief Prints the content of a file
 *
 * This function reads the content of a file in clusters and prints it to the
 * console. It stops reading when it reaches the end of the file or a cluster
 * marked as end of chain (EOC).
 *
 * @param[in] pFile Pointer to the file structure
 */
static void filePrintContents(file *pFile)
{
	if (fileIsValid(pFile))
	{
		uint32_t charCnt = 0;
		uint32_t Cluster = fileStartCluster(pFile);

		PRINTF("\n");
		// Loop until the end of the file is reached
		do
		{
			// Loop through each sector in the cluster
			for (uint8_t i = 0; i < params.BPB_SecPerClus; i++)
			{
				// Read the sector content
				SDFAT32_SD_READ_SECTOR(startSectorOfCluster(Cluster) + i, sdBuffer);
				// Loop through each byte in the sector
				for (uint16_t c = 0; c < 512; c++)
				{
					// Print the character
					PRINTF("%c", sdBuffer[c]);
					// Increment character count
					charCnt++;
					// Stop if the character count reaches the file size
					if (charCnt == fileSize(pFile))
					{
						return;
					}
				}
			}

			// Move to the next cluster
			Cluster = fatNextCluster(Cluster);
		} while (Cluster < FAT_EOC);
	}
}

bool listDirectory(const char *path)
{
	file tempFile = pathExists(path);
	if (fileStartCluster(&tempFile) == 0)
	{
		PRINTF("Invalid Path!\n");
		return false;
	}
	if (!fileIsDirectory(&tempFile))
	{

		filePrintContents(&tempFile);
		PRINTF("\n");

		return true;
	}
	file folder = tempFile;

	do
	{
		tempFile = fileGetNext(&folder);
		if (fileIsValid(&tempFile))
		{
			displayFile(&tempFile, 0);
		}
	} while (!fileIsEndOfDirectory(&tempFile));

	return true;
}

/**
 * @brief Lists the contents of the directory recursively
 *
 * This function lists the contents of the given directory recursively. It
 * handles subdirectories and prints the contents of each subdirectory
 * indented to show the hierarchy.
 *
 * @param[in] pDir Pointer to the directory to list
 * @param[in] tab Number of tabs to indent the output
 */
void listDirectoryRecursive(file *pDir, uint8_t tab)
{
	file tempFile;

	do
	{
		tempFile = fileGetNext(pDir);
		if (fileIsValid(&tempFile))
		{
			if (fileIsDirectory(&tempFile))
			{
				// Directory, list its contents recursively
				PRINTF("\n");
				displayFile(&tempFile, tab);
				listDirectoryRecursive(&tempFile, tab + 2);
				PRINTF("\n");
			}
			else
			{
				// File, print its details
				displayFile(&tempFile, tab);
			}
		}
	} while (!fileIsEndOfDirectory(&tempFile));
}

/**
 * @brief Reads data from a file on the SD card
 *
 * This function reads the specified length of data from the file into the provided buffer.
 * It handles cluster transitions and updates the file's entry index accordingly.
 *
 * @param[in] pFile Pointer to the file structure to read from
 * @param[out] buffer Pointer to the buffer to store the read data
 * @param[in] len Length of the data to be read
 * @return Number of bytes successfully read
 */
uint32_t fileRead(file *pFile, uint8_t *buffer, uint32_t len)
{
	static uint32_t Cluster = 0;	// Current cluster being read
	static uint8_t sectorIndex = 0; // Current sector index within the cluster
	uint32_t idx = 0;				// Index to track bytes read

	// Check if the file is invalid or not open for reading
	if (!fileIsValid(pFile) || !(pFile->accessMode & FA_READ))
	{
		goto done;
	}

	// Initialize cluster and sectorIndex if at the start of the file
	if (pFile->entryIndex == 0)
	{
		Cluster = fileStartCluster(pFile);
		sectorIndex = 0;
	}

	while (len > 0)
	{
		// Check if we need to move to the next cluster
		if ((pFile->entryIndex > 0) && (pFile->entryIndex % (params.BPB_SecPerClus * params.BPB_BytesPerSec) == 0))
		{
			sectorIndex = 0;
			Cluster = fatNextCluster(Cluster);
			// If end of cluster chain is reached, reset entry index and exit
			if (Cluster >= FAT_EOC)
			{
				pFile->entryIndex = 0;
				goto done;
			}
		}

		// Read the next sector if needed
		if ((pFile->entryIndex % params.BPB_BytesPerSec) == 0)
		{
			if (!SDFAT32_SD_READ_SECTOR(startSectorOfCluster(Cluster) + sectorIndex++, sdBuffer))
			{
				// Exit if read fails
				goto done;
			}
		}

		// Copy data from the buffer to the output and update indices
		buffer[idx++] = sdBuffer[pFile->entryIndex % params.BPB_BytesPerSec];
		pFile->entryIndex++;
		len--;
	}

done:
	return idx; // Return the number of bytes successfully read
}

/**
 *
 * @brief Set the start cluster for a file
 *
 * This function sets the start cluster for a file by updating the
 * DIR_FstClusLO and DIR_FstClusHI fields of the file structure.
 *
 * @param[in] pFile Pointer to file structure
 * @param[in] cluster Cluster index
 *
 */
static void fileSetStartClus(file *pFile, uint32_t cluster)
{
	// Split the cluster index into two 16-bit values
	pFile->DIR_FstClusLO = (uint16_t)(cluster & 0x0000FFFF);
	pFile->DIR_FstClusHI = (uint16_t)((cluster & 0xFFFF0000) >> 16);
}

/**
 *
 *  @brief Set the date for a file
 *
 * This function sets the date for a file by updating the DIR_WrtDate field of
 * the file structure.
 *
 * @param[in] pFile Pointer to file structure
 * @param[in] year Year (1980-2107)
 * @param[in] month Month (1-12)
 * @param[in] day Day (1-31)
 *
 */
static void fileSetDate(file *pFile, uint16_t year, uint8_t month, uint8_t day)
{
	// Adjust the year to the FAT date format (1980-2107)
	year -= 1980;

	// Set the date in the file structure
	pFile->DIR_WrtDate = (uint16_t)day;
	pFile->DIR_WrtDate |= (uint16_t)month << 5;
	pFile->DIR_WrtDate |= year << 9;
}

/**
 * @brief Set the time for a file
 *
 * This function sets the time for a file by updating the DIR_WrtTime field of
 * the file structure.
 *
 * @param[in] pFile Pointer to file structure
 * @param[in] hours Hours (0-23)
 * @param[in] minutes Minutes (0-59)
 * @param[in] seconds Seconds (0-59)
 *
 */
static void fileSetTime(file *pFile, uint8_t hours, uint8_t minutes, uint8_t seconds)
{
	// Set the time in the file structure
	pFile->DIR_WrtTime = (uint16_t)(seconds / 2); // seconds in 2-second increments
	pFile->DIR_WrtTime |= (uint16_t)minutes << 5; // minutes (0-59)
	pFile->DIR_WrtTime |= (uint16_t)hours << 11;  // hours (0-23)
}

/**
 *
 * @brief Finds a free entry in the directory
 *
 * This function searches for a free entry in the given directory. It starts
 * searching from the first sector of the cluster and moves to the next sector
 * until a free entry is found. If the end of the cluster is reached, it moves
 * to the next cluster until a free entry is found or the end of the cluster
 * chain is reached.
 *
 * @param[in] Dir Pointer to the directory structure
 * @param[in] freeEntryCnt Number of free entries needed
 * @return freeEntInf_t structure containing the cluster index, sector index,
 *         and entry index of the free entry.
 */
static freeEntInf_t getFreeEntry(file *Dir, uint8_t freeEntryCnt)
{
	freeEntInf_t freeEntInf;
	freeEntInf.Cluster = fileStartCluster(Dir); // start cluster index
	do
	{
		// iterate through each sector in the cluster
		for (freeEntInf.sectorIndex = 0; freeEntInf.sectorIndex < params.BPB_SecPerClus; freeEntInf.sectorIndex++)
		{
			// read the sector and iterate through each entry
			SDFAT32_SD_READ_SECTOR(startSectorOfCluster(freeEntInf.Cluster) + freeEntInf.sectorIndex, sdBuffer);
			for (freeEntInf.entryIndex = 0; freeEntInf.entryIndex < 16; freeEntInf.entryIndex++)
			{
				file temp = *((file *)(sdBuffer + freeEntInf.entryIndex * 32));
				if (fileIsFreeEntry(&temp) || fileIsEndOfDirectory(&temp))
				{
					// if the entry is free, return the freeEntInf structure
					if (fileIsEndOfDirectory(&temp))
					{
						// if the end of the directory is reached, return the structure
						if ((freeEntInf.entryIndex + freeEntryCnt) > 15)
						{
							// read the sector and fill the remaining entries with 0xE5
							SDFAT32_SD_READ_SECTOR(startSectorOfCluster(freeEntInf.Cluster) + freeEntInf.sectorIndex, sdBuffer);
							for (uint8_t i = freeEntInf.entryIndex; i < 16; i++)
							{
								file *pFile = (file *)(sdBuffer + (i * 32));
								pFile->DIR_Name[0] = 0xE5;
							}
							// write the sector back
							SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(freeEntInf.Cluster) + freeEntInf.sectorIndex, sdBuffer);

							// increment the sector index
							freeEntInf.sectorIndex++;
							// reset the entry index
							freeEntInf.entryIndex = 0;
						}
						// return the freeEntInf structure
						return freeEntInf;
					}

					// if the required number of free entries is found, return the structure
					if (freeEntryCnt == 1)
					{
						return freeEntInf;
					}

					// if the entry is not free, continue to the next entry
					uint8_t i;
					for (i = 0; i < freeEntryCnt; i++)
					{
						freeEntInf.entryIndex += i;
						if (freeEntInf.entryIndex == 16)
						{
							break;
						}

						temp = *((file *)(sdBuffer + freeEntInf.entryIndex * 32));
						if (!fileIsFreeEntry(&temp))
						{
							break;
						}
					}
					if (i != freeEntryCnt)
					{
						continue;
					}

					freeEntInf.entryIndex -= (freeEntryCnt - 1);
					// return the freeEntInf structure
					return freeEntInf;
				}
			}
		}

		// if the end of the cluster is reached, move to the next cluster
	} while ((freeEntInf.Cluster = fatNextCluster(freeEntInf.Cluster)) < FAT_EOC);
	// if the end of the cluster chain is reached, return an empty structure
	memset(&freeEntInf, 0, sizeof(freeEntInf_t));
	return freeEntInf;
}

/**
 *
 *  @brief Parse the date and time strings to get the numerical values
 *
 * This function takes the date and time strings (e.g. "Apr 12 2023" and
 * "23:59:59") and parses them to get the numerical values. The values are
 * stored in the given pointers.
 *
 * @param year Pointer to store the year
 * @param month Pointer to store the month (1-12)
 * @param day Pointer to store the day (1-31)
 * @param hour Pointer to store the hour (0-23)
 * @param minute Pointer to store the minute (0-59)
 * @param second Pointer to store the second (0-59)
 */
static void getDateTimeNumerical(uint16_t *year, uint8_t *month, uint8_t *day,
								 uint8_t *hour, uint8_t *minute, uint8_t *second)
{
	const char *dateStr = __DATE__; // e.g. "Apr 12 2023"
	const char *timeStr = __TIME__; // e.g. "23:59:59"
	char *endptr;					// Pointer to the end of the parsed string

	// Parse date string
	char monthStr[4] = {'\0'};
	// Extract month
	memcpy(monthStr, dateStr, 3);

	for (int i = 0; i < 12; i++)
	{
		if (strcmp(monthStr, months[i]) == 0)
		{
			*month = i + 1;
		}
	}

	// Parse day
	*day = strtol(dateStr + 4, &endptr, 10);
	// Parse year
	*year = strtol(endptr + 1, NULL, 10);

	// Parse time string
	*hour = strtol(timeStr, &endptr, 10);
	*minute = strtol(endptr + 1, &endptr, 10);
	*second = strtol(endptr + 1, NULL, 10);
}

/**
 *
 * @brief Calculate the length of a file name (without extension)
 *
 * This function takes a file name as input and returns the length of the file
 * name without the extension. For example, if the file name is "example.txt",
 * the function returns 7.
 *
 * @param filename The file name
 * @return The length of the file name without the extension
 *
 */
static uint8_t fileNameLength(const char *filename)
{
	uint8_t i;
	for (i = 0; i < strlen(filename); i++)
	{
		/* Stop at the first '.' character, which marks the beginning of the
		 * file extension. */
		if (filename[i] == '.')
		{
			break;
		}
	}
	return i;
}

/**
 *
 * @brief Check if all characters in a file name are lower case
 *
 * This function takes a file name as input and checks if all characters in the
 * file name are lower case.
 *
 * @param filename The file name
 * @return true if all characters in the file name are lower case, false otherwise
 */
static bool allLowerCase(const char *filename)
{
	// Iterate through each character in the file name
	for (uint8_t i = 0; i < fileNameLength(filename); i++)
	{
		// Check if the character is an upper case letter
		if ((((uint8_t)filename[i]) < 91) && (((uint8_t)filename[i]) > 64))
		{
			// If the character is an upper case letter, return false
			return false;
		}
	}
	// If all characters in the file name are lower case, return true
	return true;
}

/**
 * @brief Check if a file name contains a mix of upper and lower case letters
 *
 * This function checks if a given file name contains a mix of upper and lower
 * case letters. It first checks if the file name is not all lower case, and if
 * not, it iterates through each character in the file name to see if there are
 * any lower case letters present.
 *
 * @param filename The file name to check
 * @return true if the file name contains mixed case letters, false otherwise
 */
static bool mixedLetters(const char *filename)
{
	// Check if the filename is not all lower case
	if (!allLowerCase(filename))
	{
		// Iterate through each character in the filename
		for (uint8_t i = 0; i < fileNameLength(filename); i++)
		{
			// Check if the character is a lower case letter
			if (((uint8_t)filename[i] > 96))
			{
				// Return true if a lower case letter is found
				return true;
			}
		}
	}
	// Return false if no mixed case letters are present
	return false;
}

/**
 * @brief Calculate the checksum of a file entry
 *
 * This function calculates the checksum of a given file entry. It computes
 * the checksum based on the DIR_Name and DIR_ext fields of the file entry
 * structure, useful for validating the integrity of the file entry.
 *
 * @param[in] entry Pointer to the file entry structure
 * @return Checksum value as an 8-bit unsigned integer
 */
static uint8_t getChecksum(file *entry)
{
	uint8_t i;		 // Loop counter
	uint8_t Sum = 0; // Variable to store the checksum

	// Calculate sum of DIR_Name[] field
	for (i = 0; i < 8; i++)
	{
		Sum = (Sum >> 1) + (Sum << 7) + (uint8_t)entry->DIR_Name[i];
	}
	// Calculate sum of DIR_ext[] field
	for (i = 0; i < 3; i++)
	{
		Sum = (Sum >> 1) + (Sum << 7) + (uint8_t)entry->DIR_ext[i];
	}
	return Sum; // Return the calculated checksum
}

/**
 * @brief Update the FSInfo sector with the next free cluster information
 *
 * This function reads the FSInfo sector from the SD card, updates the
 * next free cluster and the free cluster count, and writes the updated
 * information back to the FSInfo sector.
 *
 * @param nxtFreeClus The next free cluster index to be updated in FSInfo
 * @return true if the update was successful, false otherwise
 */
static bool updateFSInfo(uint32_t nxtFreeClus)
{
	// Read the FSInfo sector from the SD card
	if (SDFAT32_SD_READ_SECTOR(fsInfoSecLba, sdBuffer))
	{
		FSInfo_t *p_fsinfo = (FSInfo_t *)sdBuffer;

		// Update the next free cluster and decrement the free cluster count
		p_fsinfo->FSI_Nxt_Free = nxtFreeClus;
		p_fsinfo->FSI_Free_Count--;

		// Write the updated FSInfo sector back to the SD card
		if (SDFAT32_SD_WRITE_SECTOR(fsInfoSecLba, sdBuffer))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return false; // Return false if reading the FSInfo sector failed
}

/**
 *
 * @brief Gets the next free cluster index from the FSInfo sector
 *
 * This function reads the FSInfo sector from the SD card, gets the next free
 * cluster index, and increments it until it finds a free cluster. If the
 * update is successful, it returns the next free cluster index. Otherwise,
 * it returns 0xFFFFFFFF.
 *
 * @return The next free cluster index or 0xFFFFFFFF on failure
 */
static uint32_t getNextFreeCluster()
{
	// Read the FSInfo sector from the SD card
	if (SDFAT32_SD_READ_SECTOR(fsInfoSecLba, sdBuffer))
	{
		FSInfo_t *p_fsinfo = (FSInfo_t *)sdBuffer;
		uint32_t nxtFreeClus = p_fsinfo->FSI_Nxt_Free;

		// Increment the next free cluster index until it finds a free cluster
		while (fatNextCluster(nxtFreeClus) != 0x00000000)
		{
			nxtFreeClus++;
		}

		// Update the FSInfo sector with the new next free cluster index
		if (updateFSInfo(nxtFreeClus))
		{
			return nxtFreeClus;
		}
		else
		{
			return 0xFFFFFFFF;
		}
	}
	// Return 0xFFFFFFFF on failure
	else
	{
		return 0xFFFFFFFF;
	}
}

/**
 *
 * @brief Creates a file or directory in the specified directory
 *
 * This function creates a new file or directory in the specified directory.
 * It allocates a cluster for the new file or directory, sets up the
 * directory entry, and writes it to the disk.
 *
 * @param[in] pathDir Pointer to the directory where the file or directory will be created
 * @param[in] filename The name of the file or directory
 * @param[in] isDir Boolean indicating if the entry is a directory
 * @return The created file structure
 */
static file fileCreate(file *pathDir, const char *filename, bool isDir)
{
	file newFile = {0};

	// Allocate a new cluster for the file
	uint32_t fileStartClus = getNextFreeCluster();
	fileSetStartClus(&newFile, fileStartClus);
	fatSetNextClus(fileStartClus, FAT_EOC);

	uint8_t tempIndx = 0;
	memset(newFile.DIR_Name, ' ', 8);
	memset(newFile.DIR_ext, ' ', 3);

	freeEntInf_t freeEntInf;
	uint8_t lfnEntCnt;

	// Check if filename requires long file name (LFN) entries
	if (filenameNeedsLfn(filename))
	{
		for (uint8_t i = 0; i < strlen(filename); i++)
		{
			if (filename[i] == '.')
			{
				tempIndx = i + 1;
				break;
			}
			if (i < 8)
			{
				// Convert lower case to upper case
				if ((filename[i] > 96) && (filename[i] < 123))
				{
					newFile.DIR_Name[i] = filename[i] - 32;
				}
				else
				{
					newFile.DIR_Name[i] = (((uint8_t)filename[i]) < 0x80) ? filename[i] : '_';
				}
			}
			if (fileNameLength(filename) > 8)
			{
				newFile.DIR_Name[6] = '~';
				newFile.DIR_Name[7] = '1';
			}
		}
		for (uint8_t i = 0; i < 3; i++)
		{
			if (filename[tempIndx + i] == '\0')
			{
				break;
			}
			newFile.DIR_ext[i] = filename[tempIndx + i] - 32;
		}
		uint16_t nameUtf16[255];
		uint16_t nameUtf16Units = utf8ToUtf16(filename, nameUtf16, 255);
		lfnEntCnt = nameUtf16Units / 13;

		if ((nameUtf16Units % 13) != 0)
		{
			lfnEntCnt += 1;
		}

		uint8_t lfnEntCntTemp = lfnEntCnt;

		// Get free entry for LFN and file entry
		freeEntInf = getFreeEntry(pathDir, lfnEntCnt + 1);

		// Set LFN entry information
		newFile.fileEntInf.LFN_EntCnt = lfnEntCnt;
		newFile.fileEntInf.lfnEntryCluster = freeEntInf.Cluster;
		newFile.fileEntInf.lfnEntrySectorIndex = freeEntInf.sectorIndex;
		newFile.fileEntInf.lfnEntryIndex = freeEntInf.entryIndex;

		SDFAT32_SD_READ_SECTOR(startSectorOfCluster(freeEntInf.Cluster) + freeEntInf.sectorIndex, sdBuffer);

		// Write LFN entries
		while (lfnEntCnt)
		{
			LFN_entry_t *entry = (LFN_entry_t *)(sdBuffer + (freeEntInf.entryIndex + lfnEntCnt - 1) * 32);
			entry->LDIR_Attr = ATTR_LONG_FILE_NAME;
			entry->LDIR_FstClusLO = 0;
			entry->LDIR_Type = 0;
			entry->LDIR_Ord = lfnEntCntTemp - lfnEntCnt + 1;
			entry->LDIR_Chksum = getChecksum(&newFile);
			memset(entry->LDIR_Name1, 0, 10);
			memset(entry->LDIR_Name2, 0, 12);
			memset(entry->LDIR_Name3, 0, 4);

			if (lfnEntCnt == 1)
			{
				entry->LDIR_Ord |= 0x40;
			}

			uint16_t unitBase = (uint16_t)(entry->LDIR_Ord & 0x1F) - 1;
			unitBase *= 13;

			for (uint8_t i = 0; i < 13; i++)
			{
				uint16_t unitIndex = unitBase + i;
				uint16_t value = 0xFFFF;
				if (unitIndex < nameUtf16Units)
				{
					value = nameUtf16[unitIndex];
				}
				else if (unitIndex == nameUtf16Units)
				{
					value = 0x0000;
				}
				lfnWriteNameSlot(entry, i, value);
			}

			lfnEntCnt--;
		}
		freeEntInf.entryIndex += lfnEntCntTemp;
	}
	else
	{
		// Get free entry for file entry
		freeEntInf = getFreeEntry(pathDir, 1);
		SDFAT32_SD_READ_SECTOR(startSectorOfCluster(freeEntInf.Cluster) + freeEntInf.sectorIndex, sdBuffer);

		// Set file name and extension
		for (uint8_t i = 0; i < 9; i++)
		{
			if (filename[i] == '.')
			{
				tempIndx = i + 1;
				break;
			}

			// Convert lower case to upper case
			if ((filename[i] > 96) && (filename[i] < 123))
			{
				newFile.DIR_Name[i] = filename[i] - 32;
			}
			else
			{
				newFile.DIR_Name[i] = filename[i];
			}
		}
		for (uint8_t i = 0; i < 3; i++)
		{
			if (filename[i] == '\0')
			{
				break;
			}
			newFile.DIR_ext[i] = filename[tempIndx + i] - 32;
		}
		newFile.fileEntInf.LFN_EntCnt = 0;
		newFile.fileEntInf.lfnEntryCluster = 0;
		newFile.fileEntInf.lfnEntrySectorIndex = 0;
		newFile.fileEntInf.lfnEntryIndex = 0;
	}

	// Set attributes for directory or file
	newFile.DIR_attr = isDir ? ATTR_DIRECTORY : 0;

	// Set NTRes attribute for case sensitivity
	newFile.DIR_NTRes = allLowerCase(filename) ? 0x18 : 0x10;

	// Set creation date and time
	uint16_t year = 0;
	uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
	getDateTimeNumerical(&year, &month, &day, &hour, &minute, &second);
	fileSetDate(&newFile, year, month, day);
	fileSetTime(&newFile, hour, minute, second);

	// Update file entry information
	newFile.fileEntInf.Cluster = freeEntInf.Cluster;
	newFile.fileEntInf.sectorIndex = freeEntInf.sectorIndex;
	newFile.fileEntInf.entryIndex = freeEntInf.entryIndex;

	// Write file entry to disk
	file *pFile = (file *)(sdBuffer + freeEntInf.entryIndex * 32);
	memcpy(pFile, &newFile, 32);

	// Check if the file was created successfully
	if (SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(freeEntInf.Cluster) + freeEntInf.sectorIndex, sdBuffer))
	{
		PRINTF("File Created!\n");
	}
	else
	{
		memset(&newFile, 0, sizeof(file));
	}

	return newFile;
}

/**
 * @brief Opens a file at the specified path with the given access mode.
 *
 * This function checks if the provided path is a valid directory and then attempts
 * to open a file with the specified filename and access mode. If the file doesn't
 * exist and the access mode permits writing, a new file is created.
 *
 * @param[in] path The path to the directory containing the file.
 * @param[in] filename The name of the file to open.
 * @param[in] accessMode The mode in which to open the file (e.g., read, write, append).
 * @return The file structure representing the opened or newly created file, or an
 *         invalid file structure if the path does not exist or file creation fails.
 */
file fileOpen(const char *path, const char *filename, uint8_t accessMode)
{
	file tempFile = {0};

	// Check if both writing and appending is allowed. If so, return an invalid file, otherwise continue.
	if (!((accessMode & FA_WRITE) && (accessMode & FA_APPEND)))
	{

		// Check if the specified path exists and is a directory
		file pathDir = pathExists(path);
		if ((fileStartCluster(&pathDir) == 0) || !fileIsDirectory(&pathDir))
		{
			PRINTF("Path does not exist!\n");
			return pathDir; // Return the directory file if path is invalid
		}
		else
		{
			// Check if the file already exists in the directory
			tempFile = fileExists(&pathDir, filename);

			// If the file doesn't exist and writing or appending is allowed, create a new file
			if (!fileIsValid(&tempFile))
			{
				if (accessMode & (FA_WRITE | FA_APPEND))
				{
					PRINTF("File doesn't exist, creating new file\n");
					tempFile = fileCreate(&pathDir, filename, false);
					if (fileIsValid(&tempFile)) // Check if the file was created successfully
					{
						tempFile.accessMode = accessMode;
					}
				}
			}
			else
			{
				PRINTF("File exists!\n");
				tempFile.accessMode = accessMode; // Set the access mode for the existing file
			}
		}
	}
	return tempFile; // Return the file structure
}

/**
 * @brief Opens a directory at the specified path.
 *
 * This function checks if the given path points to a directory
 * and returns the file structure representing the directory. If
 * the path does not point to a valid directory, an empty file
 * structure is returned.
 *
 * @param[in] path The path to the directory to open.
 * @return The file structure of the directory if it exists, otherwise
 *         an empty file structure.
 */
file openDirectory(const char *path)
{
	// Check if the path exists and is a directory
	file direcotry = pathExists(path);
	if (!fileIsDirectory(&direcotry))
	{
		// If not a directory, return an empty file structure
		memset(&direcotry, 0, sizeof(file));
	}
	return direcotry;
}

/**
 * @brief Create a directory at the specified path
 *
 * This function creates a new directory with the given name at the specified path.
 * If the directory already exists, it returns the existing directory.
 *
 * @param[in] path The path where the directory should be created
 * @param[in] name The name of the directory to be created
 * @return The created directory file structure if successful, otherwise the existing directory or invalid path
 */
file createDirectory(const char *path, const char *name)
{
	// Check if the specified path exists
	file parentDir = pathExists(path);

	// If the path is invalid, return with an error message
	if (fileStartCluster(&parentDir) == 0)
	{
		PRINTF("Invalid path!");
		return parentDir;
	}

	// Check if the directory already exists in the specified path
	file thisDir = fileExists(&parentDir, name);

	// If the directory exists, return the existing directory
	if (fileStartCluster(&thisDir) != 0)
	{
		PRINTF("Folder exists!");
		return thisDir;
	}

	// Create the new directory
	thisDir = fileCreate(&parentDir, name, true);

	// Get the start cluster of the new directory
	uint32_t dirStartClus = fileStartCluster(&thisDir);

	// Initialize directory name and extension fields
	memset(thisDir.DIR_Name, ' ', 8);
	memset(thisDir.DIR_ext, ' ', 3);
	memset(parentDir.DIR_Name, ' ', 8);
	memset(parentDir.DIR_ext, ' ', 3);

	// Set current directory entry
	thisDir.DIR_Name[0] = '.';

	// Set parent directory entry
	parentDir.DIR_Name[0] = '.';
	parentDir.DIR_Name[1] = '.';

	// Clear the SD buffer
	memset(sdBuffer, 0, 512);

	// Write empty sectors for the new directory
	for (uint8_t sectorIndex = 0; sectorIndex < params.BPB_SecPerClus; sectorIndex++)
	{
		SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(dirStartClus) + sectorIndex, sdBuffer);
	}

	// Write current and parent directory entries to the first sector
	memcpy(sdBuffer, &thisDir, 32);
	memcpy(sdBuffer + 32, &parentDir, 32);

	// Write the directory entries to the SD card
	SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(dirStartClus), sdBuffer);

	return thisDir;
}

/**
 * @brief Writes or appends data to a file on the SD card
 *
 * This function writes or appends the provided data to the specified file. It handles
 * cluster allocation and updates the file's metadata accordingly.
 *
 * @param[in] pFile Pointer to the file structure to write to
 * @param[in] data Pointer to the data to be written
 * @param[in] len Length of the data to be written
 * @return true if the write operation is successful, false if file is not open for writing or an error occurs
 */
bool fileWrite(file *pFile, const uint8_t *data, uint32_t len)
{

	// Check if the file is open for writing or appending
	if (!(pFile->accessMode & (FA_WRITE | FA_APPEND)))
	{
		return false;
	}

	// File cannot be opened for both writing and appending
	if ((pFile->accessMode & FA_WRITE) && (pFile->accessMode & FA_APPEND))
	{
		return false;
	}

	uint32_t currentCluster = fileStartCluster(pFile);														  // Get the starting cluster
	uint16_t byteIndex = (pFile->accessMode & FA_WRITE) ? 0 : (pFile->DIR_FileSize % params.BPB_BytesPerSec); // Calculate byte offset in sector
	uint32_t sectorIndex = (pFile->accessMode & FA_WRITE) ? 0 : pFile->DIR_FileSize / params.BPB_BytesPerSec; // Calculate sector index
	uint32_t clusterCnt = (sectorIndex / params.BPB_SecPerClus) + 1;										  // Calculate number of clusters
	uint32_t byteCnt = 0;																					  // Counter for number of bytes written
	bool moreData = true;																					  // Flag to indicate if there is more data to write

	sectorIndex = sectorIndex % params.BPB_SecPerClus; // Normalize sector index within cluster
	uint32_t tempCluster;

	// Handle cluster allocation if necessary
	if (clusterCnt > 1)
	{
		while (clusterCnt)
		{
			tempCluster = currentCluster;
			currentCluster = fatNextCluster(currentCluster); // Move to next cluster
			clusterCnt--;
		}
		uint32_t nextClus = getNextFreeCluster(); // Allocate new cluster
		fatSetNextClus(tempCluster, nextClus);	  // Link current cluster to new cluster
		fatSetNextClus(nextClus, FAT_EOC);		  // Mark new cluster as end of chain
	}

	// Read the current sector if not starting at the beginning
	if (byteIndex != 0)
	{
		SDFAT32_SD_READ_SECTOR(startSectorOfCluster(currentCluster) + sectorIndex, sdBuffer);
	}

	// Write data to sectors
	for (uint32_t sector = sectorIndex; sector < params.BPB_SecPerClus; sector++)
	{
		for (uint32_t i = byteIndex; i < 512; i++)
		{
			sdBuffer[i] = data[byteCnt++];
			if (byteCnt == len)
			{
				moreData = false;
				break;
			}
		}
		byteIndex = 0;

		if (!SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(currentCluster) + sector, sdBuffer))
		{
			return false;
		}

		// Update file size and metadata if all data has been written
		if (!moreData)
		{
			pFile->DIR_FileSize = (pFile->accessMode & FA_WRITE) ? len : pFile->DIR_FileSize + len;

			// Read the file entry
			if (SDFAT32_SD_READ_SECTOR(startSectorOfCluster(pFile->fileEntInf.Cluster) + pFile->fileEntInf.sectorIndex, sdBuffer))
			{
				file *p_temp = (file *)(sdBuffer + pFile->fileEntInf.entryIndex * 32);

				// Update the file entry
				memcpy(p_temp, pFile, 32);

				// Write the file entry
				if (SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(pFile->fileEntInf.Cluster) + pFile->fileEntInf.sectorIndex, sdBuffer))
				{
					return true;
				}
			}
			return false;
		}
	}
	return true;
}

/**
 * @brief Deletes a file from the specified path.
 *
 * This function deletes a file from the file system by marking its directory
 * entry as deleted and clearing its clusters in the FAT table.
 *
 * @param[in] path Path to the directory containing the file.
 * @param[in] filename Name of the file to be deleted.
 * @return true if the file is successfully deleted, false otherwise.
 */
bool fileDelete(const char *path, const char *filename)
{
	file pathDir;
	// Check if the path exists
	file tempFile = pathExists(path);
	if (fileStartCluster(&tempFile) == 0)
	{
		PRINTF("Invalid path!");
		return false;
	}

	pathDir = tempFile;

	// Check if the file exists
	tempFile = fileExists(&pathDir, filename);
	if (fileStartCluster(&tempFile) == 0)
	{
		PRINTF("File doesn't exist!");
		return false;
	}

	uint8_t lfnEntCnt = tempFile.fileEntInf.LFN_EntCnt;

	// Read the sector containing the file entry
	if (SDFAT32_SD_READ_SECTOR(startSectorOfCluster(tempFile.fileEntInf.Cluster) + tempFile.fileEntInf.sectorIndex, sdBuffer))
	{
		// Mark LFN and directory entries as deleted
		for (uint8_t i = 0; i < (lfnEntCnt + 1); i++)
		{
			file *p_temp = (file *)(sdBuffer + (tempFile.fileEntInf.entryIndex - i) * 32);
			p_temp->DIR_Name[0] = 0xE5;
		}

		// Write back the modified sector
		if (SDFAT32_SD_WRITE_SECTOR(startSectorOfCluster(tempFile.fileEntInf.Cluster) + tempFile.fileEntInf.sectorIndex, sdBuffer))
		{
			// Clear the file's clusters in the FAT table
			uint32_t currentCluster = fileStartCluster(&tempFile);
			uint32_t tempCluster;
			while (currentCluster < FAT_EOC)
			{
				tempCluster = currentCluster;
				currentCluster = fatNextCluster(currentCluster);
				fatSetNextClus(tempCluster, 0x00000000);
			}
			return true;
		}
		return false;
	}
	return false;
}

/**
 * @brief Initializes the FAT32 file system
 * @return true if success, false otherwise
 * @details
 * This function initializes the FAT32 file system. It reads the boot sector, checks if the card is a FAT32 card,
 * and then computes the FAT and root directory start sectors and cluster count. It also prints the card size and
 * volume label.
 */
bool sdFat32Init()
{
	if (!SDFAT32_SD_INIT())
	{
		PRINTF("Storage initialization failed\n");
		return false;
	}

	if (SDFAT32_SD_SECTOR_SIZE() != 512U)
	{
		PRINTF("Unsupported sector size: %lu (expected 512)\n", (unsigned long)SDFAT32_SD_SECTOR_SIZE());
		return false;
	}

	if (SDFAT32_SD_READ_SECTOR(0, sdBuffer))
	{
		if (mbrGetPartitionStartLba(sdBuffer, 0, &bootSecStartLba))
		{
			fsInfoSecLba = bootSecStartLba + 1;
		}
		else
		{
			PRINTF("No FAT32 partition found\n");
			return false;
		}
	}
	else
	{
		return false;
	}

	if (getBootSecParams())
	{
		// Calculate FAT start sector
		FatStartSector = bootSecStartLba + params.BPB_RsvdSecCnt; // 0X2020

		// Calculate FAT sectors count
		FatSectorsCnt = params.BPB_FATSz32 * params.BPB_NumFATs;

		// Calculate root directory start sector
		RootDirStartSector = FatStartSector + FatSectorsCnt;

		// Calculate root directory sectors count
		RootDirSectors = (32 * params.BPB_RootEntCnt + params.BPB_BytesPerSec - 1) / params.BPB_BytesPerSec; // 0 for FAT32

		// Calculate data start sector
		DataStartSector = RootDirStartSector + RootDirSectors; // 0X96AE

		// Calculate data sectors count
		DataSectorsCnt = params.BPB_TotSec32 - DataStartSector;

		// Calculate cluster count
		uint32_t clusterCnt = DataSectorsCnt / params.BPB_SecPerClus;

		if (clusterCnt < 65526)
		{
			PRINTF("Not a FAT32 Card\n");
			return false;
		}

		// Calculate card size
		float size = (params.BPB_TotSec32 * 512.0) / (1024.0 * 1024.0 * 1024.0);
		uint16_t sizeInt = size;
		float tmpFrac = size - sizeInt;
		uint16_t tmpInt = tmpFrac * 100;

		// Print card size and volume label
		PRINTF("Card Size=%d.%d GB\n", sizeInt, tmpInt);
		PRINTF("Volume Label: %s\n", params.BS_VolLab);

		return true;
	}
	return false;
}
