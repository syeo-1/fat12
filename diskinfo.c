#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "common_functions.h"

int get_sectors_per_fat(char* p) {
    int low_byte = p[22];
    int high_byte = p[23];
    high_byte = high_byte << 8;

    return low_byte + high_byte;
}

int get_num_of_fat_copies(char* p) {
    return p[16];
}

int get_total_disk_size(char* p) {
    int lower = p[19];
    int upper = p[20];
    upper = upper << 8;

    return (upper+lower)*512;
}

char * get_disk_label(char* p) { //use root directory
    static char disk_label[9];

    // start at the appropriate byte
    int i = 19 * 512;
    int disk_label_index = 0;
    while(p[i] != 0x00) {
        int j = i;

        // check attribute for volume label
        int attribute = i+11;
        if (p[attribute] == 8) {
            while(j < i+8) {
                disk_label[disk_label_index] = p[j];
                j++;
                disk_label_index++;
            }
        }
        i+=32;
    }
    
    return disk_label;
}

char * get_os_name(char* p) {
    static char os_name[9];

    int os_name_i = 0;
    for (int i = 3 ; i < 11 ; i++) {
        os_name[os_name_i] = p[i];
        os_name_i++;
    }

    return os_name;
}


int main(int argc, char *argv[]) {
    int fd;
	struct stat sb;

    if (argc == 2) {
        fd = open(argv[1], O_RDWR);
        fstat(fd, &sb);

        char * p = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0); // p points to the starting pos of your mapped memory
        if (p == MAP_FAILED) {
            printf("Error: failed to map memory\n");
            exit(1);
        }
        printf("OS Name: %s\n", get_os_name(p));
        printf("Label of the disk: %s\n", get_disk_label(p));
        printf("Total size of the disk: %d bytes\n", get_total_disk_size(p));
        printf("Free size of the disk: %d bytes\n", get_free_size_on_disk(p));
        printf("==============\n");
        printf("The number of files in the image: %d\n", get_num_files_on_disk(p));
        printf("Number of FAT copies: %d\n", get_num_of_fat_copies(p));
        printf("Sectors per FAT: %d\n", get_sectors_per_fat(p));

    } else {
        printf("incorrect number of arguments given\n");
        exit(1);
    }

    return 0;
}