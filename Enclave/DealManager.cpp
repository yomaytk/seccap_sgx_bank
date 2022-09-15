#include "DealManager.h"
#include <stdio.h>
#include "Account.h"
#include "Enclave.h"
#include "sealing.h"

namespace AS = AccountSpace;

Manager::DealManager::DealManager()
    : sealing_unsealing(new Encryption::SealingUnsealing()),
      current_account(NULL),
      account_list({}) {}

Manager::DealManager::~DealManager() {
    this->AccountListStoreStorage();
    delete (sealing_unsealing);
}

bool Manager::DealManager::AccountLogin() { return current_account != NULL; }

/*
    this method store account list for storage.
    1. marshalling account list to bytes data.
    2. sealing the bytes data.
    3. store the bytes data using ocall function.
*/
ProcessResult Manager::DealManager::AccountListStoreStorage() {
    size_t all_account_data_size = 0;
    for (auto&& account : account_list) {
        all_account_data_size +=
            account->account_data_size + std::strlen(",") + std::strlen(",") + std::strlen("\n");
    }
    uint8_t* account_list_data = (uint8_t*)calloc(1, all_account_data_size);
    uint8_t* data_ptr          = account_list_data;
    // marshall account list
    for (auto&& account : account_list) {
        // copy account info
        // bytes: name,password,deposits
        std::memcpy(data_ptr, account->name.c_str(), account->name.size());
        data_ptr += account->name.size();

        *data_ptr = ',';
        data_ptr += 1;

        std::memcpy(data_ptr, account->password.c_str(), account->password.size());
        data_ptr += account->password.size();

        *data_ptr += ',';
        data_ptr += 1;

        std::memcpy(data_ptr, &account->deposits, sizeof(uint64_t));
        data_ptr += sizeof(uint64_t);

        *data_ptr += '\n';
        data_ptr += 1;
    }

    assert((uint64_t)(data_ptr - account_list_data) == all_account_data_size);

    auto sealing_data_info =
        this->sealing_unsealing->sealing(account_list_data, all_account_data_size, 10000);
    uint8_t* sealing_data      = sealing_data_info.first;
    uint64_t sealing_data_size = (uint64_t)sealing_data_info.second;

    ocallAccountListStore(sealing_data, sealing_data_size);

    return ProcessResult::PROCESS_SUCCESS;
}

/*
    the method set account list after reading account bytes data from storage.
    1. get sealed account list bytes data from storage using ocall function.
    2. unsealing the bytes data.
    3. marshalling the bytes data to account list
*/
ProcessResult Manager::DealManager::AccountListSetting() {
    uint64_t sealed_account_list_size = 0;
    ocallGetSealedAccountListSize(&sealed_account_list_size);
    if (sealed_account_list_size > 0) {
        uint8_t* sealed_account_list_data = (uint8_t*)calloc(1, sealed_account_list_size);
        ocallGetSealedAccountList(sealed_account_list_data, sealed_account_list_size);
        auto unsealing_data_info   = this->sealing_unsealing->unsealing(sealed_account_list_data);
        uint8_t* account_list_data = unsealing_data_info.first;
        uint64_t account_list_size = (uint64_t)unsealing_data_info.second;
        uint8_t* data_ptr          = account_list_data;
        for (;;) {
            std::string name, password;
            uint64_t deposits;
            uint8_t* name_start_ptr = data_ptr;
            for (;; data_ptr++) {
                if (*data_ptr == ',') {
                    name = std::string(name_start_ptr, data_ptr);
                    data_ptr++;
                    break;
                }
            }

            uint8_t* password_start_ptr = data_ptr;
            for (;; data_ptr++) {
                if (*data_ptr == ',') {
                    password = std::string(password_start_ptr, data_ptr);
                    data_ptr++;
                    break;
                }
            }

            uint8_t* deposits_start_ptr = data_ptr;
            deposits                    = *(uint64_t*)deposits_start_ptr;
            data_ptr += sizeof(uint64_t);

            assert(*data_ptr++ == '\n');

            this->account_list.push_back(new AS::Account(name, password, deposits));

            if (data_ptr - account_list_data == account_list_size) {
                break;
            }
        }
    }
}

ProcessResult Manager::DealManager::SetCurrentAccount(AS::Account* account) {
    this->current_account = account;
    return ProcessResult::PROCESS_SUCCESS;
}

void Manager::DealManager::DealMyDeposit(uint64_t amount) {
    this->current_account->Deposit(amount);
}

void Manager::DealManager::DealMyWithdraw(uint64_t amount) {
    this->current_account->Withdraw(amount);
}