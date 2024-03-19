#include "MyFileSystem.h"

std::vector<unsigned int> FAT::GetClustersChain(std::fstream& f, const FSInfo& infos, unsigned int startingCluster)
{
    std::vector<unsigned int> result;
    result.push_back(startingCluster);
    f.seekg(infos.sectorsBeforeFat * infos.bytesPerSector + startingCluster * fatEntrySize, f.beg);
    unsigned int nextCluster = 0;
    f.read((char*)&nextCluster, fatEntrySize);
    if (nextCluster == 0)
        return {};
    while (nextCluster != MY_EOF)
    {
        result.push_back(nextCluster);
        f.seekg(infos.sectorsBeforeFat * infos.bytesPerSector + nextCluster * fatEntrySize, f.beg);
        f.read((char*)&nextCluster, fatEntrySize);
    }
    return result;
}

std::vector<unsigned int> FAT::GetFreeClusters(std::fstream& f, const FSInfo& infos, unsigned int n)
{
    std::vector<unsigned int> result;
    //cluster for actual file's data starts from 3
    unsigned int cluster = STARTING_CLUSTER + 1;
    f.seekp(infos.sectorsBeforeFat * infos.bytesPerSector + cluster * fatEntrySize, f.beg);
    while (n && cluster <= FINAL_CLUSTER)
    {
        unsigned int nextCluster = 0;
        f.read((char*)&nextCluster, fatEntrySize);
        if (nextCluster == 0)
        {
            n--;
            result.push_back(cluster);
        }
        cluster++;
    }
    if (n)
        return {};
    return result;
}

void FAT::WriteClustersToFAT(std::fstream& f, const FSInfo& infos, const std::vector<unsigned int>& clusters)
{
    int i = 0;
    while (i < clusters.size() - 1)
    {
        unsigned int offset = infos.sectorsBeforeFat * infos.bytesPerSector + clusters[i] * fatEntrySize;
        f.seekp(offset, f.beg);
        f.write((char*)&clusters[i + 1], fatEntrySize);
        i++;
    }
    unsigned int data = MY_EOF;
    unsigned int offset = infos.sectorsBeforeFat * infos.bytesPerSector + clusters[i] * fatEntrySize;
    f.seekp(offset);
    f.write((char*)&data, fatEntrySize);
    f.flush();
}