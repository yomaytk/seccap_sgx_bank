#include "Enclave.h"
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <cstdio>  /* vsnprintf */
#include <cstring>
#include <string>
#include <utility>
#include "Account.h"
#include "DealManager.h"
#include "Enclave_t.h" /* print_string */
#include "sgx_trts.h"

Manager::DealManager* DEAL_MANAGER = NULL;

int printf(const char* fmt, ...) {
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return (int)strnlen(buf, BUFSIZ - 1) + 1;
}

/*
    ecall function for Initialize DealManager
*/
ProcessResult ecallInitManager() {
    DEAL_MANAGER = new Manager::DealManager();
    DEAL_MANAGER->AccountListSetting();
    printf("[Server]: サーバー初期設定完了\n");
    return ProcessResult::PROCESS_SUCCESS;
}

/*
    ecall function for setting new account for DealManager
*/
ProcessResult ecallNewAccount(const char* name, const char* password) {
    ProcessResult result;
    bool account_exist = false;
    for (auto&& account : DEAL_MANAGER->account_list) {
        if (name == account->name) {
            result = DEAL_MANAGER->SetCurrentAccount(account);
            if (strcmp(password, account->password.c_str()) != 0) {
                return ProcessResult::ACCOUNT_SET_FAIL;
            }
            account_exist = true;
            break;
        }
    }

    /*
        if new account is generated, store it for storage.
    */
    if (!account_exist) {
        result = DEAL_MANAGER->SetCurrentAccount(
            AccountSpace::Account::NewAccount(std::string(name), std::string(password)));
        DEAL_MANAGER->account_list.push_back(DEAL_MANAGER->current_account);
        DEAL_MANAGER->AccountListStoreStorage();
    }

    if (result != PROCESS_SUCCESS) {
        return ProcessResult::ACCOUNT_SET_FAIL;
    }

    return ProcessResult::PROCESS_SUCCESS;
}

/*
    ecall function for calculating for balance by deposit process
*/
uint64_t ecallMyDeposit(uint64_t amount) {
    if (!DEAL_MANAGER->AccountLogin()) {
        return ProcessResult::NEED_LOGOUT;
    }
    DEAL_MANAGER->DealMyDeposit(amount);
    DEAL_MANAGER->AccountListStoreStorage();
    return DEAL_MANAGER->current_account->deposits;
}

/*
    ecall function for calculating for balance by withdraw process
*/
uint64_t ecallMyWithdraw(uint64_t amount) {
    if (!DEAL_MANAGER->AccountLogin()) {
        return ProcessResult::NEED_LOGOUT;
    }
    DEAL_MANAGER->DealMyWithdraw(amount);
    DEAL_MANAGER->AccountListStoreStorage();
    return DEAL_MANAGER->current_account->deposits;
}

uint64_t ecallMyInquiry() { return DEAL_MANAGER->current_account->deposits; }