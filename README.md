# Chat Server

## Design Decisions

### Threading Model

The server spawns a new thread for each client connection using `thread::detach()`. This allows concurrent handling of multiple clients. The multithreading code and reference is taken from '**classroom-code/Threading**'. The choice of threads over processes is due to the following reasons:

- **Lower overhead**: Threads share the same memory space, reducing context-switching overhead.
- **Ease of synchronization**: Using mutexes to manage shared resources (like the clients and groups maps) is simpler than inter-process communication.

### Synchronization Mechanisms

- `clients_mutex` protects access to the clients map.
- `users_mutex` ensures thread-safe access to user authentication data.
- `groups_mutex` synchronizes operations on the groups map.

Using `mutex` ensures that concurrent access to these shared resources does not lead to race conditions.

### Authentication Handling

- Authentication is done by verifying credentials from `users.txt`.
- If authentication fails, the client connection is closed immediately.
- Once authenticated, the client's username is stored in the clients map.

### Group Chat Implementation

- Groups are stored in `unordered_map<string, unordered_set<int>> groups`.
- Each group name maps to a set of client sockets representing members.
- Only members of a group can send messages to it.
- Users must join a group before they can participate.

### Client Disconnection Handling

- When a client disconnects, a broadcast message informs others.
- The client is removed from the clients map and its socket is closed.
- The client is also removed from any groups it was a part of.

## Implementation Details

### Important Functions
#### Every function is refactored to have just one purpose which includes breaking up of large functions into smaller ones.

- `load_users()`: Reads and parses `users.txt` to load valid usernames and passwords.
- `authenticate_client(int client_socket)`: Prompts user for username and password. Verifies credentials and either grants or denies access.
- `broadcast_message(const string& message)`: Sends a message to all connected clients.
- `private_message(int client_socket, const string& username, const string& message)`: Sends a direct message to a specific user. Prevents messaging a non-existent/non-online user.
- `create_group(int client_socket, const string& message)`: Creates a new group and adds the requesting client as a member. Throws error on recreation of an existing group.
- `join_group(int client_socket, const string& message)`: Adds a client to an existing group. Throws error on tyring to join a non-existent group.
- `group_message(int client_socket, const string& group_name, const string& message)`: Sends a message to all members of a group. Throws error on trying to message a non-existent group or trying to message a group you are not a member of.
- `leave_group(int client_socket, const string& message)`: Removes a client from a group they are a member of. Throws error when tyring to leave a non-existent group or tyring to leave a group you are not a member of.
- `handle_client(int client_socket)`: Main loop that listens for client messages and processes commands. Throws error on wrong command formats.

### Code Flow

1. **Server Startup**: Loads user data and listens for incoming connections.
2. **Client Connection**: A new thread is spawned for each connecting client.
3. **Authentication**: The client must enter valid credentials.
4. **Message Handling**: Commands like `/msg`, `/broadcast`, `/create_group`, `/group_msg` are processed.
5. **Client Disconnection**: When a client disconnects, the server cleans up resources.

## Testing Strategy

### Correctness Testing

- Tested user authentication with valid and invalid credentials.
- Verified private, broadcast, and group messaging features.

### Stress Testing

- Simulated multiple concurrent clients who authenticate and then leave to ensure correct synchronization.
- Flooded the server with messages to test stability under high load.

### Edge Cases Tested

- Attempting to send a message to a non-existent user.
- Joining a non-existent group.
- Creating a group that already exists.
- Disconnecting a client in the middle of a message transfer.
- Leaving a non-existent group.
- Leaving a group you are not a member of.
- Attempting to send a group message to a non-existent group.
- Attempting to send a group message you are not a member of.

## Server Restrictions

| Restriction             | Limit                  |
|-------------------------|------------------------|
| Maximum clients         | OS limit on sockets    |
| Maximum client to listen| 100000
| Maximum groups          | No hard limit, but memory-bound |
| Maximum group members   | No hard limit, but performance-bound |
| Maximum message size    | 1024 bytes             |

## Challenges Faced

### Synchronization Issues

- Race conditions when multiple clients send messages simultaneously.
- Solved by using `mutex` to protect shared data.

### Socket Handling Bugs

- Some sockets were not closing properly, causing memory leaks.
- Fixed by ensuring `close(client_socket)` is always called on disconnect.
- Solved the **TIME_WAIT** issue with TCP sockets using `SO_REUSEADDR` to immediately bind again.

### Deadlocks During Group Messaging

- Using nested mutex locks led to potential deadlocks.
- Solved by ensuring locks are acquired in a consistent order and avoiding holding multiple locks at once.

### Comparision fails during Authenticaion 

- Solved by triming the leading and trailing whitespace characters from the username and password in the function `load_users()` before inserting into the map.

### Dealing with edge cases

- Solved by carefully addressing edge cases by meticulously identifying, unwinding, and testing each one to ensure correctness.

### Sources Referred 

- Stack Overflow - https://stackoverflow.com/questions/5592747/bind-error-while-recreating-socket
- GeeksforGeeks - https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
- CS330 Operating System lectures on Multithreading

