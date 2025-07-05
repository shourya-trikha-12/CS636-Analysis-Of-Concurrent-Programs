#include <iostream>
#include <thread>
#include <atomic>
using namespace std;

int main() {
    atomic<int> counter1{0}, counter2{0};

    thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {counter1++; counter2--;}
    });

    thread t2([&]() {
        for (int i = 0; i < 1000; ++i) {counter2++; counter1--;}
    });

    t1.join(); t2.join();

    this_thread::sleep_for(chrono::milliseconds(50)); 

    cout << "counter1 = " << counter1 << ", counter2 = " << counter2 << endl;
    return 0;
}
// No race: all operations on atomic variables
