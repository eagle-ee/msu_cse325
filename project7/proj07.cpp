/**
 * @file: proj07.cpp
 * @author: Brennan Eagle
 * 
 * Simulate 16-bit microprocessor
 * 
 * Parse CMD line args
 * Generate register, RAM, and cache data structures
 * Determine optional files needed to update initial RAM
 * Display info (with optional -debug tag)
 * 
 * 
 */

#include <iostream>
using std::cout; using std::cin; using std::endl; using std::cerr;
#include <iomanip>
#include <sstream>
#include <string>
using std::string;
#include <vector>
using std::vector;
#include <fstream>

#include <sys/stat.h>
#include <typeinfo>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>

class Microprocessor{
public:
    Microprocessor(const string& inputFile, bool debug=false);
    Microprocessor(const string& inputFile, const string& ramFile, 
        bool debug=false);

    void display() const;
    void Start();
private:
    struct CacheLine{
        bool valid = false;
        bool mod = false;
        uint16_t tag = 0;
        uint8_t data[8] = {0};
    };
    CacheLine Cache[8];
    uint16_t r[16] = {0};
    uint8_t RAM[65536] = {0};

    string InputFile;
    bool debug;

    void loadRAM(const std::string& ramFile);
    void readInstr(const std::string& line);
    void displayCache() const;

    void loadCache(int reg, uint16_t add);
    void storeCache(int reg, uint16_t add);
};

/**
 * Constructors
 */
Microprocessor::Microprocessor(const string& inputFile, bool debug) : debug(debug){
    InputFile = inputFile;
}
Microprocessor::Microprocessor(const string& inputFile, const string& ramFile, 
    bool debug) : debug(debug)
{
    InputFile = inputFile;
    loadRAM(ramFile);
}

/**
 * Load RAM from file
 * 
 * file line:
 * 0000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 */
void Microprocessor::loadRAM(const string& ramFile){
    if(!ramFile.empty()){
        std::ifstream file(ramFile);
        string line;
        while(std::getline(file,line)){
            std::istringstream iss(line);
            uint16_t addr;
            iss >> std::hex >> addr;

            for(int i=0; i<16; ++i){
                int byte;
                iss >> std::hex >> byte;
                RAM[addr+i] = static_cast<uint8_t>(byte);
            }

        }
    }
    if(debug){ display(); }
}

/**
* LDR
*/
void Microprocessor::loadCache(int reg, uint16_t addr){
    uint8_t byteOffset = addr & 0x7;
    uint8_t cacheIndex = (addr >> 3) & 0x7;
    uint16_t tag = (addr >> 6) & 0x3FF;
    uint16_t blockAddr = addr & 0xFFF8;
    
    CacheLine& line = Cache[cacheIndex];
    uint16_t value;
    bool hit = (line.valid && line.tag == tag);
    char hitBit = hit ? 'H' : 'M';

    if(hit){
        //hit
        value = (line.data[byteOffset + 1] << 8) | line.data[byteOffset];
        r[reg] = value;
    }
    else{
        //miss
        if(line.valid && line.mod){
            uint16_t oldBlockAddr = (line.tag << 6) | (cacheIndex << 3);
            for(int i = 0; i < 8; ++i){
                RAM[oldBlockAddr + i] = line.data[i];
            }
        }
        
        //block RAM into cache
        for(int i = 0; i < 8; ++i){
            line.data[i] = RAM[blockAddr + i];
        }
        
        line.valid = true;
        line.mod = false;
        line.tag = tag;
        
        //load
        value = (line.data[byteOffset] << 8) | line.data[byteOffset + 1];
        r[reg] = value;
    }

    cout << "LDR " << std::hex << reg << " " 
         << std::setw(4) << std::setfill('0') << addr << " "
         << std::setw(3) << std::setfill('0') << tag << " "
         << static_cast<int>(cacheIndex) << " " 
         << static_cast<int>(byteOffset) << " "
         << hitBit << " "
         << std::setw(4) << std::setfill('0') << value
         << endl;

    if(debug){
        displayCache();
    }
}

/**
* STR
*/
void Microprocessor::storeCache(int reg, uint16_t addr){
    uint8_t byteOffset = addr & 0x7;
    uint8_t cacheIndex = (addr >> 3) & 0x7;
    uint16_t tag = (addr >> 6) & 0x3FF;
    uint16_t blockAddr = addr & 0xFFF8;
    
    CacheLine& line = Cache[cacheIndex];
    uint16_t value;
    bool hit = (line.valid && line.tag == tag);
    char hitBit = hit ? 'H' : 'M';

    if(hit){
        //hit
        line.data[byteOffset] = r[reg] & 0xFF;
        line.data[byteOffset + 1] = (r[reg] >> 8) & 0xFF;
        line.mod = true; 
    }
    else{
        //miss
        if(line.valid && line.mod){
            uint16_t oldBlockAddr = (line.tag << 6) | (cacheIndex << 3);
            for(int i = 0; i < 8; ++i){
                RAM[oldBlockAddr + i] = line.data[i];
            }
        }
        
        //block RAM into cache
        for(int i = 0; i < 8; ++i){
            line.data[i] = RAM[blockAddr + i];
        }
        
        line.valid = true;
        line.tag = tag;
        
        //store
        line.data[byteOffset] = (r[reg] >> 8) & 0xFF;
        line.data[byteOffset + 1] = r[reg] & 0xFF;
        line.mod = true;
    }

    cout << "STR " << std::hex << reg << " " 
         << std::setw(4) << std::setfill('0') << addr << " "
         << std::setw(3) << std::setfill('0') << tag << " "
         << static_cast<int>(cacheIndex) << " " 
         << static_cast<int>(byteOffset) << " "
         << hitBit << " "
         << std::setw(4) << std::setfill('0') << value
         << endl;

    if(debug){
        displayCache();
    }
}

