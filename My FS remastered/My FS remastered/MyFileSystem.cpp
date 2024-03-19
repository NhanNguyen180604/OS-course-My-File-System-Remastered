#include "MyFileSystem.h"

std::string MyFileSystem::GenerateHash(const std::string& data)
{
    using namespace CryptoPP;

    byte *secret = new byte[data.size()];
    size_t secretLen = data.size();
    for (int i = 0; i < secretLen; i++)
    {
        secret[i] = data[i];
    }
    
    byte salt[] = "Sodium Cloride";
    size_t saltLen = strlen((const char*)salt);

    byte derived[SHA256::DIGESTSIZE];

    PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
	byte unused = 0;

    pbkdf.DeriveKey(derived, sizeof(derived), unused, secret, secretLen, salt, saltLen, 10000, 0.0f);

    std::string result(derived, derived + sizeof(derived));

    delete[] secret;
    return result;
}

void MyFileSystem::EncryptData(std::string& data, const std::string& key, Entry *&entry)
{
    if (data.size() == 0)
    {
        std::cout << "Program does not encrypt empty file!\n";
        return;
    }
    using namespace CryptoPP;

    //24 bytes
    const byte iv[] = 
    {
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x58
    };
    
    byte *encrypted = new byte[data.size()];

    XChaCha20Poly1305::Encryption enc;
    enc.SetKeyWithIV((byte*)key.c_str(), key.size(), iv, sizeof(iv));
    enc.EncryptAndAuthenticate(encrypted, entry->mac, sizeof(entry->mac), iv, sizeof(iv), nullptr, 0, (byte*)data.c_str(), data.size());

    data = std::string(encrypted, encrypted + data.size());
    delete[] encrypted;
}

void MyFileSystem::DecryptData(std::string& data, const std::string& key, Entry *&entry)
{
    if (data.size() == 0)
        return;

    using namespace CryptoPP;

    //24 bytes
    const byte iv[] = 
    {
        0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x58
    };
    
    byte *decrypted = new byte[data.size()];

    XChaCha20Poly1305::Decryption dec;
    dec.SetKeyWithIV((byte*)key.c_str(), key.size(), iv, sizeof(iv));
    dec.DecryptAndVerify(decrypted, entry->mac, sizeof(entry->mac), iv, sizeof(iv), nullptr, 0, (byte*)data.c_str(), data.size());

    data = std::string(decrypted, decrypted + data.size());
    delete[] decrypted;
}

void MyFileSystem::CreateFSPassword()
{    
    //write password byte in boot sector
    f.seekp(10, f.beg);
    bool hasPassword = true;
    f.write((char*)&hasPassword, 1);
    this->hasPassword = hasPassword;
    
    //input password
    std::string password;
    std::cout << "Enter new password: ";
    std::cin >> password;
    std::cin.ignore();
    std::cout << "Your password for the file system will be: " << password << '\n';
    std::string hashedPassword = GenerateHash(password);

    //write hashed password to volume
    f.seekp(32, f.beg);
    f.write(hashedPassword.c_str(), hashedPassword.size());

    f.flush();
}

bool MyFileSystem::CheckFSPassword(const std::string& password)
{
    f.seekg(32, f.beg);
    std::string encrypedPassword(32, 0);
    f.read(&encrypedPassword[0], 32);

    return encrypedPassword == GenerateHash(password);
}

void MyFileSystem::ChangeFSPassword()
{
    std::string password;
    std::cout << "Enter file system's password: ";
    std::cin >> password;
    std::cin.ignore();

    if (!CheckFSPassword(password))
    {
        std::cout << "Incorrect password!\n";
        return;
    }

    CreateFSPassword();
}

bool MyFileSystem::CheckFSPassword()
{
    if (hasPassword)
    {
        int attemp = 3;
        std::string password;
        do
        {
            std::cout << "Enter password to access file system: ";
            std::cin >> password;
            std::cin.ignore();
            if (!CheckFSPassword(password))
            {
                std::cout << "Incorrect password!\n";
                attemp--;
                std::cout << "Number of attemps left: " << attemp << '\n';
            }
            else return true;;
        } while (attemp);

        if (attemp == 0)
            return false;
    }
    return true;
}

MyFileSystem::MyFileSystem()
{
    f.open(FS_PATH, std::ios::binary | std::ios::in | std::ios::out);
    //read boot sector
    f.read((char*)&infos.bytesPerSector, 2);
    f.read((char*)&infos.sectorsPerCluster, 1);
    f.read((char*)&infos.sectorsBeforeFat, 1);
    f.seekg(6, f.beg);
    f.read((char*)&infos.volumeSize, 4);
    f.read((char*)&hasPassword, 1);
}

MyFileSystem::~MyFileSystem()
{
    f.close();
}

