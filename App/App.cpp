/*
 * Copyright (C) 2011-2020 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ext/stdio_filebuf.h>

#include <pwd.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#define MAX_PATH FILENAME_MAX

#include "App.h"
#include "Enclave_u.h"
#include "sgx_urts.h"

const std::string PROJECT_PATH = "/opt/intel/sgxsdk/SampleCode/seccap_sgx_bank/";
const int RECEIVE_BUF_SIZE     = 1024;

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char* msg;
    const char* sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {SGX_ERROR_UNEXPECTED, "Unexpected error occurred.", NULL},
    {SGX_ERROR_INVALID_PARAMETER, "Invalid parameter.", NULL},
    {SGX_ERROR_OUT_OF_MEMORY, "Out of memory.", NULL},
    {SGX_ERROR_ENCLAVE_LOST, "Power transition occurred.",
     "Please refer to the sample \"PowerTransition\" for details."},
    {SGX_ERROR_INVALID_ENCLAVE, "Invalid enclave image.", NULL},
    {SGX_ERROR_INVALID_ENCLAVE_ID, "Invalid enclave identification.", NULL},
    {SGX_ERROR_INVALID_SIGNATURE, "Invalid enclave signature.", NULL},
    {SGX_ERROR_OUT_OF_EPC, "Out of EPC memory.", NULL},
    {SGX_ERROR_NO_DEVICE, "Invalid SGX device.",
     "Please make sure SGX module is enabled in the BIOS, and install SGX "
     "driver afterwards."},
    {SGX_ERROR_MEMORY_MAP_CONFLICT, "Memory map conflicted.", NULL},
    {SGX_ERROR_INVALID_METADATA, "Invalid enclave metadata.", NULL},
    {SGX_ERROR_DEVICE_BUSY, "SGX device was busy.", NULL},
    {SGX_ERROR_INVALID_VERSION, "Enclave version was invalid.", NULL},
    {SGX_ERROR_INVALID_ATTRIBUTE, "Enclave was not authorized.", NULL},
    {SGX_ERROR_ENCLAVE_FILE_ACCESS, "Can't open enclave file.", NULL},
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret) {
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist / sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if (ret == sgx_errlist[idx].err) {
            if (NULL != sgx_errlist[idx].sug) printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }

    if (idx == ttl)
        printf(
            "Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer "
            "Reference\" for "
            "more details.\n",
            ret);
}

int initialize_enclave(void) {
    char token_path[MAX_PATH] = {'\0'};
    sgx_launch_token_t token  = {0};
    sgx_status_t ret          = SGX_ERROR_UNEXPECTED;
    int updated               = 0;

    /* try to retrieve the launch token saved by last transaction
     * if there is no token, then create a new one.
     */
    /* try to get the token saved in $HOME */
    const char* home_dir = getpwuid(getuid())->pw_dir;

    if (home_dir != NULL &&
        (strlen(home_dir) + strlen("/") + sizeof(TOKEN_FILENAME) + 1) <= MAX_PATH) {
        /* compose the token path */
        strncpy(token_path, home_dir, strlen(home_dir));
        strncat(token_path, "/", strlen("/"));
        strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME) + 1);
    } else {
        /* if token path is too long or $HOME is NULL */
        strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
    }
    FILE* fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL) {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }
    /* call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        if (fp != NULL) fclose(fp);
        return -1;
    }

    /* save the launch token if it is updated */
    if (updated == FALSE || fp == NULL) {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL) fclose(fp);
        return 0;
    }

    /* reopen the file with write capability */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL) return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

/* OCall functions */
void ocall_print_string(const char* str) {
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    printf("%s", str);
}

/*
    ocall function for get Account data bytes from "account" file.
*/
void ocallAccountListStore(uint8_t* account_list_data, size_t account_data_len) {
    std::ofstream ofs((PROJECT_PATH + "account").c_str(),
                      std::ios::binary | std::ios::out | std::ios::in);

    if (!ofs.good()) {
        printf("write open account file failed.\n");
        exit(EXIT_FAILURE);
    }

    ofs.write(reinterpret_cast<const char*>(account_list_data), account_data_len);

    if (ofs.fail()) {
        printf("write execute account file failed.\n");
        exit(EXIT_FAILURE);
    }

    ofs.close();
}

/*
    ocall function for get sealed account data bytes size from "account" file.
*/
void ocallGetSealedAccountListSize(uint64_t* sealed_account_list_size) {
    FILE* fp = fopen((PROJECT_PATH + "account").c_str(), "rb");

    if (fp == NULL) {
        printf("read account failed.\n");
        exit(EXIT_FAILURE);
    }

    if (fseek(fp, 0L, SEEK_END) == 0) {
        fpos_t pos;

        if (fgetpos(fp, &pos) == 0) {
            fclose(fp);
            *sealed_account_list_size = (uint64_t)pos.__pos;
        }
    }
}

