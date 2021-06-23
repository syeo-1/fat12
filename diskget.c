#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "common_functions.h"

void copy_to_current_dir(char *p, unsigned short first_logic_cluster, char* filename, int filesize) {
    // create the file and set to writing mode
    FILE *fp;
    fp = fopen(filename, "w");

    // first logic cluster will be the first fat entry to look at
    unsigned short current_cluster = first_logic_cluster;
    int fat_entry_value = first_logic_cluster;
    int bytes_copied = 0;
    while (fat_entry_value < 0xff8 && bytes_copied < filesize) {
        // read from the current logic cluster from the data area

        // get the cluster to copy from within the data area
        int sector_num = 33 + current_cluster - 2;

        // copy a max of 512 bytes from the disk image
        for (int i = 0 ; i < 512 ; i++) {
            if (bytes_copied >= filesize) {
                break;
            }
            fprintf(fp, "%c", p[(sector_num*512)+i]);
            bytes_copied++;
        }

        // get the next fat entry using current fat entry value
        fat_entry_value = get_fat_entry_value(p,current_cluster);

        // get the next cluster value to copy using the fat value
        current_cluster = fat_entry_value;

    }

    // close the file after finished copying
    fclose(fp);
}

unsigned short get_first_logic_cluster(char* p) {
    return *(unsigned short *)(p + 26);
}

int get_file_location(char* p, char* actual_filename) {

    int data_area = 33 * 512;
    // go through each directory entry
    for(int i = 0 ; i < data_area ; i+=32) {
        char full_filename[12];
        get_full_filename(p, full_filename, i, 'F');
        if (strcmp(full_filename, actual_filename) == 0) {
            return i;
        }
    }
    return -1;
}

char * capitalize(char* filename) {
    char* caps_filename = malloc(sizeof(char) * (strlen(filename)+1));
    for(int i = 0 ; i < strlen(filename) ; i++) {
        caps_filename[i] = toupper(filename[i]);
    }
    caps_filename[strlen(filename)] = '\0';
    return caps_filename;
}

int main(int argc, char *argv[]) {
    int fd;
	struct stat sb;

    if (argc == 3) {
        fd = open(argv[1], O_RDWR);
        fstat(fd, &sb);

        char * p = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            printf("Error: failed to map memory\n");
            exit(1);
        }

        // convert file name to all caps first
        char * caps_filename = capitalize(argv[2]);

        // check if the filename exists in root directory
        int file_location = get_file_location(p+(19*512), caps_filename);

        if (file_location == -1) {
            printf("file not in root directory\n");
            exit(1);
        }

        // extract the first logical cluster
        unsigned short first_logic_cluster = get_first_logic_cluster(p+((19*512)+file_location));

        // get the file size of the file
        int filesize = get_dir_or_file_size(p, (19*512)+file_location, 'F');

        // use FAT entries to copy the file to current directory
        copy_to_current_dir(p, first_logic_cluster, caps_filename, filesize);



    } else {
        printf("incorrect number of arguments given\n");
        exit(1);
    }


    return 0;
}