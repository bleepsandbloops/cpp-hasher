#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <openssl/md5.h>
#include <cctype>
#include <vector>
#include <bitset>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>

bool debug_mode = false;

std::mutex collision_mutex; // Mutex to synchronize access to the console during collision reporting
std::mutex cout_mutex;

class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this]() { worker_thread(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }

        condition.notify_all();

        for (std::thread &worker : workers) {
            worker.join();
        }

        // Wait until all tasks are finished
        while (!tasks.empty()) {
        std::this_thread::yield();
        }
    }  

    //void enqueue(const std::function<void()>& task) {
    //    {
    //        std::unique_lock<std::mutex> lock(queue_mutex);
    //        tasks.push(task);
    //    }
    //
    //    condition.notify_one();
    //}
    
    // Updated enqueue function to accept generic callable objects
    template <typename Func>
    void enqueue(Func&& task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(std::forward<Func>(task));
        }

        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    
    //void worker_thread();
   

    void worker_thread() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this]() { return stop || !tasks.empty(); });

                if (stop && tasks.empty()) {
                    return;
                }

                task = std::move(tasks.front());
                tasks.pop();
		//if (debug_mode) { std::cout << "Worker thread popped a task. \n"; }
            }

            task();
        }
    }
};
    //std::vector<std::thread> workers;
    //std::queue<std::function<void()>> tasks;
    //std::mutex queue_mutex;
    //std::condition_variable condition;
    //bool stop;


void print_progress(size_t current, size_t total) {
    std::lock_guard<std::mutex> lock(cout_mutex);  // Lock cout for the duration of this function
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

std::string calculate_md5(const char *data, size_t size) {
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

void print_debug_info(const char *data, size_t size, size_t byte_position, size_t changed_bit_position, char variant) {
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

void process_variant(const char *buffer, size_t file_size, size_t byte_position, size_t changed_bit_position, const char *target_hash, size_t total_variants) {
    // Create a temporary buffer for the variant
    char *variant_buffer = new char[file_size];
    // Copy the original data to the variant buffer
    memcpy(variant_buffer, buffer, file_size);
    if (debug_mode) {std::cout << "Copied original data to the variant buffer\n";}

    // Flip the specified bit in the variant buffer
    variant_buffer[byte_position] ^= (1 << (7 - changed_bit_position));
    if (debug_mode) {std::cout << "Flip the specified bit (" << byte_position << ", " << changed_bit_position << ") in the variant buffer\n";}

    // Calculate the MD5 hash for the variant
    std::string hashed_value = calculate_md5(variant_buffer, file_size);
    if (debug_mode) {std::cout << "Calculated md5: " << hashed_value << "\n";}

    // Check for a collision
    if (hashed_value == target_hash) {
        {
            std::unique_lock<std::mutex> lock(collision_mutex);
            std::cout << "\nCollision found!\n";
            std::cout << "Hash: " << hashed_value << "\n";
            print_debug_info(buffer, file_size, byte_position, changed_bit_position, variant_buffer[byte_position]);
        }

        // Exit the entire program when a collision is found
        std::exit(0);
    }
    
    // If the loop completes without a collision, print a message
    if (debug_mode) {
	    std::cout << "No collision found for variant: " << byte_position << ", " << changed_bit_position << "\n";
    }

    // Update progress bar less frequently (every 1%)
    if (total_variants >= 100 && (byte_position * 8 + changed_bit_position + 1) % (total_variants / 100) == 0) {
        // Update progress bar
        print_progress(byte_position * 8 + changed_bit_position + 1, total_variants);
    } else if (total_variants < 100 && (byte_position * 8 + changed_bit_position + 1) % 1 == 0) {
        // Update progress bar for each variation when total_variants is less than 100
        print_progress(byte_position * 8 + changed_bit_position + 1, total_variants);
}
    
    // Delete the variant buffer
    delete[] variant_buffer;
}

void parallel_process_variants(const char *file_path, const char *target_hash, size_t num_threads) {
    std::ifstream input_file(file_path, std::ios::binary);

    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << file_path << "\n";
        std::exit(1);
    }

    size_t file_size = static_cast<size_t>(input_file.seekg(0, std::ios::end).tellg());
    input_file.seekg(0, std::ios::beg);

    char *buffer = new char[file_size];
    input_file.read(buffer, file_size);

    size_t total_variants = file_size * 8;

    ThreadPool thread_pool(num_threads);

    // Enqueue a task for each variant using the functor
    for (size_t i = 0; i < file_size; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            // Update the functor's parameters for each variant
            thread_pool.enqueue([buffer, file_size, i, j, target_hash, total_variants]() {
                process_variant(buffer, file_size, i, j, target_hash, total_variants);
            });
        }
    }

    // Wait for all tasks to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow some time for the progress bar to update
    std::cout << "\nWaiting for tasks to finish...\n";
}

int main(int argc, char *argv[]) {
    if (argc < 3 || (argc == 4 && std::string(argv[3]) != "-d")) {
        std::cerr << "Usage: " << argv[0] << " <file_path> <target_md5_hash> [-d]\n";
        return 1;
    }

    const char *file_path = argv[1];
    const char *target_hash = argv[2];

    // Set debug mode flag if "-d" is provided
    if (argc == 4 && std::string(argv[3]) == "-d") {
        debug_mode = true;
    }

    // Set the number of threads based on hardware concurrency
    size_t num_threads = std::thread::hardware_concurrency();
    std::cout << "Using " << num_threads << " threads.\n";

    parallel_process_variants(file_path, target_hash, num_threads);

    return 0;
}

