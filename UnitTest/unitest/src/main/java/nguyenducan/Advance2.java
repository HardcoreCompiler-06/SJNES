package nguyenducan;

public class Advance2 {
    public int sum(long number) {
        int sum = 0;
        long index;
        while (number != 0) {
            index = number % 10;
            sum += index;
            number /= 10;
        }
        return sum;
    }
}