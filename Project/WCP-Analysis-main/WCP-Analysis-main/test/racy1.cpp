#include <iostream>
#include <thread>
using namespace std;

struct Point {
    int x, y;
};

int main() {
    Point p{1, 2};

    thread t1([&]() {
        p.x += 1;
        p.y += 1;

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    thread t2([&]() {
        cout << "x = " << p.x << ", y = " << p.y << endl;

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    t1.join(); t2.join();

    std::cout << "Expected data races on p.x: " << &(p.x) << " and p.y: " << &(p.y) << '\n';
    return 0;
}
//  Races on variables: p.x, p.y
