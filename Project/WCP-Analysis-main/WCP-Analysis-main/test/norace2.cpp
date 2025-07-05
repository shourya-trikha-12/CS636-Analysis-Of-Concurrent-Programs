#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

int main() {
    int x = 1, y = 2;
    mutex m1, m2;

    auto swap_xy = [&]() {
        lock(m1, m2);
        lock_guard<mutex> lock1(m1, adopt_lock);
        lock_guard<mutex> lock2(m2, adopt_lock);
        int temp = x;
        x = y;
        y = temp;
        // m2.unlock();
        // m1.unlock();
    };

    thread t1(swap_xy);
    thread t2(swap_xy);
    t1.join();
    t2.join();

    this_thread::sleep_for(chrono::milliseconds(50)); 

    cout << "x = " << x << ", y = " << y << endl;
    return 0;
}
// No race: swap is properly locked with deadlock avoidance
