package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test2 {

    @ParameterizedTest(name = "n1={0}, n2={1}, n3={2} -> max={3}")
    @CsvSource({
            "10, 5, 3, 10",   // number1 lớn nhất
            "5, 10, 3, 10",   // number2 lớn nhất
            "5, 3, 10, 10",   // number3 lớn nhất
            "10, 10, 5, 10",  // number1 = number2
            "5, 10, 10, 10",  // number2 = number3
            "10, 5, 10, 10",  // number1 = number3
            "7, 7, 7, 7"      // cả ba bằng nhau
    })
    void testMax3(int n1, int n2, int n3, int expected) {
        MaxNumber1 finder = new MaxNumber1();
        finder.setNumber1(n1);
        finder.setNumber2(n2);
        finder.setNumber3(n3);
        assertEquals(expected, finder.max3());
    }
}
