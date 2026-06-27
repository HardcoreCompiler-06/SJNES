package nguyenducan;

public class Advance4 {
    public boolean isPrimeNumber(int n) {
        if (n < 2) return false;
        int squareRoot = (int) Math.sqrt(n);
        for (int i = 2; i <= squareRoot; i++) {
            if (n % i == 0) return false;
        }
        return true;
    }
}