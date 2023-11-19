#include <iostream>
#include <fstream>
#include <cstring>
#include <openssl/md5.h>
#include <thread>
#include <vector>
#include <iomanip>

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
std::vector<std::string> generate_variants(const std::string& input_data) {
    std::vector<std::string> variants;
    for (size_t i = 0; i < input_data.size() * 8; ++i) {
        size_t index = i / 8;
        size_t bit_position = i % 8;
        std::string variant = input_data;
        variant[index] ^= (1 << bit_position);
        variants.push_back(variant);

        // Debugging: Print the variant in hex format
        if (debug_mode) {
            std::cout << "Variant " << i << ": ";
            for (char c : variant) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)c;
            }
            std::cout << "\n";
        }
    }
    return variants;
}

// Function to process a range of variants
void process_variants(const std::vector<std::string>& variants, size_t start, size_t end, const std::string& target_hash) {
    for (size_t i = start; i < end; ++i) {
        std::string hashed_value = calculate_md5(variants[i].c_str(), variants[i].size());
        if (hashed_value == target_hash) {
            std::cout << "Collision found! Original hash: " << target_hash << "\n";
            std::cout << "Position of changed byte: " << i / 8 << "\n";
            std::cout << "Hex value changed to: " << std::hex << static_cast<int>(variants[i][i % 8]) << "\n";
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
    const std::string target_hash = argv[2];
    
    // Set debug mode flag if "-d" is provided
    if (argc == 4 && std::string(argv[3]) == "-d") {
        debug_mode = true;
    }

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

    // Generate variants
    auto variants = generate_variants(file_data.data());

    // Number of threads to use
    const size_t num_threads = std::thread::hardware_concurrency();
    
    // Calculate variants per thread
    const size_t variants_per_thread = variants.size() / num_threads;

    // Vector to store thread objects
    std::vector<std::thread> threads;

    // Launch threads
    for (size_t i = 0; i < num_threads; ++i) {
        size_t start = i * variants_per_thread;
        size_t end = (i == num_threads - 1) ? variants.size() : (i + 1) * variants_per_thread;
        threads.emplace_back(process_variants, std::ref(variants), start, end, std::cref(target_hash));
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "No collision found.\n";
    return 0;
}

