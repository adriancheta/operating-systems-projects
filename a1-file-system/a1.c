#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char name[8];
    short int type;
    int offset;
    int size;
} HeaderSection;

typedef struct {
    short int version;
    short int nr_of_sect;
    HeaderSection *sections;
    short int size;
    char magic;
} SFFile;

int parse_file(int fd, SFFile *sf_file, bool should_print);
int handle_parse_file(int argc, char **argv);
int extract_from_file(int fd, int sect_nr, int sect_line);
int handle_extract_from_file(int argc, char **argv);
int list_dir_content(char *dir_path, bool is_rec_option, char *string_to_check, bool is_perm_option);
int handle_list_dir_content(int argc, char **argv);
int findall(char *dir_path);
int handle_findall(int argc, char **argv);

int handle_findall(int argc, char **argv) {
    char *path = NULL;
    struct stat file_meta_data;

    path = argv[argc-1] + 5;

    if (stat(path, &file_meta_data) < 0) {
        printf("Could not get file data\n");
        return -1;
    }

    if (!S_ISDIR(file_meta_data.st_mode)) {
        printf("ERROR\ninvalid directory path\n");
        return -1;
    }

    printf("SUCCESS\n");
    findall(path);
    return 0;
}

int findall(char *dir_path) {
    DIR* dir;
    struct dirent *dir_entry;
    struct stat file_meta_data;
    char name[512];
    int fd;
    SFFile sf_file;
    int good_sects;
    int lines;
    char *buf;

    dir = opendir(dir_path);
    if (dir == 0) {
        printf("Could not open directory\n");
        return -1;
    }

    while ((dir_entry = readdir(dir)) != 0) {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        sprintf(name, "%s/%s", dir_path, dir_entry->d_name);

        if (stat(name, &file_meta_data) < 0) {
            continue;
        }

        if (S_ISDIR(file_meta_data.st_mode)) {
            findall(name);
            continue;
        }

        fd = open(name, O_RDONLY);
        if (fd < 0) {
            printf("Could not open file\n");
            continue;
        }

        sf_file.sections = NULL;

        if (parse_file(fd, &sf_file, false) != 0) {
            close(fd);
            continue;
        }

        good_sects = 0;

        for (int i = 0; i < sf_file.nr_of_sect; i++) {
            buf = (char*)malloc(sf_file.sections[i].size);
            if (buf == NULL) {
                continue;
            }

            lseek(fd, sf_file.sections[i].offset, SEEK_SET);
            if (read(fd, buf, sf_file.sections[i].size) == sf_file.sections[i].size) {

                lines = 0;

                for (int j = 0; j < sf_file.sections[i].size; j++) {
                    if (buf[j] == 0x0A) {
                        lines++;
                    }
                }

                if (sf_file.sections[i].size > 0 && buf[sf_file.sections[i].size - 1] != 0x0A) {
                    lines++;
                }

                if (lines == 13) {
                    good_sects++;
                }
            }

            free(buf);
        }

        if (good_sects >= 2) {
            printf("%s\n", name);
        }

        free(sf_file.sections);
        close(fd);
    }

    closedir(dir);
    return 0;
}

int handle_extract_from_file(int argc, char **argv) {
    int fd = -1;
    int sect_nr = -1;
    int sect_line = -1;
    char *path = NULL;

    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "path=", 5) == 0) {
            path = argv[i] + 5;
        } else if (strncmp(argv[i], "section=", 8) == 0) {
            sect_nr = atoi(argv[i] + 8);
        } else if (strncmp(argv[i], "line=", 5) == 0) {
            sect_line = atoi(argv[i] + 5);
        }
    }

    if (sect_nr <= 0 || sect_line <= 0) {
        printf("Wrong command line input\n");
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("File could not be opened\n");
        return -1;
    }

    extract_from_file(fd, sect_nr, sect_line);
    close(fd);
    return 0;
}

