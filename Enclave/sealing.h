#ifndef _SEALING_H_
#define _SEALING_H_

#include <sgx_tseal.h>
#include <cstdint>
#include <string>
#include <utility>

namespace Encryption {
    class SealingUnsealing {
       public:
        std::string add_mac_txt;
        SealingUnsealing();
        ~SealingUnsealing();
        uint16_t get_sealed_data_size(uint32_t txt_encrypt_size, uint32_t limit_sealed_data_size);
        std::pair<uint8_t*, uint16_t> sealing(const uint8_t* secret_plain_data,
                                              uint16_t secret_plain_data_size,
                                              uint32_t limit_sealed_data_size);
        std::pair<uint8_t*, uint16_t> unsealing(const uint8_t* encrypted_secret_data);
    };

}  // namespace Encryption

#endif