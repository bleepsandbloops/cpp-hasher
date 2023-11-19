#include <iostream> // Console IO streams
#include <fstream>  // File IO streams
#include <iomanip>  // Progress bar formatting
#include <cstring>  // memcpy
#include <string>   // String manipulation for collision_outfile
#include <sstream>  // Convert MD5 hashes
#include <openssl/md5.h> // MD5 hash
#include <vector>   // Vector to contain threads
#include <thread>   // Threads
#include <queue>    // Task queue for the thread pool
#include <functional> // Create callable object in the thread pool
#include <condition_variable> // Coordinate tasks in the thread pool
#include <chrono>   // Wait for thread execution
#include <mutex>    // Mutual exclusions for console, queue, outfile access

// Global variables
bool debug_mode = false; // Holds the optional mode for printing debug statements
std::mutex printing_mutex; // Mutex to synchronize access to the standard output stream
std::mutex outfile_mutex; // Mutex to ensure exclusive access to the outfile
std::string collision_outfile; // Holds the filename to save the corrected file, if found 

// Function declarations
class ThreadPool;
std::string calculate_md5(const char *data, size_t size);
void print_debug_info(const char *data, size_t size, size_t byte_position, size_t changed_bit_position, char variant);
void save_buffer(const char *variant_buffer, size_t file_size);
void process_variant(const char *buffer, size_t file_size, size_t byte_position, size_t changed_bit_position, const char *target_hash, size_t total_variants);
void parallel_process_variants(const char *file_path, const char *target_hash, size_t num_threads);
void print_progress(size_t current, size_t total);


// Class to define the ThreadPool 
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

    // Updated enqueue function to accept generic callable objects
    template <typename Func>
    void enqueue(Func&& task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<Func>(task));  //compare tasks.puhs and tasks.emplace
        }

        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    
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
            }

            task();
        }
    }
};


// Function to calculate MD5 hash for the char array
std::string calculate_md5(const char *data, size_t size) {
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    MD5_Update(&md5Context, data, size);

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);
  
    // Create the hex string ensiring that each digit is two characters 
    std::stringstream md5Stream;
    md5Stream << std::hex << std::setfill('0');
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        md5Stream << std::setw(2) << static_cast<unsigned int>(result[i]);
    }

    return md5Stream.str();
}

// Function to print debugging statements
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

//Function to save the corrected variant buffer to file
void save_buffer(const char *variant_buffer, size_t file_size){
    
    // Open an output file stream in binary mode
    std::ofstream outputFile(collision_outfile, std::ios::binary);
	
    // Write the contents of the buffer to the file
    if (outputFile.is_open()) {
        outputFile.write(variant_buffer, file_size);
        outputFile.close();
        std::cout << "Variant saved to file: " << collision_outfile << std::endl;
    } else {
        std::cerr << "Error opening file: " << collision_outfile << std::endl;
    }
}

// Function to process one variant and return collisions 
void process_variant(const char *buffer, size_t file_size, size_t byte_position, size_t changed_bit_position, const char *target_hash, size_t total_variants) {
    
    // Create a temporary buffer for the variant
    char *variant_buffer = new char[file_size];
    
    // Copy the original data to the variant buffer
    memcpy(variant_buffer, buffer, file_size);

    // Flip the specified bit in the variant buffer
    variant_buffer[byte_position] ^= (1 << (7 - changed_bit_position));

    // Calculate the MD5 hash for the variant
    std::string hashed_value = calculate_md5(variant_buffer, file_size);
    
    // Check for a collision
    if (hashed_value == target_hash) {
        {
            std::unique_lock<std::mutex> lock(printing_mutex);
            std::cout << "\nCollision found!\n";
            std::cout << "Hash: " << hashed_value << "\n";
            print_debug_info(buffer, file_size, byte_position, changed_bit_position, variant_buffer[byte_position]);
        }

        // If collision is found, save the modified variant buffer and exit the entire program
        save_buffer(variant_buffer, file_size);
	delete[] variant_buffer;
        std::exit(0);
    }
    
    // If the loop completes without a collision, print a message
    if (debug_mode) {
	    std::unique_lock<std::mutex> lock(printing_mutex);
	    std::cout << "No collision found for variant: " << byte_position << ", " << changed_bit_position << ": " << hashed_value << "\n";
    }

    // Update progress bar every 1%
    if ((total_variants > 100 && (byte_position * 8 + changed_bit_position + 1) % (total_variants / 100) == 0) || (total_variants <= 100)) {
        // Update progress bar
        print_progress(byte_position * 8 + changed_bit_position + 1, total_variants);
    } 
    
    // Delete the variant buffer
    delete[] variant_buffer;
}

// Function to create the Thread Pool and start the jobs
void parallel_process_variants(const char *file_path, const char *target_hash, size_t num_threads) {
    
    // Open a binary mode input stream 
    std::ifstream input_file(file_path, std::ios::binary);
    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << file_path << "\n";
        std::exit(1);
    }

    // Determine the file size using seek and tell of the file stream
    size_t file_size = static_cast<size_t>(input_file.seekg(0, std::ios::end).tellg());
    input_file.seekg(0, std::ios::beg);

    // Read the file stream into the buffer
    char *buffer = new char[file_size];
    input_file.read(buffer, file_size);

    // Calculate number of variants, equalling number of bits
    size_t total_variants = file_size * 8;

    // Initialize the ThreadPool
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

// Function to print progress bar
void print_progress(size_t current, size_t total) {
    
    // Lock standard output stream for the duration of this function
    std::lock_guard<std::mutex> lock(printing_mutex);  
    
    // Calculate parameters
    const int bar_width = 50;
    float progress = static_cast<float>(current) / total;
    int bar_position = static_cast<int>(bar_width * progress);

    // Print the progress bar
    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < bar_position) std::cout << "=";
        else std::cout << " ";
    }
    std::cout << "] " << std::setprecision(2) << std::fixed << progress * 100.0 << "%";
    std::cout.flush();
}

// Main function
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
    
    // Set the filename for the collision outfile
    collision_outfile = std::string(file_path) + "_corrected";

    // Set the number of threads based on hardware concurrency
    size_t num_threads = std::thread::hardware_concurrency();
    std::cout << "Using " << num_threads << " threads.\n";

    // Start processing
    parallel_process_variants(file_path, target_hash, num_threads);

    return 0;
}

