#include "Account.h"
#include "Enclave.h"

namespace AS = AccountSpace;

AS::Account::Account(std::string name_arg, std::string password_arg, uint64_t deposits_arg)
    : name(name_arg), password(password_arg), deposits(deposits_arg) {
    account_data_size = name_arg.size() + password_arg.size() + sizeof(uint64_t);
};

AS::Account::~Account() { delete (this); }

AS::Account* AS::Account::NewAccount(std::string name, std::string password) {
    auto account = new Account(name, password, 0);
    return account;
}

ProcessResult AS::Account::Withdraw(uint64_t amount) {
    if (this->deposits < amount) {
        return ProcessResult::DEPOSIT_OVERFLOW;
    }
    this->deposits -= amount;
    return ProcessResult::PROCESS_SUCCESS;
}

ProcessResult AS::Account::Deposit(uint64_t amount) {
    if (UINT64_MAX - this->deposits < amount) {
        return ProcessResult::DEPOSIT_OVERFLOW;
    }
    this->deposits += amount;
    return ProcessResult::PROCESS_SUCCESS;
}
