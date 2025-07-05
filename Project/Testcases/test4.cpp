#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

struct Point {
    int x, y;
    mutex m;
};

int main() {
    Point p{0, 0};

    thread t1([&]() {
        lock_guard<mutex> lock(p.m);
        p.x += 1;
        p.y += 2;
    });

    thread t2([&]() {
        lock_guard<mutex> lock(p.m);
        p.x += 3;
        p.y += 4;
    });

    t1.join(); t2.join();
    cout << "Point: (" << p.x << ", " << p.y << ")" << endl;
    return 0;
}
// No race: struct's state is guarded by mutex
