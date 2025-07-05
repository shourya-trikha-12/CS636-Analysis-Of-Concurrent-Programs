#include <iostream>
#include <thread>
#include <mutex>
using namespace std;

int main() {
    int balance = 1000;
    int transactions = 0;
    mutex m;

    auto deposit = [&](int amount) {
        lock_guard<mutex> lock(m);
        balance += amount;
        transactions++;
    };

    auto withdraw = [&](int amount) {
        lock_guard<mutex> lock(m);
        if (balance >= amount) {
            balance -= amount;
            transactions++;
        }
    };

    thread t1(deposit, 500);
    thread t2(withdraw, 300);
    thread t3(deposit, 200);
    thread t4(withdraw, 100);

    t1.join(); t2.join(); t3.join(); t4.join();

    this_thread::sleep_for(chrono::milliseconds(50)); 

    cout << "Final Balance = " << balance << ", Total Transactions = " << transactions << endl;
    return 0;
}
// No race: shared variables balance and transactions are protected by a single mutex