/**
* Read an instruction line
*/
void Microprocessor::readInstr(const string& line){
    std::istringstream iss(line);
    std::string operation;
    int reg;
    uint16_t addr;
    
    if(iss >> operation >> std::hex >> reg >> addr){
        if(operation == "LDR"){ loadCache(reg, addr);}
        else if(operation == "STR"){ storeCache(reg, addr);}
    }
}

/**
* Begin reading instructions
*/
void Microprocessor::Start(){
    std::ifstream file(InputFile);
    string line;
    while(std::getline(file, line)){
        readInstr(line);
    }
    display();
}

/**
* Display
*/
void Microprocessor::display() const{
    //Display registers
    for(int i=0; i<16;++i){
        cout << "R" << std::hex << i << ": " << std::setw(4) << 
        std::setfill('0') << r[i];

        if(i%4 == 3){ cout << endl; }
        else{ cout << "   "; }
    }
    cout << endl;

    //Display Cache
    cout << "   V M Tag  0  1  2  3  4  5  6  7" << endl;
    cout << "----------------------------------" << endl;
    for(int i=0; i<8; ++i){
        const CacheLine& line = Cache[i];
        cout << std::hex << i << "  ";
        cout << (line.valid ? '1' : '0') << " " << (line.mod ? '1' : '0') << " ";
        cout << std::setw(3) << std::setfill('0') << line.tag << " ";
        for(int j=0; j<8; ++j){
            cout << std::setw(2) << std::setfill('0') << 
                static_cast<int>(line.data[j]);
            if(j < 7) cout << " ";
        }
        cout << endl;
    }
    cout << endl;

    //Display RAM
    for(int i=0; i<128; i+=16){
        cout << std::setw(4) << std::setfill('0') << std::hex << i << ": ";
        for(int j=0; j<16; ++j){
            cout << std::setw(2) << std::setfill('0') << 
                static_cast<int>(RAM[i+j]);
            if(j < 15) cout << " ";
        }
        cout << endl;
    }
    cout << endl;

    
}

/**
* Display Cache
*/
void Microprocessor::displayCache() const{
    cout << "Cache Contents:" << endl;
    cout << "   V M Tag  0  1  2  3  4  5  6  7" << endl;
    for(int i=0; i<8; ++i){
        const CacheLine& line = Cache[i];
        cout << std::hex << i << "  ";
        cout << (line.valid ? '1' : '0') << " " << (line.mod ? '1' : '0') << " ";
        cout << std::setw(3) << std::setfill('0') << line.tag << " ";
        for(int j=0; j<8; ++j){
            cout << std::setw(2) << std::setfill('0') << 
                static_cast<int>(line.data[j]);
            if(j < 7) cout << " ";
        }
        cout << endl;
    }
    cout << endl;
}

int main(int argc, char const *argv[]){
    bool debug = false;
    string inputFile = "";
    string ramFile = "";
    
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " [options] <filename>" << endl;
        return -1;
    }

    //parse CMD line
    int i = 1;
    while(i < argc){
        string arg = argv[i];

        if(arg[0] == '-'){
            if(arg == "-debug"){ //handle debug
                debug = true;
            }
            else if(arg == "-input"){ //handle -input
                if(!(i+1 < argc)){
                    std::cerr << "Usage: " << argv[i] << " <filename>" << endl;
                    return -1;
                }
                inputFile = argv[i+1];
                ++i;
            }
            else if(arg == "-ram"){ //handle -ram
                if(!(i+1 < argc)){
                    std::cerr << "Usage: " << argv[i] << " <filename>" << endl;
                    return -1;
                }
                ramFile = argv[i+1];
                ++i;
            }
            else{
                cerr << "Error: Unknown option: " << arg << endl;
                return -1;
            }
        }
        ++i;
    }

    if(inputFile == ""){
        cerr << "Error: Input file required. Use -input <filename>" << endl;
        return -1;
    }

    //create processor
    if(ramFile == ""){
        Microprocessor ARM(inputFile, debug);
        ARM.Start();
    }
    else{
        Microprocessor ARM(inputFile, ramFile, debug);
        ARM.Start();
    }
    
    return 0;
}