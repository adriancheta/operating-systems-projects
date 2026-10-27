#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define VARIANT 42383
#define SHM_NAME "/gtWPIMD"
#define PAGE_SIZE 5120

typedef struct {
    unsigned int offset;
    unsigned int size;
} Section;

typedef struct {
    int req_pipe_fd;
    int resp_pipe_fd;

    int shm_fd;
    char *shm_char;
    unsigned int shm_size;

    int file_fd;
    unsigned char *file_char;
    unsigned int file_size;
} AppData;

void write_string(int fd, const char *string) {
    write(fd, string, strlen(string));
    write(fd, "#", 1);
}

void write_int(int fd, unsigned int value) {
    write(fd, &value, sizeof(unsigned int));
}

int read_string(int fd, char *string) {
    char ch;
    int pos = 0;

    while (1) {
        int r = read(fd, &ch, 1);

        if (r <= 0) {
            return -1;
        }

        if (ch == '#') {
            break;
        }

        if (pos < 250) {
            string[pos] = ch;
            pos++;
        }
    }

    string[pos] = '\0';
    return 0;
}

int read_int(int fd, unsigned int *value) {
    int total = 0;
    char *p = (char *)value;

    while (total < 4) {
        int r = read(fd, p + total, 4 - total);

        if (r <= 0) {
            return -1;
        }

        total += r;
    }

    return 0;
}

int parse_sf_file(AppData *app, Section sections[], unsigned int *nr_sections) {
    unsigned int header_size;
    unsigned int header_start;
    unsigned int version;
    unsigned int nr;
    unsigned int pos;

    if (app->file_char == NULL || app->file_size < 5) {
        return -1;
    }

    if (app->file_char[app->file_size - 1] != 'S') {
        return -1;
    }

    header_size = (unsigned int)app->file_char[app->file_size - 3] | 
                  ((unsigned int)app->file_char[app->file_size - 2] << 8);

    if (header_size < 5 || header_size > app->file_size) {
        return -1;
    }

    header_start = app->file_size - header_size;

    version = app->file_char[header_start];

    if (version < 26 || version > 97) {
        return -1;
    }

    nr = app->file_char[header_start + 1];

    if (nr != 2 && (nr < 7 || nr > 16)) {
        return -1;
    }

    pos = header_start + 2;

    if (pos + nr * 17 > app->file_size - 3) {
        return -1;
    }

    for (unsigned int i = 0; i < nr; i++) {
        unsigned int sect_type;

        sect_type = (unsigned int)app->file_char[pos + 7] |
                    ((unsigned int)app->file_char[pos + 8] << 8);

        if (sect_type != 30 && 
            sect_type != 50 && 
            sect_type != 68 && 
            sect_type != 74 && 
            sect_type != 93) {
            return -1;
        }

        sections[i].offset = (unsigned int)app->file_char[pos + 9] |
                             ((unsigned int)app->file_char[pos + 10] << 8) |
                             ((unsigned int)app->file_char[pos + 11] << 16) |
                             ((unsigned int)app->file_char[pos + 12] << 24);

        sections[i].size = (unsigned int)app->file_char[pos + 13] |
                           ((unsigned int)app->file_char[pos + 14] << 8) |
                           ((unsigned int)app->file_char[pos + 15] << 16) |
                           ((unsigned int)app->file_char[pos + 16] << 24);

        if (sections[i].offset > app->file_size) {
            return -1;
        }

        if (sections[i].size > app->file_size - sections[i].offset) {
            return -1;
        }

        pos += 17;
    }

    *nr_sections = nr;
    return 0;
}

void init_app(AppData *app) {
    app->req_pipe_fd = -1;
    app->resp_pipe_fd = -1;

    app->shm_fd = -1;
    app->shm_char = NULL;
    app->shm_size = 0;

    app->file_fd = -1;
    app->file_char = NULL;
    app->file_size = 0;
}

void clean_app(AppData *app) {
    if (app->file_char != NULL && app->file_char != MAP_FAILED) {
        munmap(app->file_char, app->file_size);
    }

    if (app->file_fd >= 0) {
        close(app->file_fd);
    }

    if (app->shm_char != NULL && app->shm_char != MAP_FAILED) {
        munmap(app->shm_char, app->shm_size);
    }

    if (app->shm_fd >= 0) {
        close(app->shm_fd);
    }

    shm_unlink(SHM_NAME);

    if (app->req_pipe_fd >= 0) {
        close(app->req_pipe_fd);
    }

    if (app->resp_pipe_fd >= 0) {
        close(app->resp_pipe_fd);
    }

    unlink("RESP_PIPE_42383");
}

