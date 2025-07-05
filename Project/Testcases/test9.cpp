#include <iostream>
#include <thread>
using namespace std;

int main() {
    int x = 0, y = 0;

    thread t1([&]() {
        for (int i = 0; i < 1000; ++i) x++;
    });

    thread t2([&]() {
        for (int i = 0; i < 1000; ++i) y--;
    });

    thread t3([&]() {
        for (int i = 0; i < 1000; ++i) x++;
    });

    t1.join(); t2.join(); t3.join();
    cout << "x = " << x << ", y = " << y << endl;
    return 0;
}
//  Races on variables: x
