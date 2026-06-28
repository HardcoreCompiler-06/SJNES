package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test8 {

    @ParameterizedTest(name = "number = {0} -> Tổng các chữ số mong muốn: {1}")
    @CsvSource({
        "5765, 23",   // Trường hợp thông thường giống ví dụ: 5+7+6+5 = 23
        "0, 0",       // Số 0 (vòng lặp while không chạy)
        "9, 9",       // Số có 1 chữ số
        "11111, 5",   // Các chữ số giống nhau
        "-123, -6"    // Trường hợp số âm: -123 % 10 = -3 -> sum = (-3) + (-2) + (-1) = -6
    })
    void testSumOfDigits(long number, int expected) {
        Advance2 advance = new Advance2();
        assertEquals(expected, advance.sum(number));
    }
}