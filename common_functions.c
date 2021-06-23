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

#define dir_reading 8
#define file_reading 12

int get_free_size_on_disk(char* p) {

    int max_fat_entries = 2848;
    int free_sectors = 0;

    int entry_number = 2;
    
    while (entry_number <= max_fat_entries ) {
        int fat_value = get_fat_entry_value(p, entry_number);
        if (fat_value == 0) {
            free_sectors++;
        }
        entry_number++;
    }
    return free_sectors*512;
}

void get_dirname(char *p, char* calling_dirname, int dir_start) {
    char dirname[8];
    int dirname_index = 0;
    int dirname_end = dir_start+8;

    for (int i = dir_start ; i < dirname_end ; i++) {
        if (p[i] == ' ') {
            continue;
        } 
        dirname[dirname_index] = p[i];
        dirname_index++;
    }
    dirname[dirname_index] = '\0';
    
    calling_dirname[dirname_index] = '\0';
    strncpy(calling_dirname, dirname, dirname_index);
    
}

char get_dir_or_file(char* p, int dir_start) {
    int attribute_field = p[dir_start+11];

    unsigned short logic_cluster = p[dir_start+26];
    if (logic_cluster == 0 || logic_cluster == 1) {
        return '\0';
    }

    if ((attribute_field & 0x10) == 16) {
        return 'D';
    } else {
        return 'F';
    }
}

int get_fat_entry_value(char* p, int entry_number) {
    int fat1_start = 512;
    int fat_value;

    if (entry_number%2 == 0) { // even
        // get low 4 bits in this location
        int four_bit_loc = fat1_start+(1+(3*entry_number)/2);
        int four_bits = p[four_bit_loc];
        four_bits = four_bits & 0x0F;
        four_bits = four_bits << 8;

        // get 8 bits in this location, ie. 1 byte
        int byte_loc = fat1_start+(3*entry_number)/2;
        unsigned char byte = p[byte_loc];

        fat_value = byte + four_bits;

    } else { // odd
        // get high 4 bits in this location
        int four_bit_loc = fat1_start+(3*entry_number)/2;
        int four_bits = p[four_bit_loc];
        four_bits = four_bits & 0xF0;
        four_bits = four_bits >> 4;

        // get 8 bits in this location, ie. 1 byte
        int byte_loc = fat1_start+(1+(3*entry_number)/2);
        unsigned char byte = p[byte_loc];
        int new_byte = byte << 4;

        fat_value = four_bits + new_byte;
    }
    return fat_value;
}

int get_dir_or_file_size(char* p, int dir_start, char data_type) {
    if (data_type == 'D') return 0;

    int offset = dir_start+28;

    unsigned char lowest_byte = p[offset];
    unsigned char low_mid_byte = p[offset+1];
    unsigned char high_mid_byte = p[offset+2];
    unsigned char highest_byte = p[offset+3];


    int low_mid_byte_int = low_mid_byte;
    int high_mid_byte_int = high_mid_byte;
    int highest_byte_int = highest_byte;

    low_mid_byte_int  = low_mid_byte << 8;
    high_mid_byte_int  = high_mid_byte << 16;
    highest_byte_int  = highest_byte << 24;
    int file_size_as_int = lowest_byte + low_mid_byte_int + high_mid_byte_int + highest_byte_int;
    return file_size_as_int;

}

void get_full_filename(char* p, char* filename, int dir_start, char data_type) {

    char full_filename[12];
    int filename_index = 0;
    int filename_end = dir_start+11;

    for (int i = dir_start ; i < filename_end ; i++) {
        if (i == dir_start+8) {
            full_filename[filename_index] = '.';
            filename_index++;
        } else if (p[i] == ' ') {
            continue;
        } 
        full_filename[filename_index] = p[i];
        filename_index++;
    }
    full_filename[filename_index] = '\0';
    filename[filename_index] = '\0';
    strncpy(filename, full_filename, filename_index);
}

int first_logic_cluster_zero_or_one(char* p, int i) {
    int first_logic_cluster_field_low_byte = p[i+26] == 0 || p[i+26] == 1; // uses little endian
    int first_logic_cluster_field_high_byte = p[i+27] == 0; // high byte must be 0

    return first_logic_cluster_field_high_byte && first_logic_cluster_field_low_byte;
}

int get_num_files_on_disk(char* p) {
    int root_dir = 19 * 512;
    int data_area = 33 * 512;
    int dir_entry_length = 32;

    int num_files = 0;

    int i = root_dir;
    while (i < data_area) {
        int attribute_field = p[i+11];

        int first_logic_cluster_check = first_logic_cluster_zero_or_one(p, i); 

        int volume_label_bit_set = (attribute_field & 0x08) == 8;

        unsigned char empty_check = p[i];

        if (attribute_field == 0x0f
         || first_logic_cluster_check
         || volume_label_bit_set
         || empty_check == 0x00
         || empty_check == 0xe5) {
            i+=dir_entry_length;
            continue;
        } else { 
            int j = i;
            while (j < i+8) {
                j++;
            }
            num_files++;
        }
        i+=dir_entry_length;
    }

    return num_files;
}