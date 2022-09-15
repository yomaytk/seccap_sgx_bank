#include "sealing.h"
#include <stdio.h>
#include <cstring>
#include <limits>
#include <memory>
#include "Enclave.h"

Encryption::SealingUnsealing::SealingUnsealing() : add_mac_txt(""){};

Encryption::SealingUnsealing::~SealingUnsealing(){};

/*
    calculate encrypted_size from received target plain bytes data.
*/
uint16_t Encryption::SealingUnsealing::get_sealed_data_size(uint32_t txt_encrypt_size,
                                                            uint32_t limit_sealed_data_size) {
    uint32_t sealed_data_size = sgx_calc_sealed_data_size(
        static_cast<uint32_t>(std::strlen(add_mac_txt.c_str())), txt_encrypt_size);
    if (static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) < sealed_data_size) {
        printf("sealed data size too big. sealed_data_size: %d\n", sealed_data_size);
    }
    if (limit_sealed_data_size < sealed_data_size) {
        printf("sealed data size exceeds limit_sealed_data_size. sealed_data_size: %d\n",
               sealed_data_size);
    }
    return static_cast<uint16_t>(sealed_data_size);
}

/*
    sealing method
*/
std::pair<uint8_t*, uint16_t> Encryption::SealingUnsealing::sealing(
    const uint8_t* secret_plain_data, uint16_t secret_plain_data_size,
    uint32_t limit_sealed_data_size) {
    // calculate sealed data size
    uint16_t sealed_data_size =
        get_sealed_data_size(secret_plain_data_size, limit_sealed_data_size);
    // execute sealing
    uint8_t* sealed_data_buf = (uint8_t*)calloc(1, sealed_data_size);
    if (sealed_data_buf == NULL) {
        printf("sealed_data_buf allocate fail.");
    }
    sgx_status_t sealing_result =
        sgx_seal_data((uint32_t)std::strlen(add_mac_txt.c_str()),
                      (const uint8_t*)add_mac_txt.c_str(), (uint32_t)secret_plain_data_size,
                      secret_plain_data, sealed_data_size, (sgx_sealed_data_t*)sealed_data_buf);

    if (sealing_result != SGX_SUCCESS) {
        printf("sealing failed. sealed_data_size: %d, secret_plain_data_size: %d\n",
               sealed_data_size, secret_plain_data_size);
    }

    return std::make_pair(sealed_data_buf, sealed_data_size);
}

/*
    unsealing method
*/
std::pair<uint8_t*, uint16_t> Encryption::SealingUnsealing::unsealing(
    const uint8_t* encrypted_secret_data) {
    // calculate mac txt and decrypted secret data len
    uint32_t mac_txt_len = sgx_get_add_mac_txt_len((const sgx_sealed_data_t*)encrypted_secret_data);
    uint32_t decrypted_data_len =
        sgx_get_encrypt_txt_len((const sgx_sealed_data_t*)encrypted_secret_data);

    // unsealing
    uint8_t* decrypted_mac_txt     = (uint8_t*)calloc(1, mac_txt_len);
    uint8_t* decrypted_secret_data = (uint8_t*)calloc(1, decrypted_data_len);
    sgx_status_t unsealing_result =
        sgx_unseal_data((const sgx_sealed_data_t*)encrypted_secret_data, decrypted_mac_txt,
                        &mac_txt_len, decrypted_secret_data, &decrypted_data_len);

    if (unsealing_result != SGX_SUCCESS) {
        printf("unsealing failed. mac_txt_len: %d, decrypted_data_len: %d\n", mac_txt_len,
               decrypted_data_len);
    }

    assert(!std::memcmp((const char*)decrypted_mac_txt, add_mac_txt.c_str(), mac_txt_len));
    free(decrypted_mac_txt);

    return std::make_pair(decrypted_secret_data, decrypted_data_len);
}
