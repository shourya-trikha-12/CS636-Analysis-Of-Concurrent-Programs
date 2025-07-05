#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

int main() {
    int counter = 0;
    mutex m;

    thread t1([&]() {
        lock_guard<mutex> lock(m);
        counter++;

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    thread t2([&]() {
        counter++; // no lock

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    t1.join(); t2.join();
    cout << "counter = " << counter << endl;
    std::cout << "Expected data races on counter: " << &counter << '\n';
    return 0;
}
//  Race on variable: counter (only one thread uses mutex)
