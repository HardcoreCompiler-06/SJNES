package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test4 {

    @ParameterizedTest(name = "n1={0}, n2={1} -> sau sortAsc: n1={2}, n2={3}")
    @CsvSource({
            "5, 2, 2, 5",  // number1 > number2 -> phải đổi chỗ
            "2, 5, 2, 5",  // number1 < number2 -> giữ nguyên
            "4, 4, 4, 4"   // number1 = number2 -> giữ nguyên
    })
    void testSortAsc(int n1, int n2, int expected1, int expected2) {
        Sort1 x = new Sort1();
        x.number1 = n1;
        x.number2 = n2;
        x.sortAsc();
        assertEquals(expected1, x.number1);
        assertEquals(expected2, x.number2);
    }
}