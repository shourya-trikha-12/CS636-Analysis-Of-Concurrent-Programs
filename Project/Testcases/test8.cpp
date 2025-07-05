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
    });

    thread t2([&]() {
        cout << "x = " << p.x << ", y = " << p.y << endl;
    });

    t1.join(); t2.join();
    return 0;
}
//  Races on variables: p.x, p.y
