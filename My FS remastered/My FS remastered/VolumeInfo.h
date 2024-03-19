#pragma once
#define FS_PATH "D:\\MyFS.Dat"
#define BYTES_PER_SECTOR 512  //2 bytes
#define SECTORS_PER_CLUSTER 4  //1 byte
#define SECTORS_BEFORE_FAT 1 //1 byte
#define FAT_SIZE 4089  //2 byte, each entry in FAT takes up 4 bytes, do math to get this number
#define FAT_ENTRY_SIZE 4 //size in bytes
#define FREE 0 //free cluster value in FAT
#define MY_EOF 268435455  //EOF cluster value in FAT
#define VOLUME_SIZE 2097152 //4 bytes, size in sector
#define STARTING_CLUSTER 2
#define NUMBER_OF_CLUSTERS 523265 //size in sector, do math to get this number
#define FINAL_CLUSTER 523266  //cluster starts at 2
#define ENTRY_NAME_SIZE 48
#define FILE_EXTENSION_LENGTH 4