int extract_from_file(int fd, int sect_nr, int sect_line) {
    SFFile sf_file;
    char *buf;
    int curr_line = 1;
    int line_start = 0;
    int sect_size;
    int sect_offset;
    sf_file.sections = NULL;

    bool had_error = false;
    char err_msg[512] = "invalid ";

    if (parse_file(fd, &sf_file, false) != 0) {
        sprintf(err_msg + strlen(err_msg), "file");
        had_error = true;
    }

    if (sect_nr < 1 || sect_nr > sf_file.nr_of_sect) {
        if (had_error) {
            sprintf(err_msg + strlen(err_msg), " | ");
        }
        sprintf(err_msg + strlen(err_msg), "section");
    }

    sect_size = sf_file.sections[sect_nr - 1].size;
    sect_offset = sf_file.sections[sect_nr - 1].offset;

    buf = (char*)malloc(sect_size + 1);

    lseek(fd, sect_offset, SEEK_SET);
    if (read(fd, buf, sect_size) != sect_size) {
        printf("Could not read Section Size\n");
        free(buf);
        free(sf_file.sections);
        return -1;
    }

    buf[sect_size] = '\0';

    for (int i = 0; i <= sect_size; i++) {
        if (i == sect_size || buf[i] == 0x0A) {
            if (curr_line == sect_line) {
                buf[i] = '\0';
                printf("SUCCESS\n%s\n", buf + line_start);
                free(buf);
                free(sf_file.sections);
                return 0;
            }

            curr_line++;
            line_start = i + 1;
        }
    }

    printf("%sline", err_msg);
    free(buf);
    free(sf_file.sections);
    return -1;
}

int handle_parse_file(int argc, char **argv) {
    int fd = -1;
    char *path = NULL;
    SFFile sf_file;

    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "path=", 5) == 0) {
            path = argv[i] + 5;
        }
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Could not open file\n");
        return -1;
    }

    sf_file.sections = NULL;
    parse_file(fd, &sf_file, true);

    if (sf_file.sections != NULL) {
        free(sf_file.sections);
    }

    close(fd);
    return 0;
}

int parse_file(int fd, SFFile *sf_file, bool should_print) {
    int file_size;
    short int header_size;
    short int sect_type;
    char version;
    char nr_of_sect;
    char err_msg[512] = "wrong ";
    bool had_error = false;

    sf_file->sections = NULL;

    if (fd < 0) {
        if (should_print) {
            printf("Could not open file\n");
        }
        return -1;
    }

    file_size = lseek(fd, 0, SEEK_END);
    if (file_size < 0) {
        if (should_print) {
            printf("File size too small\n");
        }
        return -1;
    }

    lseek(fd, -1, SEEK_END);
    if (read(fd, &sf_file->magic, 1) != 1) {
        if (should_print) {
            printf("Could not read Magic\n");
        }
        return -1;
    }

    if (sf_file->magic != 'S') {
        sprintf(err_msg + strlen(err_msg), "magic");
        had_error = true;
    }

    lseek(fd, -3, SEEK_END);
    if (read(fd, &header_size, 2) != 2) {
        if (should_print) {
            printf("Could not read Header Size\n");
        }
        return -1;
    }

    sf_file->size = header_size;

    if (header_size < 5 || header_size > file_size) {
        if (should_print) {
            printf("Header Size bigger than File Size\n");
        }
        return -1;
    }

    lseek(fd, file_size - header_size, SEEK_SET);
    if (read(fd, &version, 1) != 1) {
        if (should_print) {
            printf("Could not read Version\n");
        }
        return -1;
    }

    sf_file->version = version;

    if (sf_file->version < 26 || sf_file->version > 97) {
        if (had_error) {
            sprintf(err_msg + strlen(err_msg), " | ");
        }
        sprintf(err_msg + strlen(err_msg), "version");
        had_error = true;
    }

    if (read(fd, &nr_of_sect, 1) != 1) {
        if (had_error) {
            sprintf(err_msg + strlen(err_msg), " | ");
        }
        sprintf(err_msg + strlen(err_msg), "sect_nr");
        had_error = true;
    }

    sf_file->nr_of_sect = nr_of_sect;

    if (sf_file->nr_of_sect != 2 && (sf_file->nr_of_sect < 7 || sf_file->nr_of_sect > 16)) {
        if (had_error) {
            sprintf(err_msg + strlen(err_msg), " | ");
        }
        sprintf(err_msg + strlen(err_msg), "sect_nr");
        had_error = true;
    }

    sf_file->sections = (HeaderSection*)malloc(sf_file->nr_of_sect * sizeof(HeaderSection));

    for (int i = 0; i < sf_file->nr_of_sect; i++) {
        if (read(fd, sf_file->sections[i].name, 7) != 7) {
            free(sf_file->sections);
            sf_file->sections = NULL;
            if (should_print) {
                printf("Could not read Section Name\n");
            }
            return -1;
        }

        sf_file->sections[i].name[7] = '\0';

        if (read(fd, &sect_type, 2) != 2) {
            free(sf_file->sections);
            sf_file->sections = NULL;
            if (should_print) {
                printf("Could not read Section Type\n");
            }
            return -1;
        }

        sf_file->sections[i].type = sect_type;

        if (sect_type != 30 && sect_type != 50 && sect_type != 93 && sect_type != 68 && sect_type != 74) {
            if (had_error) {
                sprintf(err_msg + strlen(err_msg), " | ");
            }
            sprintf(err_msg + strlen(err_msg), "sect_types");
            had_error = true;
            break;
        }

        if (read(fd, &sf_file->sections[i].offset, 4) != 4) {
            free(sf_file->sections);
            sf_file->sections = NULL;
            if (should_print) {
                printf("Could not read Section Offset\n");
            }
            return -1;
        }

        if (read(fd, &sf_file->sections[i].size, 4) != 4) {
            free(sf_file->sections);
            sf_file->sections = NULL;
            if (should_print) {
                printf("Could not read Section Size\n");
            }
            return -1;
        }
    }

    if (had_error) {
        if (should_print) {
            printf("ERROR\n%s\n", err_msg);
        }
        free(sf_file->sections);
        sf_file->sections = NULL;
        return -1;
    }

    if (should_print) {
        printf("SUCCESS\n");
        printf("version=%d\nnr_sections=%d\n", sf_file->version, sf_file->nr_of_sect);

        for (int i = 0; i < sf_file->nr_of_sect; i++) {
            printf("section%d: %s %d %d\n", i + 1, sf_file->sections[i].name, sf_file->sections[i].type, sf_file->sections[i].size);
        }
    }

    return 0;
}

