#include <err.h>
#include <sys/file.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

#define OPTIONS              "t:l:"
#define BUF_SIZE             4096
#define DEFAULT_THREAD_COUNT 4

static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 10028) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
void logging(int code, char *method_name, char *file, int rId) {
    flock(fileno(logfile), LOCK_EX);
    LOG("%s,%s,%d,%d\n", method_name, file, code, rId);
    fflush(logfile);
    flock(fileno(logfile), LOCK_UN);
}

void response(int fd, int code, char *message, int length) {
    printf("responding\n");
    flock(fd, LOCK_EX);
    if (code == 404) {
        char *output = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
        printf("not found\n");
        write(fd, output, strlen(output));
    } else if (code == 200) {
        // Successful GET response
        if (!strcmp("getting", message)) {
            struct stat st;
            fstat(length, &st);
            write(fd, "HTTP/1.1 200 OK\r\nContent-Length: ", 33);
            char number[24];
            sprintf(number, "%d", (int) st.st_size);
            write(fd, number, strlen(number));
            write(fd, "\r\n\r\n", 4);
            char *buffer = malloc(2048);
            int bytess = 0;
            flock(length, LOCK_SH);
            while ((bytess = read(length, buffer, 2048)) > 0) {
                write(fd, buffer, bytess);
            }
            flock(length, LOCK_UN);
            free(buffer);
            close(length);

            // Successful PUT/APPEND RESPONSE

        } else {

            write(fd, "HTTP/1.1 200 OK\r\n", 17);
            write(fd, "Content-Length: 3\r\n\r\nOK\n", 24);
        }
    } else if (code == 400) {
        char *output = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
        write(fd, output, strlen(output));
    } else if (code == 501) {
        char *output
            = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
        write(fd, output, strlen(output));
    } else if (code == 403) {
        char *output = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
        write(fd, output, strlen(output));
    } else if (code == 201) {
        char *output = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
        write(fd, output, strlen(output));
    } else if (code == 500) {
        char *output = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal "
                       "Server Error\n";
        write(fd, output, strlen(output));
    }
    printf("responded %d\n", code);

    flock(fd, LOCK_UN);
    //close(fd);
    return;
}

/**
	Frees buffer, array storing different parts of request, and other allocated memory.
*/
void freeing(
    char *http, char **parts, char *method, char *uri, char *buffer, int count, char *buffer_cpy) {
    free(buffer);
    free(buffer_cpy);
    free(method);
    free(http);
    free(uri);
    for (int i = 0; i < count - 1; i++) {
        free(parts[i]);
    }
    free(parts);

    return;
}

