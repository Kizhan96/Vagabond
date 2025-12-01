# This file contains documentation for the protocol, explaining how to use and implement it.

## Protocol Overview

The client-server application utilizes a defined message protocol to facilitate communication between clients and the server. This protocol ensures that messages are structured and can be easily interpreted by both parties.

## Message Types

The protocol defines several message types that dictate the nature of the communication. Each message type is represented by an enumeration, `MessageType`, which includes:

- **LOGIN**: Used for user authentication.
- **LOGOUT**: Indicates that a user has logged out.
- **TEXT_MESSAGE**: Represents a text message sent from one user to another.
- **VOICE_MESSAGE**: Used for transmitting voice data.
- **CHAT_HISTORY_REQUEST**: Requests the chat history from the server.
- **CHAT_HISTORY_RESPONSE**: Contains the requested chat history.

## Message Structure

Each message sent over the protocol follows a specific structure:

```cpp
struct Message {
    MessageType type; // Type of the message
    QString sender;   // Username of the sender
    QString recipient; // Username of the recipient (if applicable)
    QString content;  // Content of the message (text or voice data)
    QDateTime timestamp; // Time when the message was sent
};
```

## Implementation Guidelines

1. **Encoding and Decoding**: Implement functions to encode and decode messages based on the `Message` structure. This ensures that messages can be serialized for transmission and deserialized upon receipt.

2. **Error Handling**: Include error handling mechanisms to manage unexpected message types or malformed messages.

3. **Versioning**: Consider implementing versioning for the protocol to accommodate future changes without breaking compatibility.

4. **Documentation**: Keep this documentation updated with any changes to the protocol or message types.

By adhering to this protocol, developers can ensure smooth and efficient communication between the client and server components of the application.