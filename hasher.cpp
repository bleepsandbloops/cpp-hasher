#include <iostream>
#include <fstream>
#include <iomanip>  // Added for std::setw and std::setfill
#include <cstring>
#include <openssl/md5.h>
#include <vector>

bool debug_mode = false; // Global flag for debugging

// Function to calculate MD5 hash
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

// Function to generate variants by flipping one bit at a time
std::vector<std::string> generate_variants(const char* input_data, size_t data_size) {
    std::vector<std::string> variants;
    variants.reserve(data_size * 8);

    for (size_t i = 0; i < data_size * 8; ++i) {
        size_t index = i / 8;
        size_t bit_position = i % 8;

        char variant = input_data[index];
        variant ^= (1 << bit_position);

        // Append the modified byte to the variants
        variants.push_back(std::string(input_data, data_size));
        variants.back()[index] = variant;

        // Debugging: Print the variant in hex format
        if (debug_mode) {
            std::cout << "Variant " << i << ": ";
            for (size_t j = 0; j < data_size; ++j) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)variants.back()[j];
            }
            std::cout << "\n";
        }
    }

    return variants;
}

// Function to process variants and find collisions
void process_variants(const std::vector<std::string>& variants, const char* target_hash, size_t data_size) {
    for (size_t i = 0; i < variants.size(); ++i) {
        std::string hashed_value = calculate_md5(variants[i].c_str(), data_size);
        if (hashed_value == target_hash) {
            size_t changed_byte_position = i / 8;
            char original_value = variants[i][changed_byte_position];

            std::cout << "Collision found! Original hash: " << target_hash << "\n";
            std::cout << "Position of changed byte: " << changed_byte_position << "\n";
            std::cout << "Hex value changed to: " << std::hex << static_cast<int>(variants[i][changed_byte_position]) << "\n";
            std::cout << "Original value of changed byte: " << std::hex << static_cast<int>(original_value) << "\n";
            std::exit(0);  // Exit the entire program when a collision is found
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

    // Read the entire file into memory
    std::vector<char> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Generate variants
    auto variants = generate_variants(file_data.data(), file_data.size());

    // Process variants and find collisions
    process_variants(variants, target_hash, file_data.size());

    std::cout << "No collision found.\n";
    return 0;
}

