// Small leaky test program

void foo() {
    int *x = new int;
}

int main() {
    int *z = new int[10];
    char *q = new char[4];
    q[4] = 'x';                 // MAGIC overrun
    // Commenting out should make this abort
    // delete q;
    foo();
    foo();
    delete z;
    delete z;   // delete value twice
}
