/**
 * @author: Brennan Eagle
 * 
 * Tree Company Inventory
 * 
 * read old inventory file
 * calculate sales
 * write new inventory file
*/

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <sstream>


using namespace std;

struct Product{
    unsigned int ID;
    double price;
    unsigned int quantity;
    string description;
};

struct Order{
    unsigned int customerID;
    unsigned int prodID;
    unsigned int quantity;
    int producerID;
    bool isEndSignal;
};

// Global variables
vector<Product> prodArr;
vector<Order> boundedBuffer;
int bufferSize = 10;
int numProducers = 1;
int in = 0;
int out = 0;
int activeProducers = 0;

//semahpores
pthread_mutex_t bufferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inventoryMutex = PTHREAD_MUTEX_INITIALIZER;
sem_t emp;
sem_t full;

void* producer(void *arg){
    int threadNum = *((int*)arg);
    
    stringstream ss;
    ss << "orders" << threadNum;
    string filename = ss.str();
    
    ifstream infile(filename.c_str());
    string line;
    
    if(!infile){
        cerr << "Error opening " << filename << endl;
        pthread_exit(NULL);
    }
    
    while(getline(infile, line)){
        if(line.empty()) continue;

        size_t pos1 = line.find(' ');
        size_t pos2 = line.find(' ', pos1+1);

        if(pos1 == string::npos || pos2 == string::npos) continue;

        Order ord;
        ord.customerID = stoul(line.substr(0, pos1));
        ord.prodID = stoul(line.substr(pos1 + 1, pos2 - pos1 - 1));
        ord.quantity = stoul(line.substr(pos2+1));
        ord.producerID = threadNum;
        ord.isEndSignal = false;

        sem_wait(&emp);
        pthread_mutex_lock(&bufferMutex);
        
        boundedBuffer[in] = ord;
        in = (in + 1) % bufferSize;
        
        pthread_mutex_unlock(&bufferMutex);
        sem_post(&full);
    }

    infile.close();
    
    Order endSignal;
    endSignal.isEndSignal = true;
    endSignal.producerID = threadNum;
    
    //critical
    sem_wait(&emp);
    pthread_mutex_lock(&bufferMutex);
    
    boundedBuffer[in] = endSignal;
    in = (in + 1) % bufferSize;
    
    pthread_mutex_unlock(&bufferMutex);
    sem_post(&full);
    
    pthread_exit(NULL);
}

void* consumer(void *arg){
    ofstream logfile("log");
    if(!logfile){
        cerr << "Error opening log file" << endl;
        pthread_exit(NULL);
    }

    int producersFinished = 0;
    
    while(producersFinished < numProducers){
        sem_wait(&full);
        pthread_mutex_lock(&bufferMutex);
        
        Order ord = boundedBuffer[out];
        out = (out + 1) % bufferSize;
        
        pthread_mutex_unlock(&bufferMutex);
        sem_post(&emp);
        
        if(ord.isEndSignal){
            producersFinished++;
            continue;
        }
        
        bool productFound = false;
        
        //critical
        pthread_mutex_lock(&inventoryMutex);
        
        for(Product& prod : prodArr){
            if(ord.prodID == prod.ID){
                productFound = true;

                logfile << setw(7) << ord.customerID << " "
                        << setw(6) << ord.prodID << " "
                        << left << setw(30) << prod.description << " "
                        << right << setw(5) << ord.quantity << " ";

                if(ord.quantity <= prod.quantity){
                    double ordPrice = ord.quantity * prod.price;
                    prod.quantity -= ord.quantity;
                    logfile << "$" << fixed << setprecision(2) 
                            << setw(8) << ordPrice << " "
                            << "Filled" << endl;
                }
                else{
                    logfile << " " << setw(9) << " " 
                            << "Rejected (Insufficient quantity)" << endl;
                }
                break;
            }
        }

        if(!productFound){
            logfile << setw(7) << ord.customerID << " "
                    << setw(6) << ord.prodID << " "
                    << left << setw(30) << "Unknown product" << " "
                    << right << setw(5) << ord.quantity << " "
                    << setw(10) << " " 
                    << "Rejected (Unknown product)" << endl;
        }
        
        pthread_mutex_unlock(&inventoryMutex);
    }

    logfile.close();
    pthread_exit(NULL);
}

int main(int argc, char* argv[]){
    //cmd line parsing
    for(int i = 1; i < argc; i++){
        if(string(argv[i]) == "-p" && i + 1 < argc){
            numProducers = atoi(argv[i+1]);
            if(numProducers < 1 || numProducers > 9) numProducers = 1;
            i++;
        }
        else if(string(argv[i]) == "-b" && i + 1 < argc){
            bufferSize = atoi(argv[i+1]);
            if(bufferSize < 1 || bufferSize > 30) bufferSize = 10;
            i++;
        }
    }
    
    boundedBuffer.resize(bufferSize);
    
    sem_init(&emp, 0, bufferSize);
    sem_init(&full, 0, 0);
    
    ifstream infile("inventory.old");
    string line;

    if(!infile){
        cerr << "Error opening inventory.old" << endl;
        return 1;
    }

    while(getline(infile, line)){
        if(line.empty()) continue;

        size_t pos1 = line.find(' ');
        size_t pos2 = line.find(' ', pos1 + 1);
        size_t pos3 = line.find(' ', pos2 + 1);
        
        if(pos1 == string::npos || pos2 == string::npos || pos3 == string::npos) continue;
        
        Product prod;
        prod.ID = stoul(line.substr(0, pos1));
        prod.price = stod(line.substr(pos1 + 1, pos2 - pos1 - 1));
        prod.quantity = stoul(line.substr(pos2 + 1, pos3 - pos2 - 1));
        prod.description = line.substr(pos3 + 1);
        
        prodArr.push_back(prod);
    }
    infile.close();

    pthread_t producers[numProducers];
    int threadIDs[numProducers];
    
    for(int i = 0; i < numProducers; i++){
        threadIDs[i] = i + 1;
        int flag = pthread_create(&producers[i], NULL, producer, &threadIDs[i]);
        if(flag != 0){
            cerr << "Error creating producer thread " << (i+1) << endl;
            return 1;
        }
    }

    pthread_t consume;
    int flag = pthread_create(&consume, NULL, consumer, NULL);
    if(flag != 0){
        cerr << "Error creating consumer thread" << endl;
        return 1;
    }

    for(int i = 0; i < numProducers; i++){
        pthread_join(producers[i], NULL);
    }
    
    pthread_join(consume, NULL);

    ofstream outfile("inventory.new");
    if(!outfile){
        cerr << "Error opening inventory.new" << endl;
        return 1;
    }
    
    for(const Product& prod : prodArr){
        outfile << setw(6) << prod.ID << " "
                << fixed << setprecision(2) << setw(8) << prod.price << " "
                << setw(5) << prod.quantity << " "
                << prod.description << endl;
    }

    outfile.close();
    
    sem_destroy(&emp);
    sem_destroy(&full);
    pthread_mutex_destroy(&bufferMutex);
    pthread_mutex_destroy(&inventoryMutex);

    return 0;
}