#include <limits.h> // Include for INT_MAX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

// Define a structure for messages
typedef struct {
    int transaction_id;
    int customer_id;
    int status; // 0: running, 1: completed, -1: failed
} Message;

// Message queue
mqd_t message_queue;

// Define a structure for message queues
typedef struct {
    mqd_t mq;
    char name[50];
} MessageQueue;

// Define a simple structure to represent an account
typedef struct {
    int customer_id;
    int balance;
} Account;

// Define a simple structure to represent a transaction process
typedef struct {
    int transaction_id;
    int customer_id;
    int status; // 0: running, 1: completed, -1: failed
    pthread_t thread_id; // Thread ID
    int (*transaction_function)(int, int); // Function pointer for transaction
    int amount; // Amount for transaction
    int time_quantum; // Time quantum for Round Robin
    int remaining_time; // Remaining time for transaction
    int start_time; // Start time of transaction
    int end_time; // End time of transaction
} Transaction;

// Define a structure to represent a memory page
typedef struct {
    void* data;
    int size;
    int is_used;
    int last_access_time;
} MemoryPage;

// Define a structure to represent the memory map
typedef struct {
    MemoryPage* pages;
    int page_count;
    int current_time;
} MemoryMap;

// Define a simple array to store accounts
#define MAX_ACCOUNTS 100
Account accounts[MAX_ACCOUNTS];
int account_count = 0;

// Define a simple array to store transactions
#define MAX_TRANSACTIONS 100
Transaction transactions[MAX_TRANSACTIONS];
int transaction_count = 0;

// Mutex for synchronization
pthread_mutex_t account_mutex = PTHREAD_MUTEX_INITIALIZER;

// Memory map
MemoryMap memory_map;

// Message queues
MessageQueue sync_queue;
MessageQueue async_queue;

// Function to initialize the message queues
void initialize_message_queues() {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Message);
    attr.mq_curmsgs = 0;

    sync_queue.mq = mq_open("/sync_queue", O_CREAT | O_RDWR, 0644, &attr);
    async_queue.mq = mq_open("/async_queue", O_CREAT | O_RDWR, 0644, &attr);

    if (sync_queue.mq == (mqd_t)-1 || async_queue.mq == (mqd_t)-1) {
        perror("Failed to create message queue");
        exit(1);
    }

    strcpy(sync_queue.name, "/sync_queue");
    strcpy(async_queue.name, "/async_queue");
}

// Function to send a message
void send_message(MessageQueue* queue, Message* message) {
    if (mq_send(queue->mq, (char*)message, sizeof(Message), 0) == -1) {
        perror("Failed to send message");
        exit(1);
    }
}

// Function to receive a message
void receive_message(MessageQueue* queue, Message* message) {
    if (mq_receive(queue->mq, (char*)message, sizeof(Message), NULL) == -1) {
        perror("Failed to receive message");
        exit(1);
    }
}

// Function to initialize the memory map
void initialize_memory_map(int page_count, int page_size) {
    memory_map.pages = (MemoryPage*)malloc(page_count * sizeof(MemoryPage));
    memory_map.page_count = page_count;
    memory_map.current_time = 0;

    for (int i = 0; i < page_count; i++) {
        memory_map.pages[i].data = malloc(page_size);
        memory_map.pages[i].size = page_size;
        memory_map.pages[i].is_used = 0;
        memory_map.pages[i].last_access_time = 0;
    }
}

// Function to allocate a memory page
MemoryPage* allocate_page() {
    int least_recently_used = -1;
    int min_last_access_time = INT_MAX;

    for (int i = 0; i < memory_map.page_count; i++) {
        if (!memory_map.pages[i].is_used) {
            memory_map.pages[i].is_used = 1;
            memory_map.pages[i].last_access_time = memory_map.current_time;
            return &memory_map.pages[i];
        }

        if (memory_map.pages[i].last_access_time < min_last_access_time) {
            least_recently_used = i;
            min_last_access_time = memory_map.pages[i].last_access_time;
        }
    }

    if (least_recently_used != -1) {
        memory_map.pages[least_recently_used].is_used = 1;
        memory_map.pages[least_recently_used].last_access_time = memory_map.current_time;
        return &memory_map.pages[least_recently_used];
    }

    return NULL; // No available pages
}

// Function to deallocate a memory page
void deallocate_page(MemoryPage* page) {
    if (page) {
        page->is_used = 0;
        page->last_access_time = 0;
    }
}

// Function to update the last access time of a memory page
void update_last_access_time(MemoryPage* page) {
    if (page) {
        page->last_access_time = memory_map.current_time;
    }
}

