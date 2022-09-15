#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    int sockfd;
    struct sockaddr_in addr;
    // ソケット生成
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
    }

    uint16_t port = 1230;
    char addr_num[9];
    port += atoi(argv[1]);
    sprintf(addr_num, "127.0.%s.1", argv[1]);

    // 送信先アドレス・ポート番号設定
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.1.1");

    // サーバ接続
    connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));

    // データ送信
    char send_info[1024];
    char receive_data[1024];

    bool login = false;

    for (;;) {
        printf("操作番号を入力してください．\n");
        if (login) {
            printf("[2] 預金 [3] 引き出し [4] 取引終了\n");
        } else {
            printf("[1] 認証 [2] 預金 [3] 引き出し [4] 取引終了\n");
        }
        int opcode = 0;
        std::cin >> opcode;
        bool deposit_print = false;
        if (opcode == 1) {
            std::cout << "アカウント名及びパスワードを入力してください．\n";
            char account_name[100];
            char password[100];
            std::cin >> account_name;
            std::cin >> password;
            sprintf(send_info, "login,%s,%s,", account_name, password);
            login = true;
        } else if (opcode == 2) {
            std::cout << "預ける金額を入力してください．\n";
            uint64_t amount;
            std::cin >> amount;
            sprintf(send_info, "deposit,%ld,", amount);
            deposit_print = true;
        } else if (opcode == 3) {
            std::cout << "引き出す金額を入力してください．\n";
            uint64_t amount;
            std::cin >> amount;
            sprintf(send_info, "withdraw,%ld,", amount);
            deposit_print = true;
        } else if (opcode == 4) {
            std::cout << "取引を終了します.\n";
            break;
        }

        // printf("send: %s\n", send_info);
        if (send(sockfd, send_info, 1024, 0) < 0) {
            perror("send error!!");
        } else {
            recv(sockfd, receive_data, 1024, 0);
            if (deposit_print) {
                printf("<<現在の合計金額>> %ld 円\n", (uint64_t)atoi(receive_data));
            }
            if (strncmp(receive_data, "login fail.", 11) == 0) {
                printf("パスワードが間違っています．もう一度ログインしてください．\n");
                login = false;
            }
        }
        std::cout << std::endl;
    }

    // ソケットクローズ
    close(sockfd);

    return 0;
}