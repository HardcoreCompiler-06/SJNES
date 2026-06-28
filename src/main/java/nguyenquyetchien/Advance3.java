package nguyenquyetchien;

public class Advance3 {
 public int fibonacci(int n) {
 if (n < 0) {
 return -1;
 } else if (n == 0 || n == 1) {
 return n;
 } else {
 return fibonacci(n - 1) + fibonacci(n - 2);
 }
 }
}

