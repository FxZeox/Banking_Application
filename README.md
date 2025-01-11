# Banking System with Transaction Scheduling and Memory Management

This is a simple banking system that supports basic operations such as creating accounts, depositing and withdrawing money, and checking balances. The system also incorporates transaction scheduling using the Round Robin algorithm and memory management with page allocation and deallocation.

## Features

- **Account Management**: Create new accounts, deposit money, withdraw money, and check account balances.
- **Transaction Scheduling**: Simulate transaction processing using the Round Robin scheduling algorithm.
- **Memory Management**: Simulate a memory map with page allocation, deallocation, and tracking of memory usage.
- **Message Queues**: Use message queues to handle communication between different parts of the system.
- **Metrics Calculation**: Calculate average waiting time and CPU utilization for transactions.
- **Multithreading**: Use pthreads to handle concurrent transactions.

## Key Components

### 1. Account Management
- Create new accounts with an initial balance.
- Deposit money into an account.
- Withdraw money from an account (with checks for sufficient balance).
- Check the balance of an account.

### 2. Transaction Scheduling
- Transactions are processed using the Round Robin algorithm with a fixed time quantum.
- Each transaction has a status (running, completed, or failed).
- The system handles concurrency and ensures that transactions are executed in a fair and scheduled manner.

### 3. Memory Management
- The system uses a simple memory map to simulate memory management, where each account and transaction are allocated memory pages.
- Pages are allocated and deallocated dynamically based on memory usage.

### 4. Message Queues
- Two message queues are used for communication:
  - **Synchronous Queue**: Used for blocking communication.
  - **Asynchronous Queue**: Used for non-blocking communication.

### 5. Metrics Calculation
- The system calculates:
  - **Average Waiting Time**: Average time a transaction waits before it is completed.
  - **CPU Utilization**: Percentage of time the CPU is actively processing transactions.

### 6. Multithreading
- Each transaction is handled by a separate thread, allowing the system to handle multiple transactions concurrently.

## Usage

1. **Compile the Code**:
   ```bash
   gcc -o banking_system banking_system.c -lpthread -lrt

    Run the System:

./banking_system

Menu Options:

    1: Create Account
    2: Deposit Money
    3: Withdraw Money
    4: Check Balance
    5: Display Memory Map
    6: Exit the System

Example Interaction:

    Banking System Menu:
    1. Create Account
    2. Deposit
    3. Withdraw
    4. Check Balance
    5. Display Memory Map
    6. Exit
    Enter your choice: 1
    Enter customer ID: 1001
    Enter initial balance: 500
    Account created successfully.

Design

The system is designed with the following key structures:

    Account: Represents a customer account with a balance.
    Transaction: Represents a banking transaction, including the transaction type (deposit or withdraw) and status.
    MemoryMap: Represents a simulated memory map for managing memory pages allocated to transactions and accounts.
    MessageQueue: Used for inter-process communication between the main process and transaction handlers.

Dependencies

    pthread: For multithreading support.
    mqueue: For message queue handling.
    unistd: For handling system calls.
    fcntl: For file control operations.
    sys/stat: For defining file permissions.

Limitations

    The system currently uses a fixed memory model, with a predefined number of memory pages and transaction slots.
    Transactions are processed sequentially in the Round Robin scheduling, and there is no advanced transaction prioritization.