void MyFileSystem::WriteFileContent(const std::string& data, const std::vector<unsigned int>& clusters)
{
    int i = 0;
    const unsigned int clusterSize = infos.bytesPerSector * infos.sectorsPerCluster;
    for (unsigned int cluster : clusters)
    {
        unsigned int sectorOffset = infos.sectorsBeforeFat + FAT::fatSize + (cluster - STARTING_CLUSTER) * infos.sectorsPerCluster;
        unsigned int bytesOffset = sectorOffset * infos.bytesPerSector;
        f.seekp(bytesOffset, f.beg);
        size_t size = ((size_t)clusterSize < data.size() - i) ? clusterSize : data.size() - i;
        f.write(&data[i], size);
        i += clusterSize;
    }
    f.flush();
}

std::string MyFileSystem::ReadFileContent(unsigned int fileSize, const std::vector<unsigned int>& clusters)
{
    std::string data(fileSize, 0);
    int i = 0;
    const unsigned int clusterSize = infos.bytesPerSector * infos.sectorsPerCluster;
    for (unsigned int cluster : clusters)
    {
        unsigned int sectorOffset = infos.sectorsBeforeFat + FAT::fatSize + (cluster - STARTING_CLUSTER) * infos.sectorsPerCluster;
        unsigned int bytesOffset = sectorOffset * infos.bytesPerSector;
        f.seekg(bytesOffset, f.beg);
        size_t size = ((size_t)clusterSize < data.size() - i) ? clusterSize : data.size() - i;
        f.read(&data[i], size);
        i += clusterSize;
    }
    return data;
}

void MyFileSystem::ImportFile(const std::string& inputPath, bool hasPassword)
{
    std::ifstream fin(inputPath, std::ios::binary | std::ios::in);
    if (!fin)
    {
        std::cout << "Path does not exist\n";
        return;
    }

    fin.seekg(0, f.end);
    unsigned int fileSize = fin.tellg();
    fin.clear();
    fin.seekg(0, f.beg);

    //check file size limit
    unsigned int limit = infos.bytesPerSector * infos.sectorsPerCluster * (NUMBER_OF_CLUSTERS - 1);
    if (fileSize > limit)
    {
        std::cout << "File's size is too large!\n";
        fin.close();
        return;
    }

    //get name
    std::string fileName = inputPath.substr(inputPath.find_last_of("/\\") + 1);
    std::string extension = fileName.substr(fileName.find_last_of(".") + 1);
    fileName = fileName.substr(0, fileName.find_last_of("."));
    Entry *entry = new Entry();
    //create short name
    entry->SetExtension(extension);
    unsigned int number = 1;
    entry->SetName(fileName, number);
    while(RDET::CheckDuplicateName(f, infos, entry))
    {
        number++;
        entry->SetName(fileName, number, true);
    }
    //get file size
    entry->fileSize = fileSize;

    //find free cluster and write to FAT
    //calculate how many clusters needed
    unsigned int clustersNeeded = (unsigned int)(std::ceil((double)entry->fileSize / (infos.bytesPerSector * infos.sectorsPerCluster)));
    if (clustersNeeded == 0)
        clustersNeeded++;
    std::vector<unsigned int> freeClusters = FAT::GetFreeClusters(f, infos, clustersNeeded);
    if (freeClusters.empty())
    {
        std::cout << "Out of clusters for file!\n";
        delete entry;
        fin.close();
        return;
    }
    entry->startingCluster = freeClusters[0];
    FAT::WriteClustersToFAT(f, infos, freeClusters);

    //read the whole file into memory
    std::string fileData(entry->fileSize, 0);
    fin.read(&fileData[0], entry->fileSize);

    //Create file password and encrypt file's content
    if (hasPassword)
    {
        entry->hasPassword = true;
        //set password's hash
        std::string password;
        std::cout << "Enter file's password: ";
        std::cin >> password;

        std::string hashedPassword = GenerateHash(password);
        std::string doublyHashedPassword = GenerateHash(hashedPassword);
        entry->SetHash(doublyHashedPassword);

        EncryptData(fileData, hashedPassword, entry);
    }

    //write entries
    if (!RDET::WriteFileEntry(f, infos, entry))
    {
        std::cout << "Out of space for file entry!\n";
        delete entry;
        fin.close();
        return;
    }

    //write file's content
    WriteFileContent(fileData, freeClusters);
    
    delete entry;
    fin.close();
}

void MyFileSystem::ImportFile()
{
    std::string path;
    std::cout << "Enter file path: ";
    std::getline(std::cin, path);

    bool hasPassword;
    std::cout << "Do you want to set password? (1 - yes/0 - no): ";
    std::cin >> hasPassword;
    std::cin.ignore();

    ImportFile(path, hasPassword);
}

