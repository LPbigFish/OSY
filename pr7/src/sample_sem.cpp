#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <new>

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr int N = 5;
constexpr int TOTAL_ITEMS = 20;

struct SharedBuffer {
    int data[N]{};
    int read_index = 0;
    int write_index = 0;
};

void down(sem_t* semaphore) {
    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {
            continue;
        }
        std::perror("sem_wait");
        std::exit(EXIT_FAILURE);
    }
}

void up(sem_t* semaphore) {
    if (sem_post(semaphore) == -1) {
        std::perror("sem_post");
        std::exit(EXIT_FAILURE);
    }
}

void producer(SharedBuffer* buffer, sem_t* empty, sem_t* mutex, sem_t* full) {
    for (int item = 1; item <= TOTAL_ITEMS; ++item) {
        down(empty);
        down(mutex);

        buffer->data[buffer->write_index] = item;
        buffer->write_index = (buffer->write_index + 1) % N;
        std::printf("Produced: %d\n", item);

        up(mutex);
        up(full);
    }
}

void consumer(SharedBuffer* buffer, sem_t* empty, sem_t* mutex, sem_t* full) {
    for (int i = 0; i < TOTAL_ITEMS; ++i) {
        down(full);
        down(mutex);

        int item = buffer->data[buffer->read_index];
        buffer->read_index = (buffer->read_index + 1) % N;
        std::printf("Consumed: %d\n", item);

        up(mutex);
        up(empty);
    }
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    // Shared memory for both processes
    void* memory = mmap(
        nullptr,
        sizeof(SharedBuffer),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (memory == MAP_FAILED) {
        std::perror("mmap");
        return EXIT_FAILURE;
    }

    auto* buffer = new (memory) SharedBuffer{};

    // Unique names prevent collisions between runs.
    char empty_name[64], mutex_name[64], full_name[64];
    long pid = static_cast<long>(getpid());

    std::snprintf(empty_name, sizeof(empty_name), "/osy_empty_%ld", pid);
    std::snprintf(mutex_name, sizeof(mutex_name), "/osy_mutex_%ld", pid);
    std::snprintf(full_name, sizeof(full_name), "/osy_full_%ld", pid);

    // Exactly three POSIX semaphores
    sem_t* empty = sem_open(empty_name, O_CREAT | O_EXCL, 0600, N);
    sem_t* mutex = sem_open(mutex_name, O_CREAT | O_EXCL, 0600, 1);
    sem_t* full = sem_open(full_name, O_CREAT | O_EXCL, 0600, 0);

    if (empty == SEM_FAILED || mutex == SEM_FAILED || full == SEM_FAILED) {
        std::perror("sem_open");

        if (empty != SEM_FAILED) {
            sem_close(empty);
        }
        if (mutex != SEM_FAILED) {
            sem_close(mutex);
        }
        if (full != SEM_FAILED) {
            sem_close(full);
        }

        sem_unlink(empty_name);
        sem_unlink(mutex_name);
        sem_unlink(full_name);
        munmap(memory, sizeof(SharedBuffer));
        return EXIT_FAILURE;
    }

    pid_t child = fork();

    if (child == -1) {
        std::perror("fork");
    } else if (child == 0) {
        // Child process = consumer
        consumer(buffer, empty, mutex, full);

        sem_close(empty);
        sem_close(mutex);
        sem_close(full);
        munmap(memory, sizeof(SharedBuffer));

        _exit(EXIT_SUCCESS);
    } else {
        // Parent process = producer
        producer(buffer, empty, mutex, full);

        while (waitpid(child, nullptr, 0) == -1 && errno == EINTR) {
        }
    }

    // Parent cleans up the resources.
    sem_close(empty);
    sem_close(mutex);
    sem_close(full);

    sem_unlink(empty_name);
    sem_unlink(mutex_name);
    sem_unlink(full_name);

    munmap(memory, sizeof(SharedBuffer));

    return child == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
}
