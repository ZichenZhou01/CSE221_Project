#include <stdio.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <errno.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#define BLOCK_SIZE 4096

void flush_cache() {
    system("echo 3 > /proc/sys/vm/drop_caches");
}

static inline uint64_t start_timestamp() {
    unsigned int cycles_low, cycles_high;
    asm volatile ("cpuid\n\t"
                  "rdtsc\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  : "=r" (cycles_high), "=r" (cycles_low)
                  :: "%rax", "%rbx", "%rcx", "%rdx");

    uint64_t result = ((uint64_t) cycles_high << 32) | cycles_low;
    return result;
}

static inline uint64_t end_timestamp() {
    unsigned int cycles_low, cycles_high;
    asm volatile ("rdtscp\n\t"  
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  "cpuid\n\t"
                  : "=r" (cycles_high), "=r" (cycles_low)
                  :: "%rax", "%rbx", "%rcx", "%rdx");
    uint64_t result = ((uint64_t) cycles_high << 32) | cycles_low;
    return result;
}

int compareMeasurements (const void * a, const void * b) {
       if (*(const double*)a < *(const double*)b) return -1;
       else if (*(const double*)a > *(const double*)b) return 1;
       else return 0;
}

void create_file(const char filename, size_t size) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        exit(EXIT_FAILURE);
    }

    char* buffer = (char*) malloc(BLOCK_SIZE);
    for (size_t i = 0; i < size / BLOCK_SIZE; i++) {
        write(fd, buffer, BLOCK_SIZE);
    }
    free(buffer);
    close(fd);
}


int main(int argc, char **argv)
{
    int fd, readData;
    int count = 0;
    double totalTime = 0.0;
    int fileSize, blockNum, fileSize, totalRead = 0;
    uint64_t start, end;
    char* testFile;
    char* data;
    double timing_measurements[100];
    const char* fileName = "testFile";

    if (argc != 2) {
        exit(0);
    }

    testFile = argv[2];
    blockNum = atoi(argv[1]) / BLOCK_SIZE;
    fileSize = atoi(argv[1]);
    int iterations = 100;
    create_file(fileName, fileSize);
    fd = open(fileName, O_RDONLY | 040000);

    flush_cache();
    posix_memalign((void**)&data, BLOCK_SIZE, BLOCK_SIZE);

    for (int i = 0; i < iterations; i++) {
        totalRead = 0;
        start = 0;
        end = 0;

        while (totalRead < fileSize) {
            start = start_timestamp();
            readData = read(fd, data, BLOCK_SIZE);
            end = end_timestamp();

            totalTime += end - start;
            totalRead += readData;
            count++;

            if (totalRead >= fileSize) {
                readData = lseek(fd, 0, SEEK_SET);
                break;
            }
        }
        timing_measurements[i] = totalTime / count;
    }

    qsort(timing_measurements, iterations, sizeof(double), compareMeasurements);

    int trim = iterations / 10; // 10 = 10%
    int trimmed_size = iterations - trim * 2;

    double sum = 0.0;
    for (int i = trim; i < iterations - trim; i++) {
        sum += timing_measurements[i];
    }

    double trimmed_mean = sum / trimmed_size;

    double var = 0.0;
    for (int i = trim; i < iterations - trim; i++) {
        double diff = timing_measurements[i] - trimmed_mean;
        var += diff * diff;
    }

    var = var / trimmed_size;
    double stdev = sqrt(var);

    double avg_block_time = trimmed_mean; 
    printf("[*] Results: %f access time per block\n",
           avg_block_time);

    free(data);
    close(fd);

    return EXIT_SUCCESS;
}
