#include <iostream>
#include <thread>
using namespace std;

int main() {
    int shared = 100;

    thread writer([&]() {
        shared = 200; // writer
    });

    thread reader([&]() {
        cout << "Reader sees: " << shared << endl; // reader
    });

    writer.join(); reader.join();
    return 0;
}
//  Race on variable: shared (one thread writes, other reads without sync)
