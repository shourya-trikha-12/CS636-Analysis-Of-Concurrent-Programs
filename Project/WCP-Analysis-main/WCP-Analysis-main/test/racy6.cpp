#include <iostream>
#include <thread>
using namespace std;

bool flag = false;
int dataVal = 0;

void writer() {
    dataVal = 42;  // write
    flag = true; // write (no fence)

    this_thread::sleep_for(chrono::milliseconds(50)); 
}

void reader() {
    if (flag) {
        cout << dataVal << endl; // may see stale value due to reordering
    }
}

int main() {
    thread t1(writer);
    thread t2(reader);
    t1.join(); t2.join();

    cout << "Expected data races on dataVal: " << &dataVal << " (depends on interleaving) and flag: " << &flag << endl;

    return 0;
}
//  Race on: dataVal and flag (missing memory fence / synchronization)
