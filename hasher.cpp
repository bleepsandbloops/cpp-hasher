#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <openssl/md5.h>

bool debug_mode = false; // Global flag for debugging

// Function to calculate MD5 hash using OpenSSL's MD5 library
std::string calculate_md5(const char* data, size_t size) {
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
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
void process_variants(std::ifstream& file, const char* target_hash) {
    constexpr size_t BUFFER_SIZE = 4096; // Adjust as needed
    char buffer[BUFFER_SIZE];

    size_t bit_position = 0;

    // Read the entire file into the buffer
    file.read(buffer, BUFFER_SIZE);

    // Ensure debug_mode is properly set when the -d flag is provided
    if (debug_mode) {
        std::cout << "Debugging enabled.\n";
    }

    for (size_t i = 0; i < file.gcount(); ++i) {
        for (size_t j = 0; j < 8; ++j) {
            char variant = buffer[i];
            variant ^= (1 << j);

            // Calculate MD5 hash for the current variant
            std::string hashed_value = calculate_md5(&variant, 1);

            // Check for collision
            if (hashed_value == target_hash) {
                size_t changed_byte_position = bit_position / 8;
                char original_value = buffer[i];

                std::cout << "Collision found! Original hash: " << target_hash << "\n";
                std::cout << "Position of changed byte: " << changed_byte_position << "\n";
                std::cout << "Bit position changed to: " << bit_position % 8 << "\n";
                std::cout << "Hex value changed to: " << std::hex << static_cast<int>(variant) << "\n";
                std::cout << "Original value of changed byte: " << std::hex << static_cast<int>(original_value) << "\n";
                std::exit(0);  // Exit the entire program when a collision is found
            }

            ++bit_position;

            if (debug_mode) {
                std::cout << "Bit Position: " << bit_position << "\n";
                std::cout << "Variant: " << std::hex << static_cast<int>(variant) << "\n";
                std::cout << "Hash: " << hashed_value << "\n";
            }
        }
    }

    // Process the remaining bits in the last partial buffer
    size_t remaining_bits = file.gcount() * 8;
    for (size_t i = 0; i < remaining_bits; ++i) {
        char variant = buffer[i / 8];
        variant ^= (1 << (i % 8));

        // Calculate MD5 hash for the current variant
        std::string hashed_value = calculate_md5(&variant, 1);

        // Check for collision
        if (hashed_value == target_hash) {
            size_t changed_byte_position = bit_position / 8;
            char original_value = buffer[i / 8];

            std::cout << "Collision found! Original hash: " << target_hash << "\n";
            std::cout << "Position of changed byte: " << changed_byte_position << "\n";
            std::cout << "Bit position changed to: " << bit_position % 8 << "\n";
            std::cout << "Hex value changed to: " << std::hex << static_cast<int>(variant) << "\n";
            std::cout << "Original value of changed byte: " << std::hex << static_cast<int>(original_value) << "\n";
            std::exit(0);  // Exit the entire program when a collision is found
        }

        ++bit_position;

        if (debug_mode) {
            std::cout << "Bit Position: " << bit_position << "\n";
            std::cout << "Variant: " << std::hex << static_cast<int>(variant) << "\n";
            //std::cout << "Hash: " << hashed_value << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3 || (argc == 4 && std::string(argv[3]) != "-d")) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <target_hash> [-d]\n";
        return 1;
    }

    const char* file_path = argv[1];
    const char* target_hash = argv[2];

    // Set debug mode flag if "-d" is provided
    if (argc == 4 && std::string(argv[3]) == "-d") {
        debug_mode = true;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << file_path << "\n";
        return 1;
    }

    // Process variants and find collisions
    process_variants(file, target_hash);

    std::cout << "No collision found.\n";
    return 0;
}

