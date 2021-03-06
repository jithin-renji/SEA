/*
 *
 *    A program to encrypt a given file, by shifting the bytes of the file
 *    by pseudorandom numbers.
 *
 *    Copyright (C) 2019-2020 Jithin Renji
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* For fstat() */
#include <sys/types.h>
#include <sys/stat.h>

/* For ioctl() */
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fs.h>

/* For read() and write() */
#include <unistd.h>

/* For open() */
#include <fcntl.h>

/* For getopt_long() */
#include <getopt.h>

#define BUF_SIZE    2048

/* Flags */
#define ENCRYPT     1
#define DECRYPT     1 << 2
#define CLEAR_DEV   1 << 3

/* Check if a given flag is enabled */
#define IS_ENABLED(flags_var, flag) ((flags & flag) == flag)

/*
    sea_encrypt():
        Takes in the name of the file to be encrypted,
        and generates a number of random numbers, where the
        number of numbers, equals the number of bytes in the
        file. This is written to a USB flash drive.
*/
void sea_encrypt(char* fname, char* key_dev_name, int flags);

/*
    sea_decrypt():
        The data relating to the series of numbers is stored
        in the device, which is given to the function during encryption.
        Without this device, the key cannot be found, and hence the file
        cannot be decrypted.
 */
void sea_decrypt(char* fname, char* key_dev_name);

/*
   Check the files before encryption.
   The key device *must* be a block device
*/
int check_files(char* fname, char* key_dev_name, int fd_file, int fd_dev);

/* Prompt to warn the user of possible data loss */
int warning_prompt(char* key_dev_name);

/* Clear the key device of its contents */
void clear_dev(int fd_dev, size_t count);

/* Print help message */
void help(char* prog_name);

/* Print version information */
void version(void);

int option_index = 0;

