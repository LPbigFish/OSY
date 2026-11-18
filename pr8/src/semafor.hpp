#pragma once
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <new>
#include <semaphore.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace sem {

constexpr size_t N = 5;
constexpr size_t MAX_JIDLO = 256;

struct SharedBuffer {
    // NOLINTNEXTLINE
    char data[N][MAX_JIDLO + 1]{};
    size_t head = 0;
    size_t tail = 0;
    size_t size = 0;
};

struct SharedState {
    SharedBuffer* buffer = nullptr;
    sem_t* empty = SEM_FAILED;
    sem_t* mutex = SEM_FAILED;
    sem_t* full = SEM_FAILED;
    std::string name;
};

inline void release(SharedState* state) {
    if (state->empty != SEM_FAILED) {
        sem_close(state->empty);
        state->empty = SEM_FAILED;
    }
    if (state->mutex != SEM_FAILED) {
        sem_close(state->mutex);
        state->mutex = SEM_FAILED;
    }
    if (state->full != SEM_FAILED) {
        sem_close(state->full);
        state->full = SEM_FAILED;
    }
    if (state->buffer != nullptr) {
        munmap(state->buffer, sizeof(SharedBuffer));
        state->buffer = nullptr;
    }
}

inline auto unlink_names(const SharedState* state) -> bool {
    bool success = true;
    for (const char* suffix : {"_empty", "_mutex", "_full"}) {
        if (sem_unlink((state->name + suffix).c_str()) < 0 && errno != ENOENT) {
            std::perror("sem_unlink");
            success = false;
        }
    }
    if (shm_unlink((state->name + "_buffer").c_str()) < 0 && errno != ENOENT) {
        std::perror("shm_unlink");
        success = false;
    }
    return success;
}

inline auto init(SharedState* state, int port) -> bool {
    state->name = "/osy_restaurant_" + std::to_string(getuid()) + "_"
                + std::to_string(port);

    if (!unlink_names(state)) {
        return false;
    }

    std::string memory_name = state->name + "_buffer";
    int fd = shm_open(memory_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        std::perror("shm_open");
        return false;
    }
    if (ftruncate(fd, sizeof(SharedBuffer)) < 0) {
        std::perror("ftruncate");
        close(fd);
        shm_unlink(memory_name.c_str());
        return false;
    }

    void* mapping = mmap(
        nullptr, sizeof(SharedBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0
    );
    close(fd);
    if (mapping == MAP_FAILED) {
        std::perror("mmap");
        shm_unlink(memory_name.c_str());
        return false;
    }
    // NOLINTNEXTLINE
    state->buffer = new (mapping) SharedBuffer{};

    state->empty
        = sem_open((state->name + "_empty").c_str(), O_CREAT | O_EXCL, 0600, N);
    state->mutex
        = sem_open((state->name + "_mutex").c_str(), O_CREAT | O_EXCL, 0600, 1);
    state->full
        = sem_open((state->name + "_full").c_str(), O_CREAT | O_EXCL, 0600, 0);
    if (state->empty == SEM_FAILED || state->mutex == SEM_FAILED
        || state->full == SEM_FAILED) {
        std::perror("sem_open");
        release(state);
        unlink_names(state);
        return false;
    }
    return true;
}

inline void down(sem_t* semaphore) {
    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {
            continue;
        }
        std::perror("sem_wait");
        std::exit(EXIT_FAILURE);
    }
}

inline void up(sem_t* semaphore) {
    if (sem_post(semaphore) == -1) {
        std::perror("sem_post");
        std::exit(EXIT_FAILURE);
    }
}
} // namespace sem
