# inspector-driver

This is a driver created to learn more about kernel programming, kernel callbacks and filesystem minifilters.
The driver will intercept - 
- process creation
- image load
- file system operations
- registry operations

This driver will block unwanted operations based on a very simple policy:

- Child process blacklists
- File operations on some files
- registry operations

The driver will kill the unwanted process before the operation has been done. 
Also, information will be shared with the user mode side.

## First project milestone: child process blacklists

Allow setting child process policy from user mode. If one of the rules hits, kill the process.