/* Long options */
struct option long_opt[] = {
    {"encrypt", required_argument,  0, 'e'},
    {"decrypt", required_argument,  0, 'd'},
    {"clear",   no_argument,        0, 'c'},
    {"help",    no_argument,        0, 'h'},
    {"version", no_argument,        0, 'V'},
    {0,         0,                  0,  0}
};

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: Too few arguments\n");
        fprintf(stderr, "Try '%s --help'\n", argv[0]);

        exit(EXIT_FAILURE);
    } else {
        int opt = 0;

        int flags = 0;

        /* Name of the file to be encrypted */
        char fname[FILENAME_MAX];

        /* Key device name */
        char key_dev_name[FILENAME_MAX];

        /* Parse options */
        while ((opt = getopt_long(argc, argv, "e:d:chV", long_opt, &option_index)) != -1) {
            switch (opt){
            case 'e':
                flags |= ENCRYPT;
                strcpy(fname, optarg);
                break;

            case 'd':
                flags |= DECRYPT;
                strcpy(fname, optarg);
                break;

            case 'c':
                flags |= CLEAR_DEV;
                break;

            case 'h':
                help(argv[0]);
                break;

            case 'V':
                version();
                break;

            default:
                exit(EXIT_FAILURE);
            }
        }

        /* Can only encrypt, or decrypt at once */
        if (IS_ENABLED(flags, ENCRYPT) ^ IS_ENABLED(flags, DECRYPT)) {
            if (argv[optind] == NULL) {
                fprintf(stderr, "Error: Key device name was not provided\n");
                exit(EXIT_FAILURE);
            } else {
                strcpy(key_dev_name, argv[optind]);
                if (IS_ENABLED(flags, ENCRYPT)) {
                    sea_encrypt(fname, key_dev_name, flags);
                } else {
                    sea_decrypt(fname, key_dev_name);
                }
            }
        } else if (IS_ENABLED(flags, ENCRYPT) && IS_ENABLED(flags, DECRYPT)) {
            fprintf(stderr, "Error: Cannot encrypt and decrypt at the same time\n");
            exit(EXIT_FAILURE);
        } else {
            fprintf(stderr, "Error: Don't know whether to encrypt or decrypt\n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

void sea_encrypt(char* fname, char* key_dev_name, int flags)
{
    srand(time(NULL));

    /* Open both the files */
    int fd_file = open(fname, O_RDONLY);
    int fd_dev = open(key_dev_name, O_RDWR);

    /*
        File stats, which will be used,
        to determine the number of bytes
        to be written.
     */
    struct stat file_stat;
    long long bytes_file = 0;

    /*
        Device stats, which will be used,
        to clear the device, and to check
        available space.
    */
    struct stat dev_stat;
    unsigned long nblocks_dev = 0;
    long long  bytes_dev = 0;

    char ofname[FILENAME_MAX];

    strcpy(ofname, fname);
    strcat(ofname, "_encr");

    int fd_ofile = open(ofname, O_WRONLY | O_CREAT);

    if (fd_ofile == -1) {
        perror("sea: cannot open file for writing");
        exit(EXIT_FAILURE);
    }

    /*
        Check to if both the input file and
        the given device file exists. Also,
        check if the device file is a block
        device.

        If any one of the above is not satisfied,
        check_files() returns -1, else 0.
    */
    int cf_err = check_files(fname, key_dev_name, fd_file, fd_dev);

    if (cf_err == -1) {
        exit(EXIT_FAILURE);
    } else {
        /* Signal to proceed or not */
        int sig = warning_prompt(key_dev_name);

        if (sig == 1) {
            printf("Aborted.\n");
            exit(2);
        } else {
            /* For getting block size of the device */
            int r_dev = ioctl(fd_dev, BLKGETSIZE, &nblocks_dev);
            if (r_dev == -1) {
                perror("sea");
                exit(EXIT_FAILURE);
            }

            fstat(fd_file, &file_stat);
            fstat(fd_dev, &dev_stat);

            bytes_file = file_stat.st_size;

            bytes_dev = nblocks_dev * 512;

            if (bytes_dev < bytes_file) {
                fprintf(stderr, "Error: Not enough space in the given device.\n");
                exit(EXIT_FAILURE);
            } else {
                if (IS_ENABLED(flags, CLEAR_DEV))
                    clear_dev(fd_dev, bytes_dev);

                /*
                    Since we are using the same file
                    descriptor, we have to seek to the
                    beginning of the device file, or else
                    we will get an error saying that there
                    is not enough space in the drive.
                */
                int ret_seek = lseek(fd_dev, 0, SEEK_SET);
                if (ret_seek == -1) {
                    perror("sea: could not seek to beginning of device");
                    exit(EXIT_FAILURE);
                }

                /* Buffer to store the character that was read. */
                size_t alloc_size = bytes_file < BUF_SIZE ? bytes_file : BUF_SIZE;
                char buf[alloc_size];
                char shifts[alloc_size];

                /*
                    This value is mainly used to calculate the
                    percentage of progress.
                */
                long double nbytes_written = 0;
                size_t bytes_read = 0;
                while ((bytes_read = read(fd_file, buf, alloc_size)) != 0) {
                    for (size_t i = 0; i < bytes_read; i++) {
                        char shift_size = rand() % 26;
                        buf[i] += shift_size;
                        shifts[i] = shift_size;
                    }

                    int ret_ofile = write(fd_ofile, buf, bytes_read);
                    if (ret_ofile == -1) {
                        perror("sea: cannot write to file");
                        exit(EXIT_FAILURE);
                    }

                    /*
                        The series of pseudorandom shift sizes
                        acts as the key. This key will be written
                        to the given device. Essentially, the
                        device becomes the key for decryption.
                    */
                    int ret_dev = write(fd_dev, (void*)shifts, bytes_read);
                    if (ret_dev == -1) {
                        perror("sea: cannot write key to device");
                        exit(EXIT_FAILURE);
                    }

                    long double percent = (nbytes_written / bytes_file) * 100;

                    /* Hide cursor */
                    fputs("\e[?25l", stdout);

                    printf("Writing encryption key to device, and writing encrypted file... [%.2Lf%%]\r", percent);
                    fflush(stdout);

                    /* Show cursor */
                    fputs("\e[?25h", stdout);

                    nbytes_written += bytes_read;
                }
                printf("\n\nDone!\n");
            }
        }
    }
    close(fd_dev);
    close(fd_file);
    close(fd_ofile);

    chmod(ofname, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

void sea_decrypt(char* fname, char* key_dev_name)
{
    /* Open both the files */
    int fd_file = open(fname, O_RDONLY);
    int fd_dev = open(key_dev_name, O_RDONLY);

    char ofname[FILENAME_MAX];
    strcpy(ofname, fname);
    strcat(ofname, "_decr");

    int fd_ofile = open(ofname, O_WRONLY | O_CREAT);

    if (fd_ofile == -1) {
        perror("sea: cannot open file for writing");
        exit(EXIT_FAILURE);
    }

    if (fd_file == -1) {
        perror("sea: cannot open file for reading");
        exit(EXIT_FAILURE);
    }

    if (fd_dev == -1) {
        perror("sea: cannot open device for reading");
        exit(EXIT_FAILURE);
    }

    /*
        Input file stats, which are to be used to find
        the size of the file.
    */
    struct stat buf;
    fstat(fd_file, &buf);

    /* Same as in sea_encrypt() */
    long double nbytes_written = 0;
    long double bytes_file = buf.st_size;

    const size_t alloc_size = bytes_file < BUF_SIZE ? bytes_file : BUF_SIZE;

    /* Buffer for storing bytes read from the input file */
    char file_buf[alloc_size];

    /* Buffer for storing bytes read from the device */
    char dev_buf[alloc_size];

    /* Read bytes from device and input file simultaneously */
    while (read(fd_file, (void*) file_buf, alloc_size) != 0 &&
           read(fd_dev, (void*) dev_buf, alloc_size) != 0) {
        char out_buf[alloc_size];

        for (size_t i = 0; i < alloc_size; i++) {
            int shift_size = dev_buf[i];
            out_buf[i] = file_buf[i] - shift_size;
        }

        int ret = write(fd_ofile, (void*)out_buf, alloc_size);
        if (ret == -1) {
            perror("sea: cannot write to file");
            exit(EXIT_FAILURE);
        }

        long double percent = (nbytes_written/bytes_file) * 100;

        printf("Writing decrypted file... [%.2Lf%%]\r", percent);
        fflush(stdout);

        nbytes_written += alloc_size;
    }

    close(fd_dev);
    close(fd_file);
    close(fd_ofile);

    chmod(ofname, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    printf("\n\nDone!\n");
}

int check_files(char* fname, char* key_dev_name, int fd_file, int fd_dev)
{
    /*
        Value to be returned by the function.
        If ret == 0, then no errors, if ret == -1,
        there were errors.
    */
    int ret = 0;

    if (fd_file == -1) {
        char msg[100];

        strcpy(msg, "sea: ");
        strcat(msg, "'");
        strcat(msg, fname);
        strcat(msg, "'");

        perror(msg);
        ret = -1;
    } else if (fd_dev == -1) {
        char msg[100];

        strcpy(msg, "sea: ");
        strcat(msg, "'");
        strcat(msg, key_dev_name);
        strcat(msg, "'");

        perror(msg);
        ret = -1;
    } else {
        /* Check if the given device file is a block device */
        struct stat buf;
        fstat(fd_dev, &buf);

        if (buf.st_rdev == 0) {
            printf("Error: '%s' is not a block device\n", key_dev_name);
            ret = -1;
        }
    }

    return ret;
}

int warning_prompt(char* key_dev_name)
{
    int ret = 1;

    printf("WARNING: The contents of '%s' CANNOT be recovered after this operation.\n\
         If the encryption key for another file is stored in this device, it\n\
         will be removed.\n\n", key_dev_name);

    printf("Do you STILL want to continue? [y/N] ");
    int ch = fgetc(stdin);

    printf("\n");

    if (ch == 'y' || ch == 'Y') {
        ret = 0;
    } else {
        ret = 1;
    }

    return ret;
}

void clear_dev(int fd_dev, size_t count)
{
    /*
        Number of bytes written, is
        used to calculate the percentage of
        progress
    */
    long double nbytes_written = 0;

    /* Buffer to store 0's to be written */
    char buf[512];

    memset(buf, 0, sizeof(buf));

    while (nbytes_written != count) {
        int ret = write(fd_dev, buf, 512);

        if (ret == -1) {
            perror("sea: could not clear device");
            exit(3);
        }

        long double percent = (nbytes_written / count) * 100;

        /* Hide cursor */
        fputs("\e[?25l", stdout);

        printf("Clearing %ld byte(s). [%.2Lf%%]\r", count, percent);
        fflush(stdout);

        /* Show cursor */
        fputs("\e[?25h", stdout);

        nbytes_written += 512;
    }
    printf("\n");
}

void help(char* prog_name)
{
    printf("Usage: %s <-e | -d> <file name> <key device>\n\n", prog_name);

    printf("SEA is a program which can be used to encrypt any given file,\n\
by shifting the bytes of the file by pseudorandom numbers.\n\n");

    printf(
        "Options:\n"
               "\t-e, --encrypt <file name>\tEncrypt given file\n"
               "\t-d, --decrypt <file name>\tDecrypt given file\n"
               "\t-c, --clear\t\t\tClear the device before writing the key\n"
               "\t-h, --help\t\t\tShow this help message\n"
               "\t-V, --version\t\t\tShow version information\n\n"
    );

    printf("NOTE: This program requires root privileges for writing encryption key to\n\
the given device, or to read the encryption key from the same device.\n");

    exit(EXIT_SUCCESS);
}

void version(void)
{
    printf(
        "sea 1.0\n"
        "Copyright (C) 2019-2020 Jithin Renji.\n"
        "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
        "This is free software: you are free to change and redistribute it.\n"
        "There is NO WARRANTY, to the extent permitted by law.\n\n"

        "Written by Jithin Renji.\n"
    );

    exit(EXIT_SUCCESS);
}