// Function to create a new account
int create_account(int customer_id, int initial_balance) {
    if (account_count >= MAX_ACCOUNTS) {
        return -1; // Error: Maximum number of accounts reached
    }
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].customer_id == customer_id) {
            return -2; // Error: Account already exists
        }
    }

    MemoryPage* page = allocate_page();
    if (!page) {
        return -3; // Error: No available memory pages
    }

    Account* new_account = (Account*)page->data;
    new_account->customer_id = customer_id;
    new_account->balance = initial_balance;
    accounts[account_count] = *new_account;
    account_count++;
    update_last_access_time(page);
    return 0; // Success
}

// Function to deposit money into an account
int deposit(int account_id, int amount) {
    if (amount < 0) {
        printf("Error: Invalid amount. Amount must be non-negative.\n");
        return -4; // Error: Invalid amount
    }

    pthread_mutex_lock(&account_mutex);
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].customer_id == account_id) {
            accounts[i].balance += amount;
            pthread_mutex_unlock(&account_mutex);
            return 0; // Success
        }
    }
    pthread_mutex_unlock(&account_mutex);
    return -1; // Error: Account not found
}

// Function to withdraw money from an account
int withdraw(int account_id, int amount) {
    if (amount < 0) {
        printf("Error: Invalid amount. Amount must be non-negative.\n");
        return -4; // Error: Invalid amount
    }

    pthread_mutex_lock(&account_mutex);
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].customer_id == account_id) {
            if (accounts[i].balance >= amount) {
                accounts[i].balance -= amount;
                pthread_mutex_unlock(&account_mutex);
                return 0; // Success
            } else {
                pthread_mutex_unlock(&account_mutex);
                return -2; // Error: Insufficient balance
            }
        }
    }
    pthread_mutex_unlock(&account_mutex);
    return -1; // Error: Account not found
}

// Function to check the balance of an account
int check_balance(int account_id) {
    if (account_id < 0) {
        printf("Error: Invalid account ID. Account ID must be non-negative.\n");
        return -4; // Error: Invalid account ID
    }

    pthread_mutex_lock(&account_mutex);
    for (int i = 0; i < account_count; i++) {
        if (accounts[i].customer_id == account_id) {
            int balance = accounts[i].balance;
            pthread_mutex_unlock(&account_mutex);
            return balance; // Return balance
        }
    }
    pthread_mutex_unlock(&account_mutex);
    return -1; // Error: Account not found
}

// Thread function to handle transactions
void* handle_transaction(void* arg) {
    Transaction* transaction = (Transaction*)arg;
    int result = transaction->transaction_function(transaction->customer_id, transaction->amount);

    // Check if the customer ID is valid
    if (transaction->customer_id < 0 || transaction->customer_id >= MAX_ACCOUNTS) {
        printf("Error: Invalid customer ID %d\n", transaction->customer_id);
        transaction->status = -1; // Set status to failed
        pthread_exit(NULL);
    }

    // Check if the account has sufficient funds for withdrawal
    if (transaction->transaction_function == withdraw && transaction->amount > 0 && check_balance(transaction->customer_id) < transaction->amount) {
        printf("Error: Insufficient funds for customer %d\n", transaction->customer_id);
        transaction->status = -1; // Set status to failed
        pthread_exit(NULL);
    }

    if (result == 0) {
        transaction->status = 1; // Set status to completed
    } else {
        transaction->status = -1; // Set status to failed
    }

    // Send a message to the main process
    Message msg = {transaction->transaction_id, transaction->status};
    mq_send(message_queue, (const char *)&msg, sizeof(msg), 0);

    pthread_exit(NULL);
}

// Function to create a transaction process
int create_transaction(int customer_id, int (*transaction_function)(int, int), int amount) {
    if (transaction_count >= MAX_TRANSACTIONS) {
        return -1; // Error: Maximum number of transactions reached
    }

    transactions[transaction_count].transaction_id = transaction_count + 1;
    transactions[transaction_count].customer_id = customer_id;
    transactions[transaction_count].status = 0; // Set status to running
    transactions[transaction_count].transaction_function = transaction_function;
    transactions[transaction_count].amount = amount;
    transactions[transaction_count].time_quantum = 1; // Time quantum for Round Robin
    transactions[transaction_count].remaining_time = 1; // Initial remaining time
    transactions[transaction_count].start_time = 0;
    transactions[transaction_count].end_time = 0;

    pthread_create(&transactions[transaction_count].thread_id, NULL, handle_transaction, &transactions[transaction_count]);
    transaction_count++;
    return 0; // Success
}

