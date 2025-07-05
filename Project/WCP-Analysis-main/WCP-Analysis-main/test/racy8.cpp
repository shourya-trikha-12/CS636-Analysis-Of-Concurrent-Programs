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
    int a{0}, b{4} ;

    thread t1([&]() {
        p.y = 5;
        p.m.lock();
        p.x = 2;
        p.m.unlock();
    });
    
    thread t2([&]() {
        this_thread::sleep_for(chrono::milliseconds(100)); 
        p.m.lock();
        b = p.y;
        a = p.x;
        p.m.unlock();
    });

    this_thread::sleep_for(chrono::milliseconds(200)); 

    t1.join(); t2.join();


    cout << "Point: (" << p.x << ", " << p.y << "), a: " << a << ", b: " << b << endl;
    cout << "Predictable data race on p.y: " << &p.y << " if critical section of T1 executes before T2" << endl;
    return 0;
}
// Predictable race on p.y if critical section of t1 executes before t2
