#include <iostream>
#include <thread>
using namespace std;

struct Config {
    int a = 5;
    double b = 3.14;
};

int main() {
    const Config config;

    auto reader = [&]() {
        cout << "a: " << config.a << ", b: " << config.b << endl;
    };

    thread t1(reader);
    thread t2(reader);
    t1.join(); t2.join();

    this_thread::sleep_for(chrono::milliseconds(50)); 
    
    return 0;
}
// No race: multiple threads only read from immutable object