// Function to simulate Round Robin scheduling
void round_robin_scheduler() {
    int time_quantum = 1; // Time quantum for Round Robin
    int current_time = 0;
    int completed_transactions = 0;

    while (completed_transactions < transaction_count) {
        for (int i = 0; i < transaction_count; i++) {
            if (transactions[i].status == 0 && transactions[i].remaining_time > 0) {
                if (transactions[i].remaining_time <= time_quantum) {
                    // Transaction completes within the time quantum
                    pthread_join(transactions[i].thread_id, NULL);
                    transactions[i].remaining_time = 0;
                    transactions[i].status = 1; // Set status to completed
                    completed_transactions++;
                    printf("Transaction %d completed at time %d\n", transactions[i].transaction_id, current_time);
                } else {
                    // Transaction does not complete within the time quantum
                    transactions[i].remaining_time -= time_quantum;
                    printf("Transaction %d running at time %d\n", transactions[i].transaction_id, current_time);
                }
                current_time += time_quantum;
            }
        }
    }
}

// Function to calculate average waiting time and CPU utilization
void calculate_metrics() {
    int total_waiting_time = 0;
    int total_cpu_time = 0;
    int current_time = 0; // Declare current_time

    for (int i = 0; i < transaction_count; i++) {
        if (transactions[i].status == 1) {
            int waiting_time = transactions[i].start_time - transactions[i].end_time;
            total_waiting_time += waiting_time;
            total_cpu_time += transactions[i].end_time - transactions[i].start_time;
        }
    }

    double average_waiting_time = (double)total_waiting_time / transaction_count;
    double cpu_utilization = (double)total_cpu_time / current_time;

    printf("Average Waiting Time: %.2f seconds\n", average_waiting_time);
    printf("CPU Utilization: %.2f%%\n", cpu_utilization * 100);
}

// Function to display memory map
void display_memory_map() {
    printf("Memory Map:\n");
    for (int i = 0; i < memory_map.page_count; i++) {
        if (memory_map.pages[i].is_used) {
            printf("Page %d: Used, Last Access Time: %d\n", i, memory_map.pages[i].last_access_time);
        } else {
            printf("Page %d: Free\n", i);
        }
    }
}

// Function to display Gantt chart
void display_gantt_chart() {
    printf("Gantt Chart:\n");
    for (int i = 0; i < transaction_count; i++) {
        if (transactions[i].status == 1) {
            printf("T%d: [", transactions[i].transaction_id);
            for (int j = transactions[i].start_time; j < transactions[i].end_time; j++) {
                printf("-");
            }
            printf("]\n");
        }
    }
}

// Function to terminate a transaction process
int terminate_transaction(int transaction_id) {
    for (int i = 0; i < transaction_count; i++) {
        if (transactions[i].transaction_id == transaction_id) {
            if (transactions[i].status == 0) { // Running
                pthread_cancel(transactions[i].thread_id);
                pthread_join(transactions[i].thread_id, NULL);
                transactions[i].status = -1; // Set status to failed
            }
            return 0; // Success
        }
    }
    return -1; // Error: Transaction not found
}

int main() {
    int choice, customer_id, account_id, amount, balance;
    int result;

    // Initialize the memory map
    initialize_memory_map(10, sizeof(Account));

    while (1) {
        printf("\nBanking System Menu:\n");
        printf("1. Create Account\n");
        printf("2. Deposit\n");
        printf("3. Withdraw\n");
        printf("4. Check Balance\n");
        printf("5. Display Memory Map\n");
        printf("6. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("Enter customer ID: ");
                scanf("%d", &customer_id);
                printf("Enter initial balance: ");
                scanf("%d", &amount);
                result = create_account(customer_id, amount);
                if (result == 0) {
                    printf("Account created successfully.\n");
                } else {
                    printf("Failed to create account.\n");
                }
                break;
            case 2:
                printf("Enter account ID: ");
                scanf("%d", &account_id);
                printf("Enter amount to deposit: ");
                scanf("%d", &amount);
                result = create_transaction(account_id, deposit, amount);
                if (result == 0) {
                    printf("Deposit process started.\n");
                } else {
                    printf("Failed to start deposit process.\n");
                }
                break;
            case 3:
                printf("Enter account ID: ");
                scanf("%d", &account_id);
                printf("Enter amount to withdraw: ");
                scanf("%d", &amount);
                result = create_transaction(account_id, withdraw, amount);
                if (result == 0) {
                    printf("Withdrawal process started.\n");
                } else {
                    printf("Failed to start withdrawal process.\n");
                }
                break;
            case 4:
                printf("Enter account ID: ");
                scanf("%d", &account_id);
                balance = check_balance(account_id);
                if (balance >= 0) {
                    printf("Account balance: %d\n", balance);
                } else {
                    printf("Failed to check balance.\n");
                }
                break;
            case 5:
                display_memory_map();
                break;
            case 6:
                printf("Exiting the banking system.\n");
                return 0;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
