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

const std::string PROJECT_PATH = "/home/masashi/workspace/seccap/Seccap_NetBank_SGX/";
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

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void) {
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

/* OCall functions */
void ocall_print_string(const char* str) {
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    printf("%s", str);
}

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
            (uint64_t) pos.__pos;
        }
    }
    printf("get account list size end.\n");
}

void ocallGetSealedAccountList(uint8_t* sealed_account_list_data,
                               uint64_t sealed_account_list_size) {
    printf("get account list start.\n");
    if (sealed_account_list_size > 0) {
        std::ifstream ifs((PROJECT_PATH + "account").c_str(), std::ios::binary | std::ios::in);
        if (ifs.is_open()) {
            ifs.read(reinterpret_cast<char*>(sealed_account_list_data), sealed_account_list_size);
            if (ifs.fail()) {
                printf("read account file failed.\n");
            }
            ifs.close();
            printf("get account list end.\n");
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
    ecallInitManager(global_eid, &result);

    int sockfd;
    int client_sockfd;
    struct sockaddr_in addr;

    socklen_t len = sizeof(struct sockaddr_in);
    struct sockaddr_in from_addr;

    char buf[RECEIVE_BUF_SIZE];

    // 受信バッファ初期化
    memset(buf, 0, sizeof(buf));

    // ソケット生成
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
    }

    // 待ち受け用IP・ポート番号設定
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(1234);
    addr.sin_addr.s_addr = INADDR_ANY;

    // バインド
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
    }

    // 受信待ち
    if (listen(sockfd, SOMAXCONN) < 0) {
        perror("listen");
    }

    // クライアントからのコネクト要求待ち
    if ((client_sockfd = accept(sockfd, (struct sockaddr*)&from_addr, &len)) < 0) {
        perror("accept");
    }

    // 受信
    int rsize;
    for (;;) {
        for (;;) {
            rsize = recv(client_sockfd, buf, sizeof(buf), 0);

            if (rsize == 0) {
                break;
            } else if (rsize == -1) {
                perror("recv");
            } else {
                // printf("receive:%s\n", buf);

                char* data[3];
                int data_size[3];
                int data_iter  = 0;
                int start_iter = 0;
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
                // std::cout << data[0] << " process received.\n";
                if (strncmp("login", data[0], 5) == 0) {
                    std::cout << "[Server]: ログイン処理スタート\n";
                    ecallNewAccount(global_eid, &result, data[0], data[1]);
                    // 応答
                    // printf("server send: login success!\n");

                    write(client_sockfd, "login success!", 14);
                } else if (strncmp("deposit", data[0], 7) == 0) {
                    std::cout << "[Server]: 預金処理スタート\n";
                    uint64_t deposits;
                    ecallMyDeposit(global_eid, &deposits, *(uint64_t*)data[1]);
                    // 応答
                    // printf("server send: %ld\n", deposits);
                    write(client_sockfd, (uint8_t*)&deposits, 8);
                } else if (strncmp("withdraw", data[0], 8) == 0) {
                    std::cout << "[Server]: 引き出し処理スタート\n";
                    uint64_t deposits;
                    ecallMyWithdraw(global_eid, &deposits, *(uint64_t*)data[1]);
                    // 応答
                    // printf("server send: %ld\n", deposits);
                    write(client_sockfd, (uint8_t*)&deposits, 8);
                } else if (strncmp("end", buf, 3) == 0) {
                    goto server_end;
                }
                std::cout << "[Server]: 完了\n";
            }
        }
    }

server_end:

    // ソケットクローズ
    close(client_sockfd);
    close(sockfd);

    /* Destroy the enclave */
    // sgx_destroy_enclave(global_eid);

    printf("Enclave destroyed.\n");

    return 0;
}
