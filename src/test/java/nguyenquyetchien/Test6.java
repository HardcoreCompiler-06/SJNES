package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test6 {

    @ParameterizedTest(name = "n1={0}, n2={1}, n3={2} -> maxLength: {3}")
    @CsvSource({
        "10, 5, 3, 10",   // number1 lớn nhất (nhánh 1)
        "4, 12, 8, 12",   // number2 lớn nhất (nhánh 2)
        "8, 6, 15, 15",   // number3 lớn nhất từ nhánh 1 (8 >= 6 nhưng 8 < 15)
        "3, 7, 9, 9",     // number3 lớn nhất từ nhánh 2 (3 < 7 nhưng 7 < 9)
        "7, 7, 7, 7"      // Cả 3 số bằng nhau
    })
    void testMaxLength(int n1, int n2, int n3, int expected) {
        Triangle triangle = new Triangle();
        triangle.number1 = n1;
        triangle.number2 = n2;
        triangle.number3 = n3;
        
        assertEquals(expected, triangle.maxLength());
    }
}