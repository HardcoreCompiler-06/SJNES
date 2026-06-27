package nguyenducan;

public class Advance1 {
    public int USCLN(int a, int b) {
        while (a != b) {
            if (a > b) a = a - b;
            else b = b - a;
        }
        return a;
    }

    public int BSCNN(int a, int b) {
        return (a * b) / USCLN(a, b);
    }
}