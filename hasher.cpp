#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <openssl/md5.h>
#include <cctype>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>

bool debug_mode = false;
std::mutex collision_mutex;  // Mutex to synchronize access to the console during collision reporting

void print_progress(size_t current, size_t total) {
    const int bar_width = 50;
    float progress = static_cast<float>(current) / total;
    int bar_position = static_cast<int>(bar_width * progress);

    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < bar_position) std::cout << "=";
        else std::cout << " ";
    }
    std::cout << "] " << std::setprecision(2) << std::fixed << progress * 100.0 << "%";
    std::cout.flush();
}

std::string calculate_md5(const char* data, size_t size) {
    // Calculate MD5 hash for the entire buffer
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

void print_debug_info(const char* data, size_t size, size_t byte_position, size_t changed_bit_position, char variant) {
    if (debug_mode) {
        std::string hashed_value = calculate_md5(data, size);

        std::cout << "Original data at byte " << std::setw(5) << byte_position << ": ";
        for (size_t i = 0; i < size; ++i) {
            std::cout << data[i];
        }
        std::cout << "\n";

        std::cout << "Hash: " << hashed_value << "\n";

        std::cout << "Debug Info: "
                  << "Byte position within file changed to: " << byte_position << "\n"
                  << "Bit position within byte changed to: " << changed_bit_position << "\n"
                  << "Hex value changed to: " << std::hex << static_cast<int>(variant) << "\n";

        // Print the full file with the modified bit highlighted
        for (size_t k = 0; k < size; ++k) {
            char original_byte = data[k];

            std::cout << "Byte " << std::setw(5) << k << ": " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(original_byte) << " (";

            for (size_t l = 0; l < 8; ++l) {
                if (l == changed_bit_position) {
                    std::cout << "[" << ((original_byte >> (7 - l)) & 1) << "]";
                } else {
                    std::cout << ((original_byte >> (7 - l)) & 1);
                }
            }

            std::cout << ") ";
        }

        std::cout << "\n";
    }
}

void process_variant(const char* buffer, size_t file_size, size_t byte_position, size_t changed_bit_position, const char* target_hash, size_t total_variants) {
    // Create a temporary buffer for the variant
    char* variant_buffer = new char[file_size];
    std::memcpy(variant_buffer, buffer, file_size);

    // Flip the j-th bit in the variant buffer
    variant_buffer[byte_position] ^= (1 << changed_bit_position);

    // Print debugging information
    print_debug_info(variant_buffer, file_size, byte_position, changed_bit_position, variant_buffer[byte_position]);

    // Calculate MD5 hash for the entire variant buffer
    std::string hashed_value = calculate_md5(variant_buffer, file_size);

    // Check for collision
    if (hashed_value == target_hash) {
        size_t changed_bit_position = changed_bit_position;
        char original_value = buffer[byte_position];

        std::lock_guard<std::mutex> lock(collision_mutex);  // Lock to synchronize access to the console during collision reporting

        std::cout << "Collision found! Original hash: " << target_hash << "\n";
        std::cout << "Byte position within file changed to: " << byte_position << "\n";
        std::cout << "Bit position within byte changed to: " << changed_bit_position << "\n";
        std::cout << "Hex value changed to: " << std::hex << static_cast<int>(variant_buffer[byte_position]) << "\n";
        std::cout << "Original value of changed byte: " << std::hex << static_cast<int>(original_value) << "\n";

        delete[] variant_buffer;
        std::exit(0); // Exit the entire program when a collision is found
    }

    delete[] variant_buffer;

    // Update progress bar less frequently (every 1%)
    if ((byte_position * 8 + changed_bit_position + 1) % (total_variants / 100) == 0) {
        print_progress(byte_position * 8 + changed_bit_position + 1, total_variants);
    }
}

void parallel_process_variants(const char* file_path, const char* target_hash) {
    std::ifstream input_file(file_path, std::ios::binary);

    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << file_path << "\n";
        std::exit(1);
    }

    size_t file_size = static_cast<size_t>(input_file.seekg(0, std::ios::end).tellg());
    input_file.seekg(0, std::ios::beg);

    char* buffer = new char[file_size];
    input_file.read(buffer, file_size);

    size_t total_variants = file_size * 8;

    std::vector<std::thread> threads;

    for (size_t i = 0; i < file_size; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            // Launch a thread for each variant
            threads.emplace_back(process_variant, buffer, file_size, i, j, target_hash, total_variants);
        }
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Delete the original buffer outside of the loops
    delete[] buffer;
    input_file.close();

    std::cout << "\nNo collision found.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3 || (argc == 4 && std::string(argv[3]) != "-d")) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <target_md5_hash> [-d]\n";
        return 1;
    }

    const char* file_path = argv[1];
    const char* target_hash = argv[2];

    // Set debug mode flag if "-d" is provided
    if (argc == 4 && std::string(argv[3]) == "-d") {
        debug_mode = true;
    }

    parallel_process_variants(file_path, target_hash);

    return 0;
}

