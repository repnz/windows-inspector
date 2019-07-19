#include <iostream>
#include <iomanip>

using namespace std;

int main() {




	cout << "[Hello]" << endl;
	cout << setfill('-') << setbase(16) << setw(10) << left << 200 << dec << setw(20) << 250 << endl;

	cout << "[Bye]" << endl;
	return 0;
}