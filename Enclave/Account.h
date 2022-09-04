#ifndef _ACCOUNT_H_
#define _ACCOUNT_H_

#include <string>
#include "Enclave_t.h"
#include "user_types.h"

namespace AccountSpace {
    class Account {
       public:
        std::string name;
        std::string password;
        uint64_t deposits;
        size_t account_data_size;
        Account(std::string name, std::string password, uint64_t deposit);
        ~Account();
        static Account* NewAccount(std::string name, std::string password);
        ProcessResult Withdraw(uint64_t amount);
        ProcessResult Deposit(uint64_t amount);
    };
};  // namespace AccountSpace

#endif
