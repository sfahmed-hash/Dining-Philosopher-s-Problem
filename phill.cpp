#include <iostream>
#include <sys/types.h>  //pid_t
#include <sys/wait.h>   //wait_pid()
#include <unistd.h> // fork(), usleep() ftruncate()
#include <vector>
#include <semaphore.h> // sem_<>() 
#include <fcntl.h>  // for O_CREAT, O_RDWR
#include <cstdlib>  //rand() exit()
#include <sys/mman.h> // shm_open(), mmap(), shm_unlink()

using namespace std;

const int NUM_PHILOSOPHERS = 5;
const int COLOR_1 = 3, COLOR_2 = 2;

// Shared memory for chopsticks, eaten status, and semaphore
sem_t *chopstick_sem;
sem_t *max_eating;
int *chopsticks;
bool *eaten;

void think(int id) {
    usleep(1000 * (1000 + (rand() % 1000)));
}

void eat(int id) {
    usleep(1000 * (1000 + (rand() % 1000)));
}

void philosopher(int id) {
    while (true) {
        bool all_eaten = true;
        for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
            if (!eaten[i]) {
                all_eaten = false;
                break;
            }
        }
        if (all_eaten) break;

        cout << "Philosopher " << id << " is thinking.\n";
        think(id);
// trying to pick chopsticks: lock chopstick access        
        sem_wait(max_eating);
        sem_wait(chopstick_sem);
// if already eaten, release locks on chopstick
        if (eaten[id]) {
            sem_post(chopstick_sem);
            sem_post(max_eating);
            continue;
        }

        int first_pick = rand() % 2; // Randomly pick a color first
        int second_pick = 1 - first_pick;
        
        bool picked_first = false, picked_second = false;

        // Try to pick first chopstick
        if (chopsticks[first_pick] > 0) {
            chopsticks[first_pick]--;
            picked_first = true;
            cout << "Philosopher " << id << " picked a chopstick of color " << (first_pick + 1) << "\n";
        }
        
        // Try to pick second chopstick
        if (picked_first && chopsticks[second_pick] > 0) {
            chopsticks[second_pick]--;
            picked_second = true;
            cout << "Philosopher " << id << " picked a chopstick of color " << (second_pick + 1) << "\n";
        }

        // If the philosopher has one chopstick or two of the same color, then put them down
        if (!picked_second) {
            if (picked_first) {
                chopsticks[first_pick]++;
                cout << "Philosopher " << id << " put down the chopstick.\n";
            }
            sem_post(chopstick_sem);
            sem_post(max_eating);
            continue;
        }
        
        sem_post(chopstick_sem);
        
        cout << "Philosopher " << id << " is eating.\n";
        eat(id);
        eaten[id] = true;

// release chopsticks
        sem_wait(chopstick_sem);
        chopsticks[0]++;
        chopsticks[1]++;
        sem_post(chopstick_sem);
        
        cout << "Philosopher " << id << " put down both chopsticks.\n";
        sem_post(max_eating);
    }
    exit(0);
}

int main() {
    int fd = shm_open("/eaten_status", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, NUM_PHILOSOPHERS * sizeof(bool));
    eaten = (bool *)mmap(NULL, NUM_PHILOSOPHERS * sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        eaten[i] = false;
    }
    
    int chop_fd = shm_open("/chopstick_count", O_CREAT | O_RDWR, 0666);
    ftruncate(chop_fd, 2 * sizeof(int));
    chopsticks = (int *)mmap(NULL, 2 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, chop_fd, 0);
    chopsticks[0] = COLOR_1;
    chopsticks[1] = COLOR_2;
    
    chopstick_sem = sem_open("/chopstick_sem", O_CREAT, 0666, 1);
    max_eating = sem_open("/max_eating", O_CREAT, 0666, NUM_PHILOSOPHERS - 1);

    vector<pid_t> philosophers;
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            philosopher(i);
        } else {
            philosophers.push_back(pid);
        }
    }

    for (pid_t pid : philosophers) {
        waitpid(pid, NULL, 0);
    }

    sem_close(chopstick_sem);
    sem_unlink("/chopstick_sem");
    sem_close(max_eating);
    sem_unlink("/max_eating");
    munmap(eaten, NUM_PHILOSOPHERS * sizeof(bool));
    shm_unlink("/eaten_status");
    munmap(chopsticks, 2 * sizeof(int));
    shm_unlink("/chopstick_count");
    
    return 0;
}
