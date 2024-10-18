### Instruction (TCP Client)
#### 1. First compile the client
```c++
g++ -o client client.cpp
```
#### 2. Enter the application process id
./client enable -p pid -h 2 -m 1 -d 200
./client update -h 2 -m 1 -d 200
./client disable -h 2 -m 1 -d 200
