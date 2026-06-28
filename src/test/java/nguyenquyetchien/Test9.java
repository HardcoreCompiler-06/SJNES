package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test9 {

    @ParameterizedTest(name = "n = {0} -> Số Fibonacci mong muốn: {1}")
    @CsvSource({
        "-5, -1",   // Nhánh n < 0: Trả về -1
        "0, 0",     // Nhánh n == 0: Trả về 0 (F0 = 0)
        "1, 1",     // Nhánh n == 1: Trả về 1 (F1 = 1)
        "2, 1",     // Nhánh n > 1: F2 = F1 + F0 = 1 + 0 = 1
        "3, 2",     // F3 = F2 + F1 = 1 + 1 = 2
        "6, 8"      // F6 = 8 (Dãy: 0, 1, 1, 2, 3, 5, 8)
    })
    void testFibonacci(int n, int expected) {
        Advance3 advance = new Advance3();
        assertEquals(expected, advance.fibonacci(n));
    }
}