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
#ifndef __SD_FAT32_H
#define __SD_FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * Select SD backend:
 *  - SDFAT32_SD_BACKEND_SDMMC : SDMMC host controller
 *  - SDFAT32_SD_BACKEND_SPI   : SD card over SPI
 *  - SDFAT32_SD_BACKEND_USB_MSC : USB flash drive via USB MSC host
 */
#define SDFAT32_SD_BACKEND_SDMMC 0
#define SDFAT32_SD_BACKEND_SPI 1
#define SDFAT32_SD_BACKEND_USB_MSC 2

#ifndef SDFAT32_SD_BACKEND
#define SDFAT32_SD_BACKEND SDFAT32_SD_BACKEND_SDMMC
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* Legacy defaults; runtime code now auto-detects FAT boot sector from MBR/superfloppy. */


#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_FILE_NAME 0x0F

#define ATTR_LONG_NAME_MASK (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE)

#define FAT_EOC 0x0FFFFFF8

// File access mode
#define FA_READ 0x1   ///< File access mode read
#define FA_WRITE 0x2  ///< File access mode write
#define FA_APPEND 0x4 ///< File access mode append

    typedef struct
    {
        uint32_t Cluster;
        uint32_t entryIndex;
        uint32_t lfnEntryCluster;
        uint32_t lfnEntryIndex;
        uint8_t sectorIndex;
        uint8_t LFN_EntCnt;
        uint8_t lfnEntrySectorIndex;

    } fileEntInf_t;

    typedef struct
    {
        uint32_t FSI_LeadSig;
        uint8_t FSI_Reserved1[480];
        uint32_t FSI_StrucSig;
        uint32_t FSI_Free_Count;
        uint32_t FSI_Nxt_Free;
        uint8_t FSI_Reserved2[12];
        uint32_t FSI_TrailSig;

    } FSInfo_t;

    typedef struct
    {
        uint32_t fatSecNum;
        uint16_t fatEntOffset;
    } fatEntLoc_t;

    typedef fileEntInf_t freeEntInf_t;

    typedef struct
    {
        uint8_t LDIR_Ord;
        char LDIR_Name1[10];
        uint8_t LDIR_Attr;
        uint8_t LDIR_Type;
        uint8_t LDIR_Chksum;
        char LDIR_Name2[12];
        uint16_t LDIR_FstClusLO;
        char LDIR_Name3[4];
    } LFN_entry_t;

    typedef struct
    {
        uint16_t BPB_BytesPerSec;
        uint8_t BPB_SecPerClus;
        uint16_t BPB_RsvdSecCnt;
        uint8_t BPB_NumFATs;
        uint16_t BPB_RootEntCnt;
        uint32_t BPB_TotSec32;
        uint32_t BPB_FATSz32;
        uint32_t BPB_RootClus;
        uint16_t BPB_FSInfo;
        char BS_VolLab[12];
    } bootSecParams_t;

    typedef struct
    {
        char DIR_Name[8];
        char DIR_ext[3];
        uint8_t DIR_attr;
        uint8_t DIR_NTRes;
        uint8_t DIR_CrtTimeTenth;
        uint16_t DIR_CrtTime;
        uint16_t DIR_CrtDate;
        uint16_t DIR_LstAccDate;
        uint16_t DIR_FstClusHI;
        uint16_t DIR_WrtTime;
        uint16_t DIR_WrtDate;
        uint16_t DIR_FstClusLO;
        uint32_t DIR_FileSize;
        uint32_t entryIndex; ///< For directory, it indicates index of file entry. For file, it indicates index of  file contents in bytes
        fileEntInf_t fileEntInf;
        uint8_t accessMode;
    } file;

    bool sdFat32Init();

    file fileOpen(const char *path, const char *filename, uint8_t accessMode);

    uint32_t fileRead(file *pFile, uint8_t *buffer, uint32_t len);

    bool fileWrite(file *pFile, const uint8_t *data, uint32_t len);

    bool fileDelete(const char *path, const char *filename);

    file fileGetNext(file *pFolder);

    char *fileGetName(file *pFile);

    bool fileIsValid(file *pFile);

    file createDirectory(const char *path, const char *dirName);

    file openDirectory(const char *path);

    bool listDirectory(const char *path);

    void listDirectoryRecursive(file *pFolder, uint8_t tab);

    /**
     * @brief Check if a file represents the end of a directory
     * @param pFile Pointer to file structure
     * @return true if the file represents the end of a directory, false otherwise
     *
     * This function checks the first character of the file name to determine if the
     * file represents the end of a directory by verifying that the first character is 0.
     */
    static inline bool fileIsEndOfDirectory(file *pFile)
    {
        return ((uint8_t)(pFile->DIR_Name[0]) == 0);
    }

    /**
     * @brief Check if the given file is a directory
     *
     * This function checks the file attributes to determine if the
     * file is a directory by verifying the directory and volume ID bits.
     *
     * @param pFile Pointer to file structure
     * @return true if the file is a directory, false otherwise
     */
    static inline bool fileIsDirectory(file *pFile)
    {
        // Check if the file attribute indicates a directory
        return !(((pFile->DIR_attr & (ATTR_DIRECTORY | ATTR_VOLUME_ID)) == 0));
    }

    /**
     * @brief Get the file size of a file
     * @param pFile Pointer to file structure
     * @return File size in bytes
     *
     * This function returns the file size of a file by accessing the
     * DIR_FileSize member of the file structure.
     */
    static inline uint32_t fileSize(file *pFile)
    {
        return pFile->DIR_FileSize;
    }

    /**
     * @brief Reset the file index to the start of the file or directory
     * @param pFile Pointer to file structure
     *
     * This function resets the file index to the start of the file or directory
     * depending on whether the file is a directory.
     */
    static inline void fileReset(file *pFile)
    {
        if (fileIsDirectory(pFile))
        {
            // Reset the index to the start of the directory
            pFile->entryIndex = 2;
        }
        else
        {
            // Reset the index to the start of the file
            pFile->entryIndex = 0;
        }
    }

    /**
     * @brief Close a file
     * @param pFile Pointer to file structure
     *
     * This function closes a file by resetting the file structure to zero.
     * This is useful when you want to reuse the file structure for another file.
     */
    static inline void fileClose(file *pFile)
    {
        // Reset the file structure to zero
        memset(pFile, 0, sizeof(file));
    }

#ifdef __cplusplus
}
#endif

#endif
