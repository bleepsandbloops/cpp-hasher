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
//std::mutex buffer_mutex;
std::mutex printing_mutex; // Mutex to synchronize access to the standard output stream
std::mutex outfile_mutex; // Mutex to ensure exclusive access to the outfile
std::string collision_outfile; // Holds the filename to save the corrected file, if found 

// Declare a buffer
//constexpr int max_num_thread = 32;
//constexpr int max_buf_size = 4096*512; // Generously give a 2 MB buffer for 1.5MB files 
//char thread_buffer[max_num_thread][max_buf_size]; // Allocate a static array for all variant buffers

//Declare the buffer size
const int buffer_size = 3072*512

// Function declarations
class ThreadPool;
std::string calculate_md5(const char *data, size_t size);
void print_debug_info(const char *data, size_t size, size_t byte_position, size_t changed_bit_position, char variant);
void save_buffer(const char *variant_buffer, size_t file_size);
void process_variant(const char *buffer, size_t file_size, size_t byte_position, size_t changed_bit_position, const char *target_hash, size_t total_variants);
void parallel_process_variants(const char *file_path, const char *target_hash, size_t num_threads);
void print_progress(size_t current, size_t total);

//std::condition_variable bufferAvailable;

// Class to define the ThreadPool 
class ThreadPool {
public:

    // Constructor
    ThreadPool(size_t num_threads) : stop(false) {
        // Create worker threads
	for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this]() { worker_thread(); }); 
	}
    }
    
    // Destructor
    ~ThreadPool() {
        {
	    // Lock the queue_mutex 
            std::unique_lock<std::mutex> lock(queue_mutex);
            // Set stop to true to signal worker threads to stop
	    stop = true;
        }
        // Notify all threads waiting on the condition variable
	condition.notify_all();

	// Wait for all worker threads to finish, guarantee thread cleanup
        for (std::thread &worker : workers) {
            worker.join();
        }
    }  
    
    // Structure to hold the buffers
    struct VariantBuffer {
        char data[buffer_size];
    };
   
    // Vector of buffers
    std::vector<VariantBuffer> buffer_pool;

    // Enqueue function to be called by parallel_process_variants function 
    template <typename Func>
    void enqueue(Func&& task) { // heap-allocation option
        {
            std::unique_lock<std::mutex> lock(queue_mutex); // Acquire the lock
	    tasks.emplace(std::forward<Func>(task));  // headp-allocation version //compare tasks.push and tasks.emplace
        }
        // Notify one waiting worker_thread that a new task is available TODO notify_one or notify_all ?
            condition.notify_one();
    }
	    //tasks.emplace([task, &buffer]() {task(buffer); });  // Pass the task and buffer via a lambda function 


private:
    std::vector<std::thread> workers; // Vector of worker threads
    std::mutex queue_mutex;           // Queue_mutex object for the lock
    std::queue<std::function<void()>> tasks; // Heap-allocated queue of callable objects (type-erasing wrapper)
    std::condition_variable condition; // Condition variable used to wake up worker threadswith mutex to synchronize threads
    bool stop;                         // Flag to signal threads to stop processing tasks
    
    // Define a worker thread who runs as an infinite loop, picking tasks from the queue, until stop is signalled
    void worker_thread() {
        while (true) {
	    // Initialize an empty task object for this thread, passing it this worker's buffer
	    
	    std::function<void()> task;

            {
		// Block current thread until condition is notified (thread becomes available or class destructor is called). 
		// Then, reacquire the lock and reawaken the thread if stop is signalled or task list is empty
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this]() { return stop || !tasks.empty(); });

		// Exit if there are no more threads
                if (stop && tasks.empty()) {
                    return;
                }

		// Move the object from the front of the queue to task and pop the queue
                task = std::move(tasks.front());
                tasks.pop();
            }

	    // Start the task in this thread
            task();
	        // Task is 
                // thread_pool.enqueue([file_size, i, j, target_hash, total_variants]() {
                // process_variant(file_size, i, j, target_hash, total_variants);
                // });
        }
    }
};


