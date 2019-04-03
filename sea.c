/*
 *
 *    A (attempted) different implementation of the
 *    One-Time Pad.
 *
 *    Copyright (C) 2019 Jithin Renji
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

/* For lstat() */
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

#define ENCRYPT     1
#define DECRYPT     0

#define BUF_SIZE    2048

/*
    The SEA function:
        Takes in the name of the file to be encrypted,
        and generates a number of random numbers, where the
        number of numbers, equals the number of bytes in the
        file.

        The data relating to the series of numbers is stored
        in the device, which is given to the function during encryption.
        Without this device, the key cannot be found, and hence the file
        cannot be decrypted.
*/
void sea (char* fname, char* key_dev_name, int encr_decr);

int check_files (char* fname, char* key_dev_name, int fd_file, int fd_dev);

int warning_prompt (char* key_dev_name);

void clear_dev (int fd_dev, size_t count, size_t bs);

/* Print help message */
void help (char* prog_name);

/* Print version information */
void version (void);

int option_index = 0;

struct option long_opt[] = {
    {"encrypt", required_argument,  0, 'e'},
    {"decrypt", required_argument,  0, 'd'},
    {"help",    no_argument,        0, 'h'},
    {"version", no_argument,        0, 'V'},
    {0,         0,                  0,  0}
};

int main (int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: Too few arguments\n\n");
        fprintf(stderr, "Try '%s --help'\n", argv[0]);

        exit(EXIT_FAILURE);
    } else {
        int opt = 0;

        /* Encrypt or not */
        int encrypt = 0;

        /* Decrypt or not */
        int decrypt = 0;

        /* Name of the file to be encrypted */
        char fname[FILENAME_MAX];

        /* Key device name */
        char key_dev_name[FILENAME_MAX];

        while ((opt = getopt_long(argc, argv, "e:d:hV", long_opt, &option_index)) != -1) {
            switch (opt){
            case 'e':
                encrypt = 1;
                strcpy(fname, optarg);
                break;

            case 'd':
                decrypt = 1;
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

        if ((encrypt == 1) ^ (decrypt == 1)) {
            if (argv[optind] == NULL) {
                fprintf(stderr, "Error: Key device name was not provided\n");
                exit(EXIT_FAILURE);
            } else {
                strcpy(key_dev_name, argv[optind]);

                if (encrypt == 1)
                    sea (fname, key_dev_name, ENCRYPT);
                else
                    sea (fname, key_dev_name, DECRYPT);
            }
        }
    }

    return 0;
}

void sea (char* fname, char* key_dev_name, int encr_decr)
{
    srand(time(NULL));

    int fd_file = open(fname, O_RDONLY);
    int fd_dev = open(key_dev_name, O_RDWR);

    struct stat file_stat;
    int bytes_file = 0;

    struct stat dev_stat;
    unsigned long nblocks_dev = 0;
    int bytes_dev = 0;
    size_t bs = 0;
    
    if (encr_decr == ENCRYPT) {
        char ofname[FILENAME_MAX];
        
        strcpy(ofname, fname);
        strcat(ofname, "_encr");

        int fd_ofile = open(ofname, O_WRONLY | O_CREAT);

        if (fd_ofile == -1) {
            perror("sea: cannot open file for writing");
            exit(EXIT_FAILURE);
        }

        /* -1 if there was error in checking files */
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

                int r_dev = ioctl(fd_dev, BLKGETSIZE, &nblocks_dev);
                if (r_dev == -1) {
                    perror("sea");
                    exit(EXIT_FAILURE);
                }

                fstat(fd_file, &file_stat);
                fstat(fd_dev, &dev_stat);

                bytes_file = file_stat.st_size;

                bytes_dev = nblocks_dev * 512;
                bs = file_stat.st_blksize;

                if (bytes_dev < bytes_file) {
                    fprintf(stderr, "Error: Specified device, has less space,\
                    than the data in the file given\n");
                    exit(EXIT_FAILURE);
                } else {
                    clear_dev(fd_dev, bytes_dev, bs);

                    void* buf = malloc(1);
                    long double nbytes_written = 0;                    

                    while (read(fd_file, buf, 1) != 0) {
                        char ch = *((char*)buf);
                        int shift_size = rand() % 26;
                        ch += shift_size;
                        buf = &ch;

                        int ret_ofile = write(fd_ofile, buf, 1);
                        if (ret_ofile == -1) {
                            perror("sea: could not write to file");
                            exit(EXIT_FAILURE);
                        }

                        int ret_dev = write(fd_dev, (void*)&shift_size, 1);
                        if (ret_dev == -1) {
                            perror("sea: could not write key to device");
                            exit(EXIT_FAILURE);
                        }

                        long double percent = (nbytes_written/bytes_file) * 100;

                        /* Hide cursor */
                        fputs("\e[?25l", stdout);

                        printf("Writing encryption key to device, and writing encrypted file... [%.2Lf%% Done]\r", percent);
                        fflush(stdout);

                         /* Show cursor */
                        fputs("\e[?25h", stdout);

                        buf = malloc(1);
                        nbytes_written += 1;                        
                    }
                    printf("\n\nDone!\n");
                    free(buf);
                }
            }
        }
        close(fd_dev);
        close(fd_file);
    }
}

int check_files (char* fname, char* key_dev_name, int fd_file, int fd_dev)
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

int warning_prompt (char* key_dev_name)
{
    int ret = 1;

    printf("WARNING: The contents in '%s' CANNOT be recovered\n\
         after this operation. If the encryption key for another file\n\
         is stored in this device, it will be removed.\n\n", key_dev_name);

    printf("Do you STILL want to continue? [y/N] ");
    int ch = fgetc(stdin);

    if (ch == 'y' || ch == 'Y') {
        ret = 0;
    } else {
        ret = 1;
    }

    return ret;
}

void clear_dev (int fd_dev, size_t count, size_t bs)
{
    /* Number of bytes written */
    long double nbytes_written = 0;
    char buf[512];

    memset(buf, 0, sizeof(buf));

    while (nbytes_written != count) {
        int ret = write(fd_dev, buf, 512);

        if (ret == -1) {
            perror("sea: could not clear device");
            exit(3);
        }

        long double percent = (nbytes_written/count) * 100;

        /* Hide cursor */
        fputs("\e[?25l", stdout);

        printf("Clearing %ld byte(s). [%.2Lf%% Done]\r", count, percent);
        fflush(stdout);

        /* Show cursor */
        fputs("\e[?25h", stdout);

        nbytes_written += bs;
    }
    printf("\n");
}

void help (char* prog_name)
{
    printf("Usage: %s <-e | -d> <file name> <key device>\n", prog_name);
    printf("Options:\n");
    printf("\t-e, --encrypt <file name>\tEncrypt given file\n");
    printf("\t-d, --decrypt <file name>\tDecrypt given file\n");
    printf("\t-h, --help\t\t\tShow this help message\n");
    printf("\t-V, --version\t\t\tShow version information\n\n");

    printf("NOTE: This program requires root privileges.\n");

    exit(EXIT_SUCCESS);
}

void version (void)
{
    printf("sea 0.1\n");
    printf("Copyright (C) 2019 Jithin Renji.\n");
    printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n\n");

    printf("Written by Jithin Renji.\n");
}