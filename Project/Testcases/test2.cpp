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
    };

    thread t1(swap_xy);
    thread t2(swap_xy);
    t1.join();
    t2.join();
    cout << "x = " << x << ", y = " << y << endl;
    return 0;
}
// No race: swap is properly locked with deadlock avoidance
