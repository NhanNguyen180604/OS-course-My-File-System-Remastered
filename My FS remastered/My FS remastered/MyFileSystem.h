#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <utility>
#include <iomanip>

#include "VolumeInfo.h"
#include "cryptlib.h"
#include "pwdbased.h"
#include "sha.h"
#include "hex.h"
#include "chachapoly.h"
#include "filters.h"
#include "files.h"

typedef unsigned char byte;

struct FSInfo
{
    unsigned int bytesPerSector;
    unsigned int sectorsPerCluster;
    unsigned int sectorsBeforeFat;
    unsigned int volumeSize;
};

class FAT
{
public:
    static const int fatSize = FAT_SIZE;
    static const int fatEntrySize = FAT_ENTRY_SIZE;

    static std::vector<unsigned int> GetClustersChain(std::fstream& f, const FSInfo& infos, unsigned int startingCluster);
    static std::vector<unsigned int> GetFreeClusters(std::fstream& f, const FSInfo& infos, unsigned int n);
    static void WriteClustersToFAT(std::fstream& f, const FSInfo& infos, const std::vector<unsigned int>& clusters);
};

struct Entry
{
    //name may be shrunk to avoid duplicate
    char name[ENTRY_NAME_SIZE];
    char extension[FILE_EXTENSION_LENGTH];
    //reserved for storing first character of file name when deleted
    char reserved[4] = {0};
    int nameLen;
    unsigned int startingCluster;
    unsigned int fileSize;
    bool hasPassword = 0;
    char padding[11] = {0};
    char hashedPassword[32] = {0};
    byte mac[16] = {0};

    void SetName(const std::string& fileName, unsigned int number, bool adjusted = false);
    std::string GetFullName() const;
    void SetExtension(const std::string& fileExtension);
    std::string GetIndex() const;
    void SetHash(const std::string& hash);
    std::string GetInfo() const;
};
#pragma pack(pop)

class RDET
{
public:
    static bool CheckDuplicateName(std::fstream& f, const FSInfo& infos, Entry *&entry);
    static bool WriteFileEntry(std::fstream& f, const FSInfo& infos, Entry *&entry);
};

class MyFileSystem
{
private:
    std::fstream f;
    FSInfo infos;
    bool hasPassword;

    void CreateFSPassword();
    bool CheckFSPassword(const std::string& password);
    void ChangeFSPassword();

    //write consecutive clusters to FAT
    void WriteFileContent(const std::string& data, const std::vector<unsigned int>& clusters);
    std::string ReadFileContent(unsigned int fileSize, const std::vector<unsigned int>& clusters);

    std::string GenerateHash(const std::string& data);
    void EncryptData(std::string& data, const std::string& key, Entry *&entry);
    void DecryptData(std::string& data, const std::string& key, Entry *&entry);

    std::vector<std::pair<std::string, unsigned int>> GetFileList(bool getDeleted = false);
    void PrintFileList(const std::vector<std::pair<std::string, unsigned int>>& fileList);

    void ImportFile(const std::string& inputPath, bool setPassword = false);
    bool CheckFilePassword(Entry *&entry, std::string& filePassword);
    void ChangeFilePassword(Entry *&entry, unsigned int offset, bool removed = false);
    void ExportFile(const std::string& outputPath, Entry *&entry);
    void RestoreFile(unsigned int bytesOffset);
    void MyDeleteFile(unsigned int bytesOffset, bool restorable = true);

public:
    MyFileSystem();
    ~MyFileSystem();
    
    bool CheckFSPassword();
    void ChangeFilePassword();
    void ImportFile();
    void ExportFile();
    void ListFiles();
    void MyDeleteFile();
    void MyRestoreFile();

    void HandleInput();
};