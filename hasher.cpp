#include <iostream>
#include <fstream>
#include <cstring>
#include <openssl/md5.h>
#include <future>
#include <vector>

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
std::vector<std::string> generate_variants(const std::string& input_data) {
    std::vector<std::string> variants;
    for (size_t i = 0; i < input_data.size() * 8; ++i) {
        size_t index = i / 8;
        size_t bit_position = i % 8;
        std::string variant = input_data;
        variant[index] ^= (1 << bit_position);
        variants.push_back(variant);
    }
    return variants;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <target_hash>\n";
        return 1;
    }

    const char* file_path = argv[1];
    const std::string target_hash = argv[2];

    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << file_path << "\n";
        return 1;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> file_data(file_size);
    if (!file.read(file_data.data(), file_size)) {
        std::cerr << "Error reading file: " << file_path << "\n";
        return 1;
    }

    std::vector<std::future<std::string>> futures;

    auto variants = generate_variants(file_data.data());
    for (const auto& variant : variants) {
        futures.push_back(std::async(std::launch::async, calculate_md5, variant.c_str(), variant.size()));
    }

    for (size_t i = 0; i < variants.size(); ++i) {
        std::string hashed_value = futures[i].get();
        if (hashed_value == target_hash) {
            std::cout << "Collision found! Original hash: " << target_hash << "\n";
            return 0;
        }
    }

    std::cout << "No collision found.\n";
    return 0;
}
