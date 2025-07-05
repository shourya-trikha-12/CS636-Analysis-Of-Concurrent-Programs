#include <iostream>
#include <thread>
using namespace std;

int main() {
    thread t1([]() {
        thread_local int local = 1;
        local += 1;
        cout << "T1: " << local << endl;
    });

    thread t2([]() {
        thread_local int local = 2;
        local += 2;
        cout << "T2: " << local << endl;
    });

    t1.join(); t2.join();

    this_thread::sleep_for(chrono::milliseconds(50)); 
    
    return 0;
}
// No race: thread-local variables are isolated per thread