bool MyFileSystem::CheckFilePassword(Entry *&entry, std::string& filePassword)
{
    std::string hashedPassword(entry->hashedPassword, entry->hashedPassword + 32);
    std::cout << "Enter file's password: ";
    std::cin >> filePassword;
    std::cin.ignore();

    std::string key = GenerateHash(filePassword);
    if (GenerateHash(key) != hashedPassword)
    {
        std::cout << "Incorrect password!\n";
        return false;
    }

    return true;
}

void MyFileSystem::ChangeFilePassword(Entry *&entry, unsigned int offset, bool removed)
{
    std::vector<unsigned int> fileClusters = FAT::GetClustersChain(f, infos, entry->startingCluster);
    std::string fileData = ReadFileContent(entry->fileSize, fileClusters);
    if (entry->hasPassword)
    {
        std::string filePassword;
        if (!CheckFilePassword(entry, filePassword))
        {
            std::cout << "Incorrect password!\n";
            return;
        }

        DecryptData(fileData, GenerateHash(filePassword), entry);
    }

    if (!removed)
    {
        std::string newPassword;
        std::cout << "Enter file's new password: ";
        std::cin >> newPassword;
        std::cin.ignore();

        std::string hashedPassword = GenerateHash(newPassword);
        std::string doublyHashedPassword = GenerateHash(hashedPassword);
        entry->SetHash(doublyHashedPassword);

        EncryptData(fileData, hashedPassword, entry);
        entry->hasPassword = true;
    }
    else entry->hasPassword = false;
    
    //rewrite data
    WriteFileContent(fileData, fileClusters);

    //rewrite file entry
    f.seekp(offset, f.beg);
    f.write((char*)entry, sizeof(Entry));
    f.flush();
}

void MyFileSystem::ChangeFilePassword()
{
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file!\n";
        return;
    }
    PrintFileList(fileList);

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to create/change password: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());

    //read entry
    f.seekg(fileList[choice - 1].second);
    Entry *e = new Entry();
    f.read((char*)e, sizeof(Entry));

    bool removed = false;
    if (e->hasPassword)
    {
        std::cout << "Do you want to remove this file's password (1 - yes/0 - no): ";
        std::cin >> removed;
        std::cin.ignore();
    }
    
    ChangeFilePassword(e, fileList[choice - 1].second, removed);

    //free memory
    delete e;
}

void MyFileSystem::ExportFile(const std::string& outputPath, Entry *&entry)
{
    std::vector<unsigned int> fileClusters = FAT::GetClustersChain(f, infos, entry->startingCluster);
    std::string fileData = ReadFileContent(entry->fileSize, fileClusters);

    if (entry->hasPassword)
    {
        std::string filePassword;
        if (!CheckFilePassword(entry, filePassword))
            return;

        DecryptData(fileData, GenerateHash(filePassword), entry);
    }

    std::string fileName = entry->GetFullName();
    std::ofstream fout(outputPath + fileName, std::ios::binary | std::ios::out);
    fout.write(&fileData[0], fileData.size());
    fout.close();
}

void MyFileSystem::ExportFile()
{
    //list file
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file!\n";
        return;
    }
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to export: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());
    
    std::string path;
    std::cout << "Enter output path: ";
    std::getline(std::cin, path);
    if (path[path.size() - 1] != '\\')
        path += '\\';

    //read entry
    f.seekg(fileList[choice - 1].second);
    Entry *e = new Entry();
    f.read((char*)e, sizeof(Entry));
    
    //export
    ExportFile(path, e);

    //free memory
    delete e;
}

std::vector<std::pair<std::string, unsigned int>> MyFileSystem::GetFileList(bool getDeleted) {
    std::vector<std::pair<std::string, unsigned int>> fileList;
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
            if (tempEntry.name[0] == -27 && getDeleted)
            {
                //if the file is not restorable
                if (tempEntry.reserved[0] == 0)
                    continue;

                //if the file is restorable
                std::string fileName = tempEntry.GetInfo();
                fileName[0] = tempEntry.reserved[0];
                fileList.push_back(std::make_pair(fileName, bytesOffset));
            }
            //sign of empty entry
            else if (tempEntry.name[0] == 0)
            {
                return fileList;
            }

            if (tempEntry.name[0] != -27 && !getDeleted)
                fileList.push_back(std::make_pair(tempEntry.GetInfo(), bytesOffset));
        }
    }
    return fileList;
}

void MyFileSystem::PrintFileList(const std::vector<std::pair<std::string, unsigned int>>& fileList)
{
    int i = 1;
    for (std::pair<std::string, int> p : fileList)
    {
        std::cout << i++ << ". " << p.first << '\n';
    }
}

