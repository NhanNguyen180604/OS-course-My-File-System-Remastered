#include "MyFileSystem.h"

void Entry::SetExtension(const std::string& fileExtension)
{
    for (int i = 0; i < FILE_EXTENSION_LENGTH; i++)
        extension[i] = fileExtension[i];
}

void Entry::SetName(const std::string& fileName, unsigned int number, bool adjust)
{
    if (!adjust)
    {
        std::string numberStr = std::to_string(number);
        int size = ENTRY_NAME_SIZE - numberStr.size();
        //fill number
        for (int i = size; i < ENTRY_NAME_SIZE; i++)
            name[i] = numberStr[i - size];
        name[size - 1] = '~';

        int i = 0;
        for (; fileName[i] != '\0' && i < size - 1; i++)
        {
            name[i] = fileName[i];
        }
        nameLen = i;
        //fill with spaces
        while (i < size - 1)
        {
            name[i++] = ' ';
        }
    }
    //this branch doesnt account for when new number has fewer digits than old number
    else
    {
        int i = 1;
        for (int j = ENTRY_NAME_SIZE - 2; j > -1; j--)
        {
            if (name[j] == '~')
            {
                i = j;
                break;
            } 
        }
        unsigned int oldNumber = std::atoi(std::string(name + i + 1, name + ENTRY_NAME_SIZE).c_str());
        nameLen = std::min(i, nameLen);
        i -= std::to_string(number).size() - std::to_string(oldNumber).size();

        name[i] = '~';
        std::string newNumberStr = std::to_string(number);
        for (int j = i + 1; j < ENTRY_NAME_SIZE; j++)
        {
            name[j] = newNumberStr[j - i - 1];
        }
    }
}

std::string Entry::GetFullName() const
{
    std::string result(name, name + nameLen);
    result += '.' + std::string(extension, extension + FILE_EXTENSION_LENGTH);
    return result;
}

std::string Entry::GetIndex() const
{
    int i = ENTRY_NAME_SIZE;
    while (name[i] != '~')
        i--;
    return std::string(name + i + 1, name + ENTRY_NAME_SIZE).c_str();
}

void Entry::SetHash(const std::string& hash)
{
    for (int i = 0; i < 32; i++)
    {
        hashedPassword[i] = hash[i];
    }
}

std::string Entry::GetInfo() const
{
    double size = fileSize;
    int i = 0;
    std::string result = GetFullName() + "  (" + GetIndex() + ")  ";
    while (size >= 1000)
    {
        size /= 1000;
        i++;
    }

    int precision = 2;
    std::ostringstream out;
    out.precision(precision);
    out << std::fixed << size;
    result += out.str();

    if (i == 0)
        result += " B";
    else if (i == 1)
        result += " KB";
    else result += " MB";
    return result;
}

bool RDET::CheckDuplicateName(std::fstream& f, const FSInfo& infos, Entry *&entry)
{
    std::vector<unsigned int> rdetClusters = FAT::GetClustersChain(f, infos, STARTING_CLUSTER);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = infos.sectorsBeforeFat + FAT::fatSize + infos.sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * infos.bytesPerSector;
        unsigned int limitOffset = bytesOffset + infos.bytesPerSector * infos.sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            Entry tempEntry;
            f.seekg(bytesOffset, f.beg);
            f.read((char*)&tempEntry, sizeof(Entry));
            //sign of erased file
            if (tempEntry.name[0] == -27)
                continue;
            //sign of empty entry
            if (tempEntry.name[0] == 0)
                return false;
            //if duplicated
            if (tempEntry.GetFullName() == entry->GetFullName() && tempEntry.GetIndex() == entry->GetIndex())
                return true;
        }
    }
    return false;
}

bool RDET::WriteFileEntry(std::fstream& f, const FSInfo& infos, Entry *&entry)
{
    std::vector<unsigned int> rdetClusters = FAT::GetClustersChain(f, infos, STARTING_CLUSTER);
    for (unsigned int cluster : rdetClusters)
    {
        unsigned int sectorOffset = infos.sectorsBeforeFat + FAT::fatSize + infos.sectorsPerCluster * (cluster - STARTING_CLUSTER);
        unsigned int bytesOffset = sectorOffset * infos.bytesPerSector;
        unsigned int limitOffset = bytesOffset + infos.bytesPerSector * infos.sectorsPerCluster;  //start of next cluster
        for (; bytesOffset < limitOffset; bytesOffset += sizeof(Entry))
        {
            f.seekg(bytesOffset, f.beg);
            Entry tempEntry;
            f.read((char*)&tempEntry, sizeof(Entry));
            if (tempEntry.name[0] == 0 || (tempEntry.name[0] == -27 && tempEntry.reserved[0] == 0))
            {
                f.seekp(bytesOffset, f.beg);
                f.write((char*)entry, sizeof(Entry));
                f.flush();
                return true;
            }
        }
    }
    
    //if out of space, append another cluster for RDET
    std::vector<unsigned int> newFreeCluster = FAT::GetFreeClusters(f, infos, 1); 
    if (newFreeCluster.empty())
        return false;

    //write to FAT new cluster of RDET
    unsigned int offset = infos.sectorsBeforeFat * infos.bytesPerSector + rdetClusters[rdetClusters.size() - 1] * FAT::fatEntrySize;
    f.seekp(offset, f.beg);
    f.write((char*)&newFreeCluster[0], FAT::fatEntrySize);
    offset = infos.sectorsBeforeFat * infos.bytesPerSector + newFreeCluster[0] * FAT::fatEntrySize;
    f.seekp(offset, f.beg);
    unsigned int eof = MY_EOF;
    f.write((char*)&eof, FAT::fatEntrySize);

    //write entry to new cluster
    unsigned int sectorOffset = infos.sectorsBeforeFat + FAT::fatSize + infos.sectorsPerCluster * (newFreeCluster[0] - STARTING_CLUSTER);
    unsigned int bytesOffset = sectorOffset * infos.bytesPerSector;
    f.seekp(bytesOffset, f.beg);
    f.write((char*)entry, sizeof(Entry));
    f.flush();
    return true;
}