// Function to calculate MD5 hash for the char array
std::string calculate_md5(VariantBuffer& buffer, size_t size) {
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
void print_debug_info(size_t byte_position, size_t changed_bit_position, char variant) {
    if (debug_mode) {
        std::cout << "Debug Info: "
                  << "Byte position within file changed to: " << byte_position << "\n"
                  << "Bit position within byte changed to: " << changed_bit_position << "\n"
                  << "Hex value changed to: " << std::hex << static_cast<int>(variant) << "\n";
    }
}

//Function to save the corrected variant buffer to file
void save_buffer(const char *buffer, size_t file_size){
    
    // Open an output file stream in binary mode
    std::ofstream outputFile(collision_outfile, std::ios::binary);
	
    // Write the contents of the buffer to the file
    if (outputFile.is_open()) {
        outputFile.write(buffer, file_size);
        outputFile.close();
        std::cout << "Variant saved to file: " << collision_outfile << std::endl;
    } else {
        std::cerr << "Error opening file: " << collision_outfile << std::endl;
    }
}

// Function to process one variant and return collisions 
//void process_variant(const char *buffer, size_t file_size, size_t byte_position, size_t changed_bit_position, const char *target_hash, size_t total_variants) {
void process_variant(VariantBuffer& buffer, size_t byte_position, size_t changed_bit_position, const char *target_hash, size_t total_variants) {
    
    // Create a temporary buffer for the variant
    //char *variant_buffer = new char[file_size];
    
    // Copy the original data to the variant buffer
    //memcpy(variant_buffer, buffer, file_size);

    // Flip the specified bit in the variant buffer
    buffer.data[byte_position] ^= (1 << (7 - changed_bit_position));

    // Calculate the MD5 hash for the variant
    std::string hashed_value = calculate_md5(buffer, file_size);
    
    // Check for a collision
    if (hashed_value == target_hash) {

        // If collision is found, save the modified buffer
        save_buffer(buffer, file_size);
        
	if (debug_mode) {
            std::unique_lock<std::mutex> lock(printing_mutex);
            std::cout << "\nCollision found!  << byte_position << ", " << changed_bit_position << ": " << hashed_value << "\n";
            print_debug_info(byte_position, changed_bit_position, variant_buffer[byte_position]);
        }
        std::exit(0);

    }

    // Return the buffer to the pool
    buffer.data[byte_position] ^= (1 << (7 - changed_bit_position));
    //{
    //    std::lock_guard<std::mutex> lock(bufferMutex);
    //    bufferPool.push_back(*buffer);
    //}    
    //delete[] variant_buffer;

    // If collision is found, exit the entire program
    if (hashed_value == target_hash) {
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
    //delete[] variant_buffer;
}

// Function to create the Thread Pool and start the jobs
void parallel_process_variants(const char *file_path, const char *target_hash, size_t num_threads) {
    
    // Open a binary mode input stream 
    std::ifstream input_file(file_path, std::ios::binary);
    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << file_path << ". Exiting...\n";
        std::exit(1);
    }

    // Determine the file size using seek and tell of the file stream
    size_t file_size = static_cast<size_t>(input_file.seekg(0, std::ios::end).tellg());
    input_file.seekg(0, std::ios::beg);
    if (!file_size == buffer_size) {
        std:cerr << "Wrong buffer size for file size " << file_size << ". Exiting...\n";
    }

    // Read the file stream into the buffer
    //char *buffer = new char[file_size];
    //input_file.read(buffer, file_size);

    // Initialize the ThreadPool and buffer pool
    ThreadPool thread_pool(num_threads);
    thread_pool.buffer_pool.resize(num_threads);
    input_file.read(buffer_pool[0], buffer_size);
    for (size_t i = 1; i < num_threads; i++) {
	memcpy(buffer_pool[i], buffer_pool[0], buffer_size);
    }
    
    // Calculate number of variants, equalling number of bits
    size_t total_variants = buffer_size * 8;

    // Enqueue a task for each variant using the functor
    for (size_t i = 0; i < file_size; ++i) {
        for (size_t j = 0; j < 8; ++j) {
	    // Enqueue the task
            //thread_pool.enqueue(&process_variant_wrapper, buffer, file_size, i, j, target_hash, total_variants);
            // Create lambda function for each task and cast to a pointer
            //auto task = [buffer, file_size, i, j, target_hash, total_variants]() {
	    //	    process_variant(buffer, file_size, i, j, target_hash, total_variants);
	    //};
	    // thread_pool.enqueue(static_cast<void (*)()>(task))
	    thread_pool.enqueue([&buffer, file_size, i, j, target_hash, total_variants]() {
                process_variant(std::ref(buffer), file_size, i, j, target_hash, total_variants);
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