static void handle_connection(int connfd) {

    int buf_size = 2048;
    char *buffer = calloc(2048, sizeof(char));
    int bytes_read;
    int rncount = 0;

    //	while(1) {
    //bytes_read = recv(connfd, buffer, buf_size, 0);
    bytes_read = read(connfd, buffer, buf_size);
    if (bytes_read == 0) {
        //			printf("%s\n", buffer);
        return;
    }
    //	}
    /*
	if (bytes_read == 0) {
		printf("read, %d\n", bytes_read);
        printf("zero, %d\n", connfd);	
	//	return;
    } else {
        printf("good, %d\n", connfd);
    }
*/
    //PARSING INPUT
    int seg = 0;

    //printf("buffer: %s\n", buffer);

    char *buffer_cpy = calloc(2049, sizeof(char));
    char *buffer_cpy2 = calloc(2048, sizeof(char));

    int z = 1;
    int y = 1;
    int start = 0;
    int start2 = 0;
    int end = 0;
    int first = 0;
    //	int count = 0;
    do {
        //	printf("do\n");
        /*
		if(seg) {
			while (bytes_read > 0) {
				bytes_read = read(connfd, buffer, buf_size);
			}

			for(int i = 0; i < bytes_read; i++) {
				buffer_cpy[end+i] = buffer[i];
			}
			break;
		}else {
*/
        //       long long times = 0;
        while (bytes_read <= 0) {
            bytes_read = read(connfd, buffer, buf_size - end);
            //            times++;
            //if(times > 1000) {
            //	printf("toomany\n");
            //	return;
            //	}
        }

        printf("%d\n", bytes_read);
        for (int i = 0; i < bytes_read; i++) {
            buffer_cpy[end + i] = buffer[i];
        }
        printf("end: %d\n", end);
        //		}

        //	printf("got thru\n");
        z = 0;
        for (int i = 0; i < end + bytes_read - 1; i++) {
            //	printf("%c\n", buffer_cpy[i]);
            if (buffer_cpy[i] == '\r' && buffer_cpy[i + 1] == '\n') {
                if (y) {
                    first = i;
                    y = 0;
                }
                rncount++;
                if (i + 3 < end + bytes_read) {
                    if (buffer_cpy[i + 2] == '\r' && buffer_cpy[i + 3] == '\n') {
                        rncount++;
                        start = i + 4;
                        start2 = i + 4;

                        end += bytes_read;
                        seg = 1;
                        break;
                    }
                }
            }
        }
        if (!seg) {
            printf("!seg\n");
            end += bytes_read;
            if (end > 2048) {
                end = 2048;
            }
        }
        bytes_read = 0;
    } while (!seg);

    if (!seg) {
        //	printf("error\n");
        bytes_read = read(connfd, buffer, buf_size);
        free(buffer);
        free(buffer_cpy);
        //potential error
        response(connfd, 400, "", 0);
        return;
    }

    //	printf("%s\n", buffer_cpy);
    char *method_name = calloc(2048, sizeof(char));
    char *uri = calloc(2048, sizeof(char));
    char *http = calloc(2048, sizeof(char));
    char *rest = NULL;

    printf("potential error\n");
    memcpy(buffer_cpy2, buffer_cpy, 2048);
    buffer_cpy[2048] = '\0';
    char delim[] = "\r\n";
    int count = 1;
    char *bufy = buffer_cpy;

    rest = strtok_r(buffer_cpy, delim, &bufy);
    printf("rest: %s\n", rest);
    if (rest == NULL) {
        printf("REST ==NULL\n");
        response(connfd, 400, "", 0);
        free(http);
        free(method_name);
        free(uri);
        free(buffer);
        free(buffer_cpy);
        free(buffer_cpy2);
        return;
    }
    char **parts = malloc(rncount * sizeof(char *));

    //printf("%d\n", rncount);
    parts[0] = calloc(2049, sizeof(char));
    memcpy(parts[0], rest, 2048);
    printf("parts0%s\n", parts[0]);
    while (count < rncount - 1) {
        rest = strtok_r(NULL, delim, &bufy);
        //  printf("%s\n", rest);

        parts[count] = calloc(2048, sizeof(char));
        if (rest != NULL) {
            int length = strlen(rest);
            memcpy(parts[count], rest, length + 1);
        }

        //printf("%d %s\n", count, rest);
        count++;
    }
    //rest = strtok(NULL, delim);
    int vars = 0;
    if (seg) {
        printf("sscanf %s\n", parts[0]);
        vars = sscanf(parts[0], "%s %s %s", method_name, uri, http);
        printf("%s %s %s\n", method_name, uri, http);
        printf("%d\n", vars);
        if (uri[0] != '/' || strcmp(http, "HTTP/1.1")) {
            if (uri[0] == '/' && !strcmp(http, "HTTP/1.2")) {
                if (strcmp(method_name, "GET") && strcmp(method_name, "PUT")
                    && strcmp(method_name, "APPEND")) {
                    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                    response(connfd, 501, "", 0);
                    return;
                }
            }
            printf("400\n");
            response(connfd, 400, "", 0);
            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
            return;
        }
        printf("uri\n");
        char num[3000];
        int content_num = -69;
        int reqID = 0;
        int errors2 = 0;
        char content[3000];
        int valid = 1;
        int valid2 = 1;
        for (int i = 1; i < rncount - 1; i++) {
            int scanned;
            if (parts[i] != NULL) {
                printf("%s\n", parts[i]);
                scanned = sscanf(parts[i], "%s %s", content, num);
            } else {
                scanned = -1;
            }
            // printf("%s %s %d\n", content, num, scanned);

            if (scanned == 2) {
                if (content[strlen(content) - 1] == ':' && parts[i][strlen(content)] == ' '
                    && parts[i][strlen(content) + 1] != ' ') {
                    if (!strcmp(content, "Content-Length:") && content_num < 0) {
                        for (int i = 0; i < (int) strlen(num); i++) {
                            if (num[i] < 48 || num[i] > 57) {
                                valid = 0;
                            }
                        }
                        if (valid) {
                            int y = atoi(num);
                            content_num = y;
                        }
                    }
                    if (!strcmp(content, "Request-Id:")) {
                        //           printf("REQUEST\n");
                        for (int i = 0; i < (int) strlen(num); i++) {
                            if (num[i] < 48 || num[i] > 57) {
                                valid2 = 0;
                            }
                        }
                        if (valid2) {
                            int y = atoi(num);
                            reqID = y;
                        }
                    }
                } else {
                    //     errors2 = 1;
                    //    break;
                }

            } else {
                //    errors2 = 1;
                //  break;
            }
        }

        if (vars == 3) {
            //printf("equals three\n");
            if (strcmp(method_name, "GET") && strcmp(method_name, "PUT")
                && strcmp(method_name, "APPEND")) {

                freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                response(connfd, 501, "", 0);
                return;
            }
        }
        //printf("no\n");
        if (errors2) {
            //  printf("error\n");
            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
            printf("error2\n");
            response(connfd, 400, "", 0);
            return;
        }

        if (vars == 3) {
            printf("stillhere\n");
            //GET
            if (!strcmp(method_name, "GET")) {
                if (uri[0] == '/' && !strcmp(http, "HTTP/1.1")) {
                    int fd;
                    char *string = malloc(100);
                    memcpy(string, uri + 1, 100);
                    //        printf("%s\n", string);
                    if ((fd = open(string, O_RDWR)) == -1) {
                        pthread_mutex_lock(&mutex2);
                        if (errno == EISDIR) {
                            logging(403, "GET", uri, reqID);
                            response(connfd, 403, "", 0);
                        } else {
                            fd = open(string, O_RDONLY);
                            if (fd == -1) {
                                fd = access(string, F_OK);
                                if (fd != -1) {
                                    logging(403, "GET", uri, reqID);
                                    response(connfd, 403, "", 0);
                                } else {
                                    logging(404, "GET", uri, reqID);
                                    response(connfd, 404, "", 0);
                                }
                            }
                        }
                        pthread_mutex_unlock(&mutex2);
                    } else {
                        //	pthread_mutex_lock(&mutex2);
                        response(connfd, 200, "getting", fd);
                        logging(200, "GET", uri, reqID);
                        //	pthread_mutex_unlock(&mutex2);
                        //close(fd);
                    }
                    free(string);
                    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                    free(buffer_cpy2);
                    return;
                } else {
                    printf("BAD GET\n");
                    response(connfd, 400, "Bad Request\n", 12);
                    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                    free(buffer_cpy2);
                    return;
                }
                // PUT
            } else if (!strcmp(method_name, "PUT")) {
                //  printf("yes\n");
                if (uri[0] == '/' && !strcmp(http, "HTTP/1.1")) {
                    int fd;

                    //    printf("putting");
                    if (content_num < 0) {
                        printf("Content_num < 0\n");
                        response(connfd, 400, "", 0);
                        freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                        return;
                    }
                    int yes = 0;
                    char *string = malloc(100);
                    memcpy(string, uri + 1, strlen(uri));

                    if ((fd = open(string, O_WRONLY)) != -1) {
                        close(fd);
                        //    remove(string);
                    } else {
                        if (errno == EACCES) {
                            fd = access(string, F_OK);
                            if (fd != -1) {
                                logging(403, "PUT", uri, reqID);
                                close(fd);
                                response(connfd, 403, "", 0);
                            }
                            free(string);
                            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                            return;
                        } else if (errno == EISDIR) {
                            logging(403, "PUT", uri, reqID);
                            response(connfd, 403, "", 0);
                            free(string);
                            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                            return;
                        }
                        //yes = 1;
                    }
                    struct stat sb;
                    if (stat(string, &sb) == 0 && sb.st_mode & S_IXUSR) {
                        printf("500\n");
                        logging(500, "PUT", uri, reqID);
                        response(connfd, 500, "", 0);
                        free(string);
                        freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                        return;
                    }
                    int no = 0;
                    for (int i = 0; i < (int) strlen(uri); i++) {
                        if (string[i] == '/') {
                            no = 1;
                            break;
                        }
                    }
                    if (no) {
                        //        int fd2;
                        logging(500, "PUT", uri, reqID);
                        response(connfd, 500, "", 0);
                        free(string);
                        freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                        return;
                    }

                    // fd = open(string, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
                    char path[L_tmpnam];
                    tmpnam(path);
                    fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
                    //	flock(fd, LOCK_EX);
                    int chars = end - start2;

                    printf("chars%d\n", chars);
                    int written;
                    if (chars < content_num) {
                        written = write(fd, buffer_cpy2 + start, chars);

                    } else {
                        written = write(fd, buffer_cpy2 + start, content_num);
                    }
                    printf("writingtotemp\n");

                    // printf("written%d\n", written);
                    content_num -= written;

                    char *buffer2 = malloc(2048);
                    while (content_num > 0) {
                        bytes_read = read(connfd, buffer2, 2048);
                        if (content_num < bytes_read) {
                            write(fd, buffer2, content_num);
                        } else {
                            write(fd, buffer2, bytes_read);
                        }
                        content_num -= bytes_read;
                    }
                    printf("done writing\n");
                    free(buffer2);
                    //	flock(fd, LOCK_UN);
                    //  close(fd);

                    //fseek(tmp, 0, SEEK_SET);
                    //char *buffer22 = malloc(2048);
                    pthread_mutex_lock(&mutex2);
                    if ((fd = open(string, O_WRONLY)) != -1) {
                        flock(fd, LOCK_EX);
                        close(fd);
                        //    remove(string);
                    } else {
                        if (errno == EACCES) {
                            fd = access(string, F_OK);
                            if (fd != -1) {
                                logging(403, "PUT", uri, reqID);
                                close(fd);
                                response(connfd, 403, "", 0);
                            }
                            free(string);
                            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                            return;
                        } else if (errno == EISDIR) {
                            logging(403, "PUT", uri, reqID);
                            response(connfd, 403, "", 0);
                            free(string);
                            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                            return;
                        }
                        yes = 1;
                    }
                    rename(path, string);
                    flock(fd, LOCK_UN);
                    pthread_mutex_unlock(&mutex2);

                    //	free(buffer22);
                    //pthread_mutex_lock(&mutex3);
                    if (yes) {
                        response(connfd, 201, "", 0);
                        logging(201, "PUT", uri, reqID);
                    } else {
                        response(connfd, 200, "OK\n", 3);
                        logging(200, "PUT", uri, reqID);
                    }
                    pthread_mutex_unlock(&mutex2);
                    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                    free(buffer_cpy2);
                    free(string);
                    return;

                } else {
                    response(connfd, 400, "", 12);
                    printf("BAD PUT\n");
                    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                    return;
                }

                //APPEND
            } else if (!strcmp(method_name, "APPEND")) {
                int fd;
                if (content_num < 0) {
                    printf("conten_num< 0 append\n");
                    response(connfd, 400, "", 0);
                    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                    return;
                }
                char *string = calloc(100, sizeof(char));
                memcpy(string, uri + 1, strlen(uri));

                int no = 0;
                for (int i = 0; i < (int) strlen(uri); i++) {
                    if (string[i] == '/') {
                        no = 1;
                        break;
                    }
                }
                if (no) {
                    int fd2;
                    if ((fd2 = access(string, F_OK)) != -1) {
                        logging(500, "APPEND", uri, reqID);
                        response(connfd, 500, "", 0);
                        free(string);
                        freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                        free(buffer_cpy2);
                        return;
                    }
                }

                fd = open(string, O_WRONLY | O_APPEND);

                if (fd == -1) {
                    if (errno == EACCES) {
                        /*		struct stat sb;
                        if (stat(string, &sb) == 0 && sb.st_mode & S_IXUSR) {
                            logging(500, "APPEND", uri, reqID);
                            response(connfd, 500, "", 0);
                            free(string);
                            freeing(http, parts, method_name, uri, buffer, rncount);
                            close(fd);
                            return;
                        }
*/
                        fd = access(string, F_OK);
                        if (fd == -1) {
                            logging(404, "APPEND", uri, reqID);
                            response(connfd, 404, "", 0);
                        } else {
                            logging(403, "APPEND", uri, reqID);
                            response(connfd, 403, "", 0);
                        }
                        free(string);
                        freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                        free(buffer_cpy2);
                    } else {
                        pthread_mutex_lock(&mutex2);
                        if (errno == EISDIR) {
                            logging(403, "APPEND", uri, reqID);
                            response(connfd, 403, "", 0);

                        } else {
                            struct stat sb;
                            if (stat(string, &sb) == 0 && sb.st_mode & S_IXUSR) {
                                logging(500, "APPEND", uri, reqID);
                                response(connfd, 500, "", 0);
                                freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                                pthread_mutex_unlock(&mutex3);
                                return;
                            }
                            logging(404, "APPEND", uri, reqID);
                            response(connfd, 404, "", 12);
                            freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                            free(buffer_cpy2);
                            free(string);
                        }
                        pthread_mutex_unlock(&mutex2);
                    }
                    return;
                } else {
                    /*
                    struct stat sb;
                    if (stat(string, &sb) == 0 && sb.st_mode & S_IXUSR) {
                        logging(500, "APPEND", uri, reqID);
                        response(connfd, 500, "", 0);
                        freeing(http, parts, method_name, uri, buffer, rncount);
                        free(string);
                        return;
                    }*/
                    int chars = end - start2;
                    close(fd);

                    FILE *tmp = tmpfile();
                    int written;
                    //	flock(fd, LOCK_EX);

                    if (chars < content_num) {
                        written = write(fileno(tmp), buffer_cpy2 + start, chars);
                    } else {
                        written = write(fileno(tmp), buffer_cpy2 + start, content_num);
                    }

                    //  printf("written%d\n", written);
                    content_num -= written;

                    char *buffer2 = malloc(2048);
                    while (content_num > 0) {
                        bytes_read = read(connfd, buffer2, 2048);
                        if (content_num < bytes_read) {
                            write(fileno(tmp), buffer2, content_num);
                        } else {
                            write(fileno(tmp), buffer2, bytes_read);
                        }
                        content_num -= bytes_read;
                    }
                    //   close(fd);

                    fseek(tmp, 0, SEEK_SET);
                    free(buffer2);
                    buffer2 = malloc(2048);
                    fd = open(string, O_WRONLY | O_APPEND);
                    flock(fd, LOCK_EX);
                    while (1) {
                        bytes_read = read(fileno(tmp), buffer2, 2048);
                        write(fd, buffer2, bytes_read);
                        if (bytes_read < 2048) {
                            break;
                        }
                    }
                    flock(fd, LOCK_UN);
                    close(fd);
                    close(fileno(tmp));
                    free(buffer2);
                    pthread_mutex_lock(&mutex2);
                    logging(200, "APPEND", uri, reqID);
                    response(connfd, 200, "OK\n", 3);
                    pthread_mutex_unlock(&mutex2);
                }

                free(buffer_cpy2);
                freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
                free(string);
                return;

            } else {
                response(connfd, 501, "", 12);
            }
        } else {
            printf("bade end \n");
            response(connfd, 400, "", 12);
        }
    } else {
        printf("bad end\n");
        response(connfd, 400, "", 12);
    }

    freeing(http, parts, method_name, uri, buffer, rncount, buffer_cpy);
    return;
    /*
    char buf[BUF_SIZE];

    ssize_t bytes_read, bytes_written, bytes;
    do {
        // Read from connfd until EOF or error.
        bytes_read = read(connfd, buf, sizeof(buf));
        if (bytes_read < 0) {
            return;
        }

        // Write to stdout.
        bytes = 0;
        do {
            bytes_written = write(STDOUT_FILENO, buf + bytes, bytes_read - bytes);
            if (bytes_written < 0) {
                return;
            }
            bytes += bytes_written;
        } while (bytes_written > 0 && bytes < bytes_read);

        // Write to connfd.
        bytes = 0;
        do {
            bytes_written = write(connfd, buf + bytes, bytes_read - bytes);
            if (bytes_written < 0) {
                return;
            }
            bytes += bytes_written;
        } while (bytes_written > 0 && bytes < bytes_read);
    } while (bytes_read > 0);
	*/
}

pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int threads2 = 0;
int join = 0;
int count = 0;
int *buffer;
pthread_t *threadArr;

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        pthread_mutex_lock(&lock);
        count = 1;
        printf("sig\n");
        join = 1;
        pthread_cond_broadcast(&empty);
        pthread_mutex_unlock(&lock);
        printf("threads %d\n", threads2);
        for (int i = 0; i < threads2; i++) {
            printf("%d\n", i);
            pthread_mutex_lock(&lock);
            pthread_cond_broadcast(&empty);
            pthread_mutex_unlock(&lock);
            pthread_join(threadArr[i], NULL);
        }
        free(buffer);
        free(threadArr);
        warnx("received SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t full_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t empty_lock = PTHREAD_MUTEX_INITIALIZER;

int in = 0, out = 0;

int threads_busy = 0;
int threads_waiting;
int queue_size = 100;
int connfd;
sem_t sem;

void *thread_function(void *arg) {
    if (arg == NULL) {
    }
    while (1) {
        pthread_mutex_lock(&lock);
        printf("count: %d\n", count);
        while (!count) {
            printf("waiting");
            pthread_cond_wait(&empty, &lock);
        }
        if (join) {
            printf("joiining\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        //flock(fileno)
        int connfd2 = buffer[out];

        count -= 1;
        out = (out + 1) % 2048;
        pthread_mutex_unlock(&lock);

        handle_connection(connfd2);
        printf("closing\n");
        close(connfd2);
        pthread_mutex_lock(&lock);
        pthread_cond_signal(&full);
        if (join) {
            printf("joiining2\n");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        pthread_mutex_unlock(&lock);
        /*		for(int i = 0; i < 100; i++) {
			printf("%d\n", buffer[i]);
		}*/
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int opt = 0;
    logfile = stderr;

    int threads = DEFAULT_THREAD_COUNT;
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);
    threads_waiting = threads;
    threads2 = threads;
    buffer = calloc(2048, sizeof(int));

    threadArr = malloc(sizeof(pthread_t) * threads);
    for (int i = 0; i < threads; i++) {
        pthread_create(&threadArr[i], NULL, thread_function, NULL);
    }
    int listenfd = create_listen_socket(port);

    for (;;) {
        printf("accepting\n");
        connfd = accept(listenfd, NULL, NULL);
        printf("doneaccept\n");
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        printf("%d\n", connfd);
        pthread_mutex_lock(&lock);
        while (count >= 2048) {
            printf("full\n");
            pthread_cond_wait(&full, &lock);
        }
        printf("donewaiting\n");
        buffer[in] = connfd;
        in = (in + 1) % 2048;
        count += 1;
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);
    }

    return EXIT_SUCCESS;
}
