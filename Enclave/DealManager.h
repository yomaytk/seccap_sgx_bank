#ifndef _DEAL_MANAGER_H_
#define _DEAL_MANAGER_H_

#include <vector>
#include "Account.h"
#include "sealing.h"

namespace AS = AccountSpace;

namespace Manager {
    /*
        this class process all deal.
    */
    class DealManager {
       public:
        Encryption::SealingUnsealing* sealing_unsealing;
        AS::Account* current_account;
        std::vector<AS::Account*> account_list;

        bool AccountLogin();
        ProcessResult SetCurrentAccount(AS::Account* account);
        void DealMyDeposit(uint64_t amount);
        void DealMyWithdraw(uint64_t amount);
        ProcessResult AccountListStoreStorage();
        ProcessResult AccountListSetting();
        DealManager();
        ~DealManager();
    };
};  // namespace Manager

#endif