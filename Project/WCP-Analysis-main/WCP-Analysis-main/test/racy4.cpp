#include <iostream>
#include <thread>
using namespace std;

int main() {
    int shared = 0;

    thread t1([&]() { shared += 1; this_thread::sleep_for(chrono::milliseconds(50)); });
    thread t2([&]() { shared += 2; this_thread::sleep_for(chrono::milliseconds(50)); });

    t1.join(); t2.join();
    cout << "shared = " << shared << endl;

    cout << "Expected data races on shared: " << &shared << endl;
    return 0;
}
// Race on variable: shared (W-W race condition)
