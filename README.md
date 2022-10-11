# Multi-Threaded HTTP Server
## Functionality
- **USAGE**: ./httpserver [-t threads] [-l logfile] \<port\>
- **This program combines Assignments 1, 2, and 3 so it's a multithreaded HTTP server with audit logging except it now implements atomicity and coherency.**

## Design

#### Partial PUT/APPEND
- Created Temporary file and read from connfd into the file.
- This means that reading the input can be done asynchronously.
- If a PUT or APPEND is waiting on input then it allows for GET requests to process.
- Once the input has been read entirely the thread gets the lock and renames the temporary file to the destination file.
- For APPEND, it copies over the input from the temporary file to the destination file.

#### Coherence and Atomicity
- Used flocks to lock files when logging, sending responses
- Flocks prevents writing to the same file at the same time.
- Also used mutexes for specific regions that I wanted only wanted thread in at a time.

## Data Structures
- Used temporary files for PUT and APPEND to store the input until it was all received.
- Used pthread locks, flocks for atomicity and coherency.
- Used arrays of characters for the temporary buffers.