/*
    ocall function for get sealed account data bytes size from "account" file.
*/
void ocallGetSealedAccountList(uint8_t* sealed_account_list_data,
                               uint64_t sealed_account_list_size) {
    if (sealed_account_list_size > 0) {
        std::ifstream ifs((PROJECT_PATH + "account").c_str(), std::ios::binary | std::ios::in);
        if (ifs.is_open()) {
            ifs.read(reinterpret_cast<char*>(sealed_account_list_data), sealed_account_list_size);
            if (ifs.fail()) {
                printf("read account file failed.\n");
            }
            ifs.close();
        } else {
            printf("open account file failed.\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* Application entry */
int SGX_CDECL main(int argc, char* argv[]) {
    (void)(argc);
    (void)(argv);

    /* Initialize the enclave */
    if (initialize_enclave() < 0) {
        printf("Enter a character before exit ...\n");
        getchar();
        return -1;
    }

    enum ProcessResult result;

    int sockfd;
    int client_sockfd;
    struct sockaddr_in addr;

    socklen_t len = sizeof(struct sockaddr_in);
    struct sockaddr_in from_addr;

    char* buf;

    // Initialize DealManager
    ecallInitManager(global_eid, &result);

    // communication preparation
    {
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
        }

        uint16_t port = 1230;
        port += atoi(argv[1]);

        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
        }

        if (listen(sockfd, SOMAXCONN) < 0) {
            perror("listen");
        }

        if ((client_sockfd = accept(sockfd, (struct sockaddr*)&from_addr, &len)) < 0) {
            perror("accept");
        }
    }

    // reception
    int rsize;
    for (;;) {
        for (;;) {
            buf   = (char*)calloc(1, RECEIVE_BUF_SIZE);
            rsize = recv(client_sockfd, buf, RECEIVE_BUF_SIZE, 0);

            if (rsize == 0) {
                break;
            } else if (rsize == -1) {
                perror("recv");
            } else {
                char* data[3] = {NULL};
                int data_size[3];
                int data_iter  = 0;
                int start_iter = 0;
                // split received data
                for (int i = 0; i < RECEIVE_BUF_SIZE; i++) {
                    if (buf[i] == ',') {
                        int end_iter         = i;
                        data_size[data_iter] = end_iter - start_iter;
                        data[data_iter]      = (char*)calloc(1, data_size[data_iter] + 1);
                        std::memcpy(data[data_iter], buf + start_iter, data_size[data_iter]);
                        start_iter = i + 1;
                        data_iter++;
                    }
                }
                // execute server process following received instruction
                {
                    // login process
                    if (strncmp("login", data[0], 5) == 0) {
                        std::cout << "[Server]: ログイン処理スタート\n";
                        // execute Enclave process of login process
                        ecallNewAccount(global_eid, &result, data[1], data[2]);
                        if (result == ProcessResult::ACCOUNT_SET_FAIL) {
                            write(client_sockfd, "1", 1);
                        } else {
                            write(client_sockfd, "0", 1);
                        }
                        // deposit process
                    } else if (strncmp("deposit", data[0], 7) == 0) {
                        std::cout << "[Server]: 預金処理スタート\n";
                        uint64_t deposits;
                        uint64_t amount = atoi(data[1]);

                        // execute Enclave process of deposit process
                        ecallMyDeposit(global_eid, &deposits, amount);

                        char send_data[8];
                        sprintf(send_data, "%ld", deposits);
                        write(client_sockfd, send_data, sizeof(uint64_t));
                    } else if (strncmp("withdraw", data[0], 8) == 0) {
                        std::cout << "[Server]: 引き出し処理スタート\n";
                        uint64_t deposits;
                        uint64_t amount = atoi(data[1]);

                        // execute Enclave process of withdraw process
                        ecallMyWithdraw(global_eid, &deposits, amount);

                        char send_data[8];
                        sprintf(send_data, "%ld", deposits);
                        write(client_sockfd, send_data, sizeof(uint64_t));
                    } else if (strncmp("end", buf, 3) == 0) {
                        goto server_end;
                    }
                    for (int i = 0; i < 3; i++)
                        if (data[i] != NULL) free(data[i]);
                    free(buf);
                    std::cout << "[Server]: 完了\n";
                }
            }
        }
    }

server_end:

    // close socket
    close(client_sockfd);
    close(sockfd);

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    printf("Enclave destroyed.\n");

    return 0;
}
