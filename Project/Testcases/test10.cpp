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
    });

    thread t2([&]() {
        counter++; // no lock
    });

    t1.join(); t2.join();
    cout << "counter = " << counter << endl;
    return 0;
}
//  Race on variable: counter (only one thread uses mutex)
