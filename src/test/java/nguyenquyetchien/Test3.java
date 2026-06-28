package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test3 {

    @ParameterizedTest(name = "n1={0}, n2={1} -> max={2}")
    @CsvSource({
            "10, 5, 10",  // number1 lớn hơn number2
            "5, 10, 10",  // number2 lớn hơn number1
            "7, 7, 7"     // number1 = number2
    })
    void testMax2(int n1, int n2, int expected) {
        MaxNumber2 finder = new MaxNumber2(n1, n2);
        assertEquals(expected, finder.max2());
    }
}