#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

bool globalFlag = false;

int main() {
    mutex m1, m2;

    thread t1([&]() {
        lock_guard<mutex> lock1(m1);
        
        globalFlag = !globalFlag;
        
        this_thread::sleep_for(chrono::milliseconds(50));
    });

    thread t2([&]() {
        lock_guard<mutex> lock2(m2);
        
        globalFlag = !globalFlag;

        this_thread::sleep_for(chrono::milliseconds(50));
    });

    t1.join(); t2.join();

    cout << "Expected data races on globalFlag: " << &globalFlag << endl;
    return 0;
}
//  Race on: globalFlag (each thread locks separate mutex)