#include <iostream>
#include <thread>
using namespace std;

int main() {
    int x = 0, y = 0;

    thread t1([&]() {
        for (int i = 0; i < 1000; ++i) x++;

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    thread t2([&]() {
        for (int i = 0; i < 1000; ++i) y--;

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    thread t3([&]() {
        for (int i = 0; i < 1000; ++i) x++;

        this_thread::sleep_for(chrono::milliseconds(50)); 
    });

    t1.join(); t2.join(); t3.join();
    cout << "x = " << x << ", y = " << y << endl;

    std::cout << "Expected data races on x: " << &x << '\n';

    return 0;
}
//  Races on variables: x