void MyFileSystem::ListFiles() 
{
    std::vector<std::pair<std::string, unsigned int>> fileList = MyFileSystem::GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file\n";
        return;
    }
    PrintFileList(fileList);
}

void MyFileSystem::MyDeleteFile(unsigned int bytesOffset, bool restorable) 
{
    f.seekg(bytesOffset, f.beg);
    Entry e;
    f.read((char*)&e, sizeof(Entry));

    //check file password
    if (e.hasPassword)
    {
        Entry *pE = &e;
        std::string password;
        if (!CheckFilePassword(pE, password))
        {
            std::cout << "Incorrect password!\n";
            return;
        }
    }

    unsigned char deleteValue = 0xE5;
    unsigned char trueValue = e.name[0];
    //store first byte of name if restorable
    if (restorable)
    {
        f.seekp(bytesOffset + ENTRY_NAME_SIZE + FILE_EXTENSION_LENGTH, f.beg);
        f.write((char*)&trueValue, sizeof(trueValue));
    }
    //mark first byte as E5
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&deleteValue, sizeof(deleteValue));

    //remove from FAT if not restorable
    if(!restorable)
    {
        std::vector<unsigned int> fileClusters = FAT::GetClustersChain(f, infos, e.startingCluster);
        for (unsigned int cluster : fileClusters)
        {
            unsigned int fatOffset = infos.sectorsBeforeFat * infos.bytesPerSector + cluster * FAT::fatEntrySize; //in bytes
            f.seekp(fatOffset, f.beg);
            unsigned int empty = 0;
            f.write((char*)&empty, FAT::fatEntrySize);
        }
    }
    f.flush();
}

void MyFileSystem::MyDeleteFile()
{
    //list file
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList();
    if (fileList.empty())
    {
        std::cout << "There is no file!\n";
        return;
    }
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to delete: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());

    unsigned int bytesOffset = fileList[choice - 1].second;

    bool restorable;
    std::cout << "Do you want this to be restorable (1 - yes/0 - no): ";
    std::cin >> restorable;
    std::cin.ignore();
    MyDeleteFile(bytesOffset, restorable);
}

void MyFileSystem::RestoreFile(unsigned int bytesOffset) 
{
    f.seekg(bytesOffset, f.beg);
    Entry e;
    Entry *pE = nullptr;
    f.read((char*)&e, sizeof(Entry));
    pE = &e;

    e.name[0] = e.reserved[0];
    e.reserved[0] = 0;

    //check duplicate name
    int number = std::atoi(e.GetIndex().c_str());
    std::string fileName(e.name, e.name + e.nameLen);
    while (RDET::CheckDuplicateName(f, infos, pE))
    {
        number++;
        e.SetName(fileName, number, true);
    }

    //rewrite entry
    f.seekp(bytesOffset, f.beg);
    f.write((char*)&e, sizeof(Entry));
    f.flush();
}

void MyFileSystem::MyRestoreFile() 
{
    std::vector<std::pair<std::string, unsigned int>> fileList = GetFileList(true);
    if (fileList.size() == 0)
    {
        std::cout << "There is no file to restore!\n";
        return;
    }

    std::cout << "Restorable Deleted File List:\n";
    PrintFileList(fileList);
    std::cout << '\n';

    //choose file
    int choice;
    do
    {
        std::cout << "Enter which file to restore: ";
        std::cin >> choice;
        std::cin.ignore();
    } while (choice <= 0 || choice > fileList.size());

    unsigned int bytesOffset = fileList[choice - 1].second;
    RestoreFile(bytesOffset);
}

void MyFileSystem::HandleInput()
{
    char choice;
    while (true)
    {
        std::cout << '\n';
        std::cout << "1. Create/Change File System's password\n";
        std::cout << "2. List files\n";
        std::cout << "3. Create/Change a file's password\n";
        std::cout << "4. Import a file\n";
        std::cout << "5. Export a file\n";
        std::cout << "6. Delete a file\n";
        std::cout << "7. Restore a file\n";
        std::cout << "Q. Quit\n";
        std::cout << "\nEnter your choice: ";
        std::cin >> choice;
        std::cin.ignore();

        switch (choice)
        {
            case '1':
            {
                if (this->hasPassword)
                    ChangeFSPassword();
                else CreateFSPassword();
                break;
            }
            case '2':
            {
                ListFiles();
                break;
            }
            case '3':
            {
                ChangeFilePassword();
                break;
            }
            case '4':
            {
                ImportFile();
                break;
            }
            case '5':
            {
                ExportFile();
                break;
            }
            case '6':
            {
                MyDeleteFile();
                break;
            }
            case '7':
            {
                MyRestoreFile();
                break;
            }
            default:
            {
                return;
            }
        }
    }
}