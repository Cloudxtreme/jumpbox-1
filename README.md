Prototyping a jumpbox with the design goal of using a very locked down private SSH key and having the public side of the key existing on all users on all hosts. The ability to use the private key is then provided via these scripts using various ACL techniques, currently POSIX groups with manual override via the backend YAML config.