int main() {
    AppData app;
    init_app(&app);

    unlink("RESP_PIPE_42383");

    if (mkfifo("RESP_PIPE_42383", 0600) < 0) {
        printf("ERROR\ncannot create the response pipe\n");
        return 1;
    }

    app.req_pipe_fd = open("REQ_PIPE_42383", O_RDONLY);
    if (app.req_pipe_fd < 0) {
        printf("ERROR\ncannot open the request pipe\n");
        unlink("RESP_PIPE_42383");
        return 1;
    }

    app.resp_pipe_fd = open("RESP_PIPE_42383", O_WRONLY);
    if (app.resp_pipe_fd < 0) {
        printf("ERROR\ncannot open the response pipe\n");
        clean_app(&app);
        return 1;
    }

    write_string(app.resp_pipe_fd, "HELLO");
    printf("SUCCESS\n");

    while (1) {
        char command[251];

        if (read_string(app.req_pipe_fd, command) < 0) {
            break;
        }

        if (strcmp(command, "ECHO") == 0) {
            unsigned int variant = VARIANT;

            write_string(app.resp_pipe_fd, "ECHO");
            write_string(app.resp_pipe_fd, "VARIANT");
            write_int(app.resp_pipe_fd, variant);
        }

        else if (strcmp(command, "CREATE_SHM") == 0) {
            bool err = false;
            unsigned int shm_size = 0;

            if (read_int(app.req_pipe_fd, &shm_size) < 0) {
                err = true;
            }

            if (!err) {
                app.shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0664);

                if (app.shm_fd < 0) {
                    err = true;
                }
            }

            if (!err) {
                if (ftruncate(app.shm_fd, shm_size) < 0) {
                    err = true;
                }
            }

            if (!err) {
                app.shm_char = (char *)mmap(0, shm_size, PROT_READ | PROT_WRITE,
                                            MAP_SHARED, app.shm_fd, 0);

                if (app.shm_char == MAP_FAILED) {
                    app.shm_char = NULL;
                    err = true;
                } else {
                    app.shm_size = shm_size;
                }
            }

            write_string(app.resp_pipe_fd, "CREATE_SHM");

            if (err) {
                write_string(app.resp_pipe_fd, "ERROR");
            } else {
                write_string(app.resp_pipe_fd, "SUCCESS");
            }
        }

        else if (strcmp(command, "WRITE_TO_SHM") == 0) {
            bool err = false;

            unsigned int offset = 0;
            unsigned int value = 0;

            if (read_int(app.req_pipe_fd, &offset) < 0) {
                err = true;
            }

            if (read_int(app.req_pipe_fd, &value) < 0) {
                err = true;
            }

            if (app.shm_char == NULL) {
                err = true;
            }

            if (!err) {
                if (offset > app.shm_size || sizeof(unsigned int) > app.shm_size - offset) {
                    err = true;
                }
            }

            if (!err) {
                memcpy(app.shm_char + offset, &value, sizeof(unsigned int));
            }

            write_string(app.resp_pipe_fd, "WRITE_TO_SHM");

            if (err) {
                write_string(app.resp_pipe_fd, "ERROR");
            } else {
                write_string(app.resp_pipe_fd, "SUCCESS");
            }
        }

        else if (strcmp(command, "MAP_FILE") == 0) {
            bool err = false;

            char file_name[251];
            off_t file_size_aux;

            if (read_string(app.req_pipe_fd, file_name) < 0) {
                err = true;
            }

            if (!err) {
                if (app.file_char != NULL && app.file_char != MAP_FAILED) {
                    munmap(app.file_char, app.file_size);
                    app.file_char = NULL;
                }

                if (app.file_fd >= 0) {
                    close(app.file_fd);
                    app.file_fd = -1;
                }

                app.file_fd = open(file_name, O_RDONLY);

                if (app.file_fd < 0) {
                    err = true;
                }
            }

            if (!err) {
                file_size_aux = lseek(app.file_fd, 0, SEEK_END);

                if (file_size_aux <= 0) {
                    err = true;
                } else {
                    app.file_size = (unsigned int)file_size_aux;
                }
            }

            if (!err) {
                app.file_char = (unsigned char *)mmap(0, app.file_size, PROT_READ,
                                                      MAP_PRIVATE, app.file_fd, 0);

                if (app.file_char == MAP_FAILED) {
                    app.file_char = NULL;
                    err = true;
                }
            }

            write_string(app.resp_pipe_fd, "MAP_FILE");

            if (err) {
                write_string(app.resp_pipe_fd, "ERROR");
            } else {
                write_string(app.resp_pipe_fd, "SUCCESS");
            }
        }

        else if (strcmp(command, "READ_FROM_FILE_OFFSET") == 0) {
            bool err = false;

            unsigned int offset = 0;
            unsigned int no_of_bytes = 0;

            if (read_int(app.req_pipe_fd, &offset) < 0) {
                err = true;
            }

            if (read_int(app.req_pipe_fd, &no_of_bytes) < 0) {
                err = true;
            }

            if (app.shm_char == NULL || app.file_char == NULL) {
                err = true;
            }

            if (!err) {
                if (offset > app.file_size || no_of_bytes > app.file_size - offset) {
                    err = true;
                }
            }

            if (!err) {
                if (no_of_bytes > app.shm_size) {
                    err = true;
                }
            }

            if (!err) {
                memcpy(app.shm_char, app.file_char + offset, no_of_bytes);
            }

            write_string(app.resp_pipe_fd, "READ_FROM_FILE_OFFSET");

            if (err) {
                write_string(app.resp_pipe_fd, "ERROR");
            } else {
                write_string(app.resp_pipe_fd, "SUCCESS");
            }
        }

        else if (strcmp(command, "READ_FROM_FILE_SECTION") == 0) {
            bool err = false;

            unsigned int section_no = 0;
            unsigned int offset = 0;
            unsigned int no_of_bytes = 0;

            Section sections[20];
            unsigned int nr_sections = 0;

            if (read_int(app.req_pipe_fd, &section_no) < 0) {
                err = true;
            }

            if (read_int(app.req_pipe_fd, &offset) < 0) {
                err = true;
            }

            if (read_int(app.req_pipe_fd, &no_of_bytes) < 0) {
                err = true;
            }

            if (app.shm_char == NULL || app.file_char == NULL) {
                err = true;
            }

            if (!err) {
                if (parse_sf_file(&app, sections, &nr_sections) < 0) {
                    err = true;
                }
            }

            if (!err) {
                if (section_no < 1 || section_no > nr_sections) {
                    err = true;
                }
            }

            if (!err) {
                unsigned int index = section_no - 1;

                if (offset > sections[index].size ||
                    no_of_bytes > sections[index].size - offset) {
                    err = true;
                }

                if (!err && no_of_bytes > app.shm_size) {
                    err = true;
                }

                if (!err) {
                    memcpy(app.shm_char,
                           app.file_char + sections[index].offset + offset,
                           no_of_bytes);
                }
            }

            write_string(app.resp_pipe_fd, "READ_FROM_FILE_SECTION");

            if (err) {
                write_string(app.resp_pipe_fd, "ERROR");
            } else {
                write_string(app.resp_pipe_fd, "SUCCESS");
            }
        }

        else if (strcmp(command, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0) {
            bool err = false;

            unsigned int logical_offset = 0;
            unsigned int no_of_bytes = 0;

            Section sections[20];
            unsigned int logical_start[20];
            unsigned int nr_sections = 0;

            int found_section = -1;

            if (read_int(app.req_pipe_fd, &logical_offset) < 0) {
                err = true;
            }

            if (read_int(app.req_pipe_fd, &no_of_bytes) < 0) {
                err = true;
            }

            if (app.shm_char == NULL || app.file_char == NULL) {
                err = true;
            }

            if (!err) {
                if (parse_sf_file(&app, sections, &nr_sections) < 0) {
                    err = true;
                }
            }

            if (!err) {
                unsigned int current_logical_offset = 0;

                for (unsigned int i = 0; i < nr_sections; i++) {
                    unsigned int end_offset;

                    logical_start[i] = current_logical_offset;

                    end_offset = current_logical_offset + sections[i].size;

                    if (end_offset % PAGE_SIZE == 0) {
                        current_logical_offset = end_offset;
                    } else {
                        current_logical_offset = ((end_offset / PAGE_SIZE) + 1) * PAGE_SIZE;
                    }
                }

                for (unsigned int i = 0; i < nr_sections; i++) {
                    if (logical_offset >= logical_start[i]) {
                        unsigned int inside_offset = logical_offset - logical_start[i];

                        if (inside_offset <= sections[i].size &&
                            no_of_bytes <= sections[i].size - inside_offset) {
                            found_section = (int)i;
                            break;
                        }
                    }
                }

                if (found_section == -1) {
                    err = true;
                }
            }

            if (!err) {
                unsigned int index = (unsigned int)found_section;
                unsigned int inside_offset = logical_offset - logical_start[index];

                if (no_of_bytes > app.shm_size) {
                    err = true;
                }

                if (!err) {
                    memcpy(app.shm_char,
                           app.file_char + sections[index].offset + inside_offset,
                           no_of_bytes);
                }
            }

            write_string(app.resp_pipe_fd, "READ_FROM_LOGICAL_SPACE_OFFSET");

            if (err) {
                write_string(app.resp_pipe_fd, "ERROR");
            } else {
                write_string(app.resp_pipe_fd, "SUCCESS");
            }
        }

        else if (strcmp(command, "EXIT") == 0) {
            break;
        }
    }

    clean_app(&app);
    return 0;
}