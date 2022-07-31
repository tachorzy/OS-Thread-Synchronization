    #include <semaphore.h> 
    #include <pthread.h>
    #include <stdio.h>
    #include <unistd.h>
    #include <algorithm>
    #include <iostream>
    #include <vector>
    #include <cmath>
    #include <map>

    struct args { //args for the frequency function
        int* threadID;
        pthread_mutex_t* mutex; //Our mutex object which is a semaphore
        pthread_cond_t* cond; //Our condition variable
        char key;
        int bits, freq, size, val, i;
        std::string msg, decomp_msg, binval; //compressed msg, decompressed message, and binary value
    };

    struct args2 { 
        int* threadID;
        pthread_mutex_t* mutex; //Our mutex object which is a semaphore
        pthread_cond_t* cond; //Our condition variable
        char* keys; //array of keys
        std::string* segments;
        std::string* binvals; //array of binary values
        char key;
        int bits, size, val, i;
        std::string msg, decomp_msg, binval; //compressed msg, decompressed message, and binary value
    };

    std::string bin(int n, int b){ //converts an int (n) to a binary value in the size of b (number of bits)
        std::string s;
        for(int i = b-1; i >= 0; i--){
            int k = n >> i;
            s += (k & 1) ? "1" : "0"; //prob unnecessary to use a ternery operator here but meh I like my code compact
        }
        return s;
    }

    void* frequency(void* void_ptr){ 
        struct args *ptr = (struct args*)void_ptr;
        int freq = 0;
        std::string b = bin(ptr->val, ptr->bits);
        char key = ptr->key;
        int currIndx = ptr->i;
        pthread_mutex_unlock(ptr->mutex); //allows the main thread to continue the for loop

        for(int i = 0; i < ptr->msg.length(); i += ptr->bits){ //finds the frequency of a given char by comparing it's binary value to every 'b' bits of the compressed msg
            if(ptr->msg.substr(i, ptr->bits) == b)
                freq += 1;
        }

        pthread_mutex_lock(ptr->mutex);
        while(currIndx != *ptr->threadID){ //if its not the current thread's turn we make it wait on our conditional variable
            pthread_cond_wait(ptr->cond,ptr->mutex);
        }
        pthread_mutex_unlock(ptr->mutex);

        std::cout << "Character: " << key << ", Code: " << b << ", Frequency: " << freq << std::endl;

        pthread_mutex_lock(ptr->mutex); //locked the mutex, the calling thread will block
        *(ptr->threadID) = *(ptr->threadID)+1;
        pthread_cond_broadcast(ptr->cond); //wakes up all threads that are waiting on cond
        pthread_mutex_unlock(ptr->mutex); //unlocked the mutex
        
        return NULL;
    }

    //decompresses by traversing every 'b' bits and searching for its corresponding key in the arrays of keys and binvals, the key is then concated into the string
    // pass a pointer to the alphabet in main so each thread can read at the same time
    void* decompress(void* void_ptr2){
        struct args2 *ptr = (struct args2*)void_ptr2;
        int currIndx = ptr->i; //childID 
        std::string decomp_msg;
        //std::string* segments = ptr->segments;
        std::string segment = ptr->segments[currIndx];
        std::string* binvals = ptr->binvals;
        char* keys = ptr->keys;
        pthread_mutex_unlock(ptr->mutex);

        for(int i = 0; i < ptr->size; i++)
            if(segment == binvals[i]){
                decomp_msg = keys[i]; //wait for the threads to all do this work before we synchronize THEN WE PRINT
                break;
            }

        pthread_mutex_lock(ptr->mutex);
        while(currIndx !=  *ptr->threadID){ //if its not the current thread's turn we make it wait on our conditional variable
            pthread_cond_wait(ptr->cond,ptr->mutex);
        }
        pthread_mutex_unlock(ptr->mutex);

        ptr->decomp_msg += decomp_msg; //now we're all synchronized so now we're gonna assign this to the decompressed message

        pthread_mutex_lock(ptr->mutex);
        *(ptr->threadID) = *(ptr->threadID)+1;
        pthread_cond_broadcast(ptr->cond);
        pthread_mutex_unlock(ptr->mutex);

        return NULL;
    }

    int main(){
        std::string line, comp_msg;
        
        std::string n;
        getline(std::cin, n);
        const int numOfSymbols = std::stoi(n);
        std::vector<std::pair<char, int>> m; //vector of pairs will act as an unordered_map in order to retain the input order of key-value pairs

        while(getline(std::cin, line)){     
            if(line.length() <= 5){ //when the length is greater than 4 (one key, one space, and up to 2 digits for the val): compressed msg
                std::string s = line.substr(2);
                int val = std::stoi(s);
                m.push_back(std::make_pair(line[0], val));
            }
            else { 
                comp_msg = line;
                break;
            }
        }
        //finding max using max_element, iterates and finds the pair with the highest val
        const auto maxp = std::max_element(m.begin(), m.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        static int numOfBits = std::ceil(std::log2(maxp->second + 1));

        //mooolti-threading for the frequency n threads = numOfSymbols
        pthread_t* tid = new pthread_t[numOfSymbols];
        /////// SORT OUT HOW WE ARE INITIALIZING OUR ARGS, AND HOW WE ARE PASSING IT INTO OUR PTHREAD_CREATE() CALL
        static struct args x;

        x.bits = numOfBits;
        x.msg = comp_msg;
        x.size = numOfSymbols;

        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
        x.mutex = &mutex;
        x.cond = &cond;
        int threadID = 0;
        x.threadID = &threadID; //initialized before being modified by t    hreads
        
        std::cout << "Alphabet:" << std::endl;
        for(int i = 0; i < numOfSymbols; i++){
            pthread_mutex_lock(&mutex);
            
            //shared resources below
            x.i = i;
            x.threadID = &threadID;
            x.key = m.at(i).first;
            x.val = m.at(i).second;
            x.freq = 0;

            if(pthread_create(&tid[i], NULL, frequency, &x)){
                std::cout << "error creating thread:" << stderr << std::endl;
                return 1;
            }
            //pthread_mutex_lock(x.mutex);
        }

        for (int i = 0; i < numOfSymbols; i++)
            pthread_join(tid[i], NULL); 
        
        //pthreads for the DECOMPRESSION
        const int MTHREADS = comp_msg.length() / numOfBits;
        static struct args2 y;
        //filling up y
        y.size = numOfSymbols;
        y.bits = numOfBits;
        y.msg = comp_msg;
        y.keys = new char[numOfSymbols];
        y.binvals = new std::string[numOfSymbols];
        y.segments = new std::string[MTHREADS];
        int j = 0;
        for (int i = 0; i < numOfSymbols; i++) { //filling the dynamic arrays of y
            y.keys[i] = m.at(i).first;
            y.binvals[i] = bin(m.at(i).second, numOfBits);
        }

        mutex = PTHREAD_MUTEX_INITIALIZER;
        cond = PTHREAD_COND_INITIALIZER;
        y.mutex = &mutex;
        y.cond = &cond;

        threadID = 0;
        pthread_t* tid2 = new pthread_t [MTHREADS];
        //std::cout << "\nDecompressed message: ";
        for(int i = 0; i < MTHREADS; i++){
            pthread_mutex_lock(&mutex);

            //shared resources below
            y.i = i;
            y.threadID = &threadID;
            y.segments[i] = comp_msg.substr(j, numOfBits);
            j += numOfBits;

            if(pthread_create(&tid2[i], NULL, decompress, &y)){
                std::cout << "error creating thread:" << stderr << std::endl;
                return 1;
            }
        }

        for (int i = 0; i < MTHREADS; i++)
            pthread_join(tid2[i], NULL);
        
        std::cout << "\nDecompressed message: " << y.decomp_msg<< std::endl;
        
        delete[] tid;
    }