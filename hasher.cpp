#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <openssl/md5.h>
#include <cctype> // Add this line for isprint

bool debug_mode = false; // Global flag for debugging

// Function to calculate MD5 hash using OpenSSL's MD5 library
std::string calculate_md5(const char* data, size_t size) {
    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    // Calculate MD5 hash for the current variant
    MD5_Update(&md5Context, data, size);

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    char md5String[32 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        sprintf(&md5String[i * 2], "%02x", (unsigned int)result[i]);
    }
    md5String[32] = '\0';

    return md5String;
}

// Function to process variants and find collisions
void process_variants(const char* file_path, const char* target_hash) {
    std::ifstream file(file_path, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << file_path << "\n";
        std::exit(1);
    }

    size_t bit_position = 0;
    char buffer[1]; // Process one byte at a time

    while (file.read(buffer, 1)) {
        for (size_t j = 0; j < 8; ++j) {
            char variant = buffer[0];

            // Flip the j-th bit
            variant ^= (1 << j);

            // Calculate MD5 hash for the current variant
            std::string hashed_value = calculate_md5(&variant, 1);

            // Check for collision
            if (hashed_value == target_hash) {
                size_t changed_bit_position = bit_position % 8;
                char original_value = buffer[0];

                std::cout << "Collision found! Original hash: " << target_hash << "\n";
                std::cout << "Bit position within byte changed to: " << changed_bit_position << "\n";
                std::cout << "Hex value changed to: " << std::hex << static_cast<int>(variant) << "\n";
                std::cout << "Original value of changed byte: " << std::hex << static_cast<int>(original_value) << "\n";
                std::exit(0); // Exit the entire program when a collision is found
            }

            ++bit_position;

            if (debug_mode) {
                size_t current_bit_position = bit_position % 8;

                std::cout << "Bit Position within byte: " << current_bit_position << "\n";
                std::cout << "Variant (ASCII): ";
                if (std::isprint(variant)) {
                    std::cout << variant;
                } else {
                    std::cout << "Non-printable";
                }
                std::cout << "\n";
                std::cout << "Variant (Hex): " << std::hex << static_cast<int>(variant) << "\n";
                std::cout << "Hash: " << hashed_value << "\n";
            }
        }
    }

    file.close();
}

int main(int argc, char* argv[]) {
    if (argc != 3 && !(argc == 4 && std::string(argv[3]) == "-d")) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <target_md5_hash> [-d]\n";
        return 1;
    }

    const char* file_path = argv[1];
    const char* target_hash = argv[2];

    // Set debug mode flag if "-d" is provided
    if (argc == 4 && std::string(argv[3]) == "-d") {
        debug_mode = true;
    }

    // Process variants and find collisions
    process_variants(file_path, target_hash);

    std::cout << "No collision found.\n";
    return 0;
}

