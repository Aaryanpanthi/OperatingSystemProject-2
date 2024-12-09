#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>

// Function prototypes
void DearOldDad(int *SharedMem, sem_t *sem);
void PoorStudent(int *SharedMem, sem_t *sem);
void LovableMom(int *SharedMem, sem_t *sem);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <Number of Parents (1 or 2)> <Number of Students>\n", argv[0]);
        exit(1);
    }

    int num_parents = atoi(argv[1]);
    int num_students = atoi(argv[2]);

    if (num_parents < 1 || num_parents > 2 || num_students < 1) {
        printf("Invalid input. Must have 1-2 parents and at least 1 student.\n");
        exit(1);
    }

    srand(time(NULL));
    int ShmID, *ShmPTR;
    pid_t pid;

    // Shared memory for the BankAccount
    ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (ShmID < 0) {
        perror("shmget error");
        exit(1);
    }

    ShmPTR = (int *)shmat(ShmID, NULL, 0);
    if (*ShmPTR == -1) {
        perror("shmat error");
        exit(1);
    }

    *ShmPTR = 0; // Initialize BankAccount

    // Create semaphore
    sem_t *sem = sem_open("banksem", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open error");
        exit(1);
    }

    printf("Initial Bank Account = %d\n", *ShmPTR);

    // Fork parents (Dear Old Dad and optionally Lovable Mom)
    for (int i = 0; i < num_parents; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork error");
            exit(1);
        } else if (pid == 0) {
            if (i == 0)
                DearOldDad(ShmPTR, sem);
            else
                LovableMom(ShmPTR, sem);
            exit(0);
        }
    }

    // Fork children (Poor Students)
    for (int i = 0; i < num_students; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork error");
            exit(1);
        } else if (pid == 0) {
            PoorStudent(ShmPTR, sem);
            exit(0);
        }
    }

    // Wait for all child processes to finish
    while (wait(NULL) > 0);
    printf("All processes complete. Cleaning up...\n");

    // Detach and remove shared memory
    shmdt((void *)ShmPTR);
    shmctl(ShmID, IPC_RMID, NULL);

    // Unlink semaphore
    sem_close(sem);
    sem_unlink("banksem");

    return 0;
}

// Dear Old Dad's process logic
void DearOldDad(int *SharedMem, sem_t *sem) {
    while (1) {
        sleep(rand() % 6); // Sleep 0-5 seconds
        printf("Dear Old Dad: Attempting to Check Balance\n");

        sem_wait(sem); // Enter critical section
        int localBalance = *SharedMem;

        if (localBalance < 100) {
            int amount = rand() % 101; // Random amount to deposit (0-100)
            if (amount % 2 == 0) {
                localBalance += amount;
                printf("Dear Old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                *SharedMem = localBalance;
            } else {
                printf("Dear Old Dad: Doesn't have any money to give\n");
            }
        } else {
            printf("Dear Old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
        }

        sem_post(sem); // Leave critical section
    }
}

// Poor Student's process logic
void PoorStudent(int *SharedMem, sem_t *sem) {
    while (1) {
        sleep(rand() % 6); // Sleep 0-5 seconds
        printf("Poor Student: Attempting to Check Balance\n");

        sem_wait(sem); // Enter critical section
        int localBalance = *SharedMem;

        if (rand() % 2 == 0) { // Withdraw money
            int need = rand() % 51; // Random amount needed (0-50)
            printf("Poor Student needs $%d\n", need);

            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
                *SharedMem = localBalance;
            } else {
                printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
            }
        } else {
            printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }

        sem_post(sem); // Leave critical section
    }
}

// Lovable Mom's process logic
void LovableMom(int *SharedMem, sem_t *sem) {
    while (1) {
        sleep(rand() % 11); // Sleep 0-10 seconds
        printf("Lovable Mom: Attempting to Check Balance\n");

        sem_wait(sem); // Enter critical section
        int localBalance = *SharedMem;

        if (localBalance <= 100) {
            int amount = rand() % 126; // Random amount to deposit (0-125)
            localBalance += amount;
            printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);
            *SharedMem = localBalance;
        }

        sem_post(sem); // Leave critical section
    }
}
