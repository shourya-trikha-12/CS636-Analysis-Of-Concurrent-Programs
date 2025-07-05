#include <iostream>
#include <thread>
using namespace std;

int main() {
    int shared = 100;

    thread writer([&]() {
        shared = 200; // writer

        this_thread::sleep_for(chrono::milliseconds(50));
    });

    thread reader([&]() {
        cout << "Reader sees: " << shared << endl; // reader

        this_thread::sleep_for(chrono::milliseconds(50));
    });

    writer.join(); reader.join();

    cout << "Expected data races on shared: " << &shared << endl;

    return 0;
}
//  Race on variable: shared (one thread writes, other reads without sync)
