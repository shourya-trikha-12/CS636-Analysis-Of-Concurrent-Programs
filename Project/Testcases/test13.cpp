#include <iostream>
#include <thread>
using namespace std;

bool flag = false;
int data = 0;

void writer() {
    data = 42;  // write
    flag = true; // write (no fence)
}

void reader() {
    if (flag) {
        cout << data << endl; // may see stale value due to reordering
    }
}

int main() {
    thread t1(writer);
    thread t2(reader);
    t1.join(); t2.join();
    return 0;
}
//  Race on: data and flag (missing memory fence / synchronization)
