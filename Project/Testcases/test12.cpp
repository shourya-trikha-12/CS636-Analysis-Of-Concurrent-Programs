#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

int main() {
    mutex m1, m2;

    thread t1([&]() {
        lock_guard<mutex> lock1(m1);
        this_thread::sleep_for(chrono::milliseconds(50)); // encourage deadlock
        lock_guard<mutex> lock2(m2);
        cout << "Thread 1 done\n";
    });

    thread t2([&]() {
        lock_guard<mutex> lock2(m2);
        this_thread::sleep_for(chrono::milliseconds(50)); // encourage deadlock
        lock_guard<mutex> lock1(m1);
        cout << "Thread 2 done\n";
    });

    t1.join(); t2.join();
    return 0;
}
//  Deadlock risk: t1 locks m1 then m2, t2 locks m2 then m1
