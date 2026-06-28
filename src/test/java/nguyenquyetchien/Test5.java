package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test5 {

    @ParameterizedTest(name = "n1={0}, n2={1} -> sau sortDesc: n1={2}, n2={3}")
    @CsvSource({
            "8, 3, 8, 3",  // number1 > number2 -> đã đúng thứ tự giảm dần, giữ nguyên
            "3, 8, 8, 3",  // number1 < number2 -> phải đổi chỗ
            "5, 5, 5, 5"   // number1 = number2 -> giữ nguyên
    })
    void testSortDesc(int n1, int n2, int expected1, int expected2) {
        // number1, number2 là static -> phải set lại mỗi lần test để tránh ảnh hưởng giữa các lần chạy
        Sort2.number1 = n1;
        Sort2.number2 = n2;
        Sort2.sortDesc();
        assertEquals(expected1, Sort2.number1);
        assertEquals(expected2, Sort2.number2);
    }
}