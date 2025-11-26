# Multi-Threading Inventory Order Processing

Processes input of daily orders for a Landscaping company

### Repo includes precompiled executable (a.out)
Downloaders may flag the executable, don't download it if you don't want to
compiled with: g++




Inventory.old, and orders1-5 files are the input of the test cases. 
log 1-8 and inventory.new1-8 files are showing the expected output for the following test cases:

1: Processes a single orders file with default value
./a.out

2: Processes command line arguments properly
./a.out -p 1 -b 10

3: Processes command line arguments properly 
./a.out -p 1 -b 5

4: Processes command line arguments properly
./a.out -b 5

5: Processes multiple orders
./a.out -p 2 -b 10

6: Processes multiple orders
./a.out -p 5 -b 10

7: Processes multiple orders (small buffer size)
./a.out -p 2 -b 5

8: Processes multiple orders (small buffer size)
./a.out -p 5 -b 5