int handle_list_dir_content(int argc, char **argv) {
    bool is_rec_option = false;
    bool is_perm_option = false;
    char string_to_check[512] = "";
    char *path;
    struct stat file_meta_data;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "recursive") == 0) {
            is_rec_option = true;
        } else if (strncmp(argv[i], "name_ends_with=", 15) == 0) {
            strcpy(string_to_check, argv[i] + 15);
        } else if (strcmp(argv[i], "has_perm_write") == 0) {
            is_perm_option = true;
        } else if (strncmp(argv[i], "path=", 5) == 0) {
            path = argv[i] + 5;
        }
    }

    if (stat(path, &file_meta_data) < 0) {
        printf("Could not get file data\n");
        return -1;
    }

    if (!S_ISDIR(file_meta_data.st_mode)) {
        printf("ERROR\ninvalid directory path\n");
        return -1;
    }

    printf("SUCCESS\n");
    list_dir_content(path, is_rec_option, string_to_check, is_perm_option);
    return 0;
}

int list_dir_content(char *dir_path, bool is_rec_option, char *string_to_check, bool is_perm_option) {
    DIR* dir;
    struct dirent *dir_entry;
    struct stat file_meta_data;
    char name[512];

    int string_len = strlen(string_to_check);
    int path_len;

    dir = opendir(dir_path);
    if (dir == 0) {
        printf("Error opening directory\n");
        return -1;
    }

    while ((dir_entry = readdir(dir)) != 0) {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        sprintf(name, "%s/%s", dir_path, dir_entry->d_name);

        if (stat(name, &file_meta_data) < 0) {
            continue;
        }
        
        if (is_perm_option) {
            if ((file_meta_data.st_mode & S_IWUSR) == 0) {
                continue;
            }
        }

        if (strcmp(string_to_check, "") != 0) {
            path_len = strlen(name);

            if (strcmp(name + path_len - string_len, string_to_check) != 0) {
                continue;
            }
        }   

        if (is_rec_option) {
            if (S_ISDIR(file_meta_data.st_mode)) {
                list_dir_content(name, is_rec_option, string_to_check, is_perm_option);
            }
        }

        printf("%s\n", name);
    }

    closedir(dir);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Wrong usage\n");
        return -1;
    }

    if (argc == 2 && strcmp(argv[1], "variant") == 0) {
        printf("42383\n");
        return 0;
    }

    // List (WORKS)
    if (strcmp(argv[1], "list") == 0) {
        handle_list_dir_content(argc, argv);
        return 0;
    }

    // Prase (WORKS)
    if (strcmp(argv[1], "parse") == 0) {
        handle_parse_file(argc, argv);
        return 0;
    }

    // Extract (WORKS)
    if (strcmp(argv[1], "extract") == 0) {
        handle_extract_from_file(argc, argv);
        return 0;
    }

    if (strcmp(argv[1], "findall") == 0) {
        handle_findall(argc, argv);
        return 0;
    }

    return 0;
}