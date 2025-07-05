#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

int main() {
    int a = 0, b = 0;
    mutex m1, m2;

    thread t1([&]() {
        lock_guard<mutex> lock(m1);
        a += 1;
    });

    thread t2([&]() {
        lock_guard<mutex> lock(m2);
        b += 1;
    });

    thread t3([&]() {
        lock_guard<mutex> lock(m1);
        a += 2;
    });

    thread t4([&]() {
        lock_guard<mutex> lock(m2);
        b += 2;
    });

    t1.join(); t2.join(); t3.join(); t4.join();
    cout << "a = " << a << ", b = " << b << endl;
    return 0;
}
// No races: a and b protected by separate mutexes
