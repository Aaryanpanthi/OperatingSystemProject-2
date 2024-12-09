#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#define SHARED_MEM_SIZE sizeof(int)

// Function Prototypes
void ParentProcess(sem_t *sem, int *BankAccount);
void ChildProcess(sem_t *sem, int *BankAccount);
void MomProcess(sem_t *sem, int *BankAccount);

int main(int argc, char *argv[]) {
    int ShmID;
    int *BankAccount;
    sem_t *sem;
    pid_t pid, mom_pid;
    int status;

    // Create a shared memory segment to hold the BankAccount balance
    ShmID = shmget(IPC_PRIVATE, SHARED_MEM_SIZE, IPC_CREAT | 0666);
    if (ShmID < 0) {
        perror("shmget error");
        exit(1);
    }
    printf("Server: Shared memory for BankAccount created.\n");

    // Attach the shared memory segment
    BankAccount = (int *)shmat(ShmID, NULL, 0);
    if (BankAccount == (int *)-1) {
        perror("shmat error");
        exit(1);
    }
    printf("Server: Attached the shared memory segment.\n");

    // Initialize the BankAccount to a starting value, e.g., $100
    *BankAccount = 100;
    printf("Server: BankAccount initialized to $%d.\n", *BankAccount);

    // Create a named semaphore for mutual exclusion on BankAccount
    sem = sem_open("/bank_semaphore", O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open error");
        exit(1);
    }

    printf("Server: Now forking processes (Mom and Student)...\n");
    
    // Fork "Lovable Mom" process
    mom_pid = fork();
    if (mom_pid < 0) {
        perror("fork error for Mom");
        exit(1);
    } else if (mom_pid == 0) {
        // In the child process for Mom
        MomProcess(sem, BankAccount);
        exit(0);
    }

    // Fork "Poor Student" process
    pid = fork();
    if (pid < 0) {
        perror("fork error for Student");
        exit(1);
    } else if (pid == 0) {
        // In the child process for Student
        ChildProcess(sem, BankAccount);
        exit(0);
    }

    // In the original process: Dear Old Dad
    ParentProcess(sem, BankAccount);

    // Parent will only reach here if stopped externally and children exit
    waitpid(mom_pid, &status, 0);
    waitpid(pid, &status, 0);

    // Cleanup resources when done
    sem_close(sem);
    sem_unlink("/bank_semaphore");
    shmdt((void *)BankAccount);
    shmctl(ShmID, IPC_RMID, NULL);
    printf("Server: Cleaned up shared memory and semaphore. Exiting...\n");
    exit(0);
}

// Dear Old Dad Process: Attempts to deposit money if balance < 100, or just checks balance.
// He runs indefinitely, sleeping randomly between operations.
void ParentProcess(sem_t *sem, int *BankAccount) {
    srand(time(0) + getpid());
    while (1) {
        sleep(rand() % 5); // Sleep between 0-4 seconds
        printf("Dear Old Dad: Attempting to check balance.\n");

        sem_wait(sem); // Enter critical section
        int localBalance = *BankAccount;

        // Random action: If even number, consider depositing if balance is low
        if (rand() % 2 == 0) {
            if (localBalance < 100) {
                int amount = rand() % 101; // 0-100
                *BankAccount += amount;
                printf("Dear Old Dad: Deposits $%d / New Balance = $%d\n", amount, *BankAccount);
            } else {
                printf("Dear Old Dad: Thinks student has enough cash ($%d)\n", localBalance);
            }
        } else {
            // Odd number: Just report current balance
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }
        sem_post(sem); // Leave critical section
    }
}

// Lovable Mom Process: If the balance is 100 or less, she always deposits money.
// She sleeps up to 9 seconds between tries.
void MomProcess(sem_t *sem, int *BankAccount) {
    srand(time(0) + getpid());
    while (1) {
        sleep(rand() % 10); // Sleep between 0-9 seconds
        printf("Lovable Mom: Attempting to check balance.\n");

        sem_wait(sem);
        int localBalance = *BankAccount;

        // If balance <= 100, deposit a random amount between 0-125
        if (localBalance <= 100) {
            int amount = rand() % 126;
            *BankAccount += amount;
            printf("Lovable Mom: Deposits $%d / New Balance = $%d\n", amount, *BankAccount);
        } else {
            printf("Lovable Mom: Balance is sufficient ($%d), no deposit this time.\n", localBalance);
        }
        sem_post(sem);
    }
}

// Poor Student Process: Sometimes tries to withdraw money if needed is less than or equal to balance.
// Otherwise, just checks balance. Sleeps up to 4 seconds between attempts.
void ChildProcess(sem_t *sem, int *BankAccount) {
    srand(time(0) + getpid());
    while (1) {
        sleep(rand() % 5); // Sleep between 0-4 seconds
        printf("Poor Student: Attempting to check balance.\n");

        sem_wait(sem);
        int localBalance = *BankAccount;

        // If even random number, try to withdraw
        if (rand() % 2 == 0) {
            int need = rand() % 51; // Need between 0-50
            printf("Poor Student needs $%d\n", need);
            if (need <= localBalance) {
                *BankAccount -= need;
                printf("Poor Student: Withdraws $%d / New Balance = $%d\n", need, *BankAccount);
            } else {
                printf("Poor Student: Not enough cash. Current Balance = $%d\n", localBalance);
            }
        } else {
            // If odd random number, just check the balance
            printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }
        sem_post(sem);
    }
